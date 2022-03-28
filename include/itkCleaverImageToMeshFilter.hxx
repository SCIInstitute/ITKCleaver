/*=========================================================================
 *
 *  Copyright NumFOCUS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#ifndef itkCleaverImageToMeshFilter_hxx
#define itkCleaverImageToMeshFilter_hxx

#include "itkCleaverImageToMeshFilter.h"

#include "itkImageRegionIterator.h"
#include "itkImageRegionConstIterator.h"
#include "itkImage.h"
#include "itkMinimumMaximumImageCalculator.h"
#include "itkThresholdImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkDiscreteGaussianImageFilter.h"
#include "itkMultiplyImageFilter.h"
#include "itkSubtractImageFilter.h"
#include "itkApproximateSignedDistanceMapImageFilter.h"

#include "itkTetrahedronCell.h"
#include "itkTriangleCell.h"

#include "cleaver/InverseField.h"
#include "cleaver/Cleaver.h"
#include "cleaver/CleaverMesher.h"
#include "cleaver/SizingFieldCreator.h"

#include <sstream>
#include <cmath>

namespace
{

template<typename TImage>
bool checkImageSize(TImage * inputImg, double sigma)
{
  auto dims = inputImg->GetLargestPossibleRegion().GetSize();
  auto spacing = inputImg->GetSpacing();
  std::vector<double> imageSize{ dims[0] * spacing[0], dims[1] * spacing[1], dims[2] * spacing[2] };
  double imageSizeMin = *(std::min_element(std::begin(imageSize), std::end(imageSize)));

  return (sigma / imageSizeMin) >= 0.1;
}

template<typename TImage>
std::vector<cleaver::AbstractScalarField*>
segmentationToIndicatorFunctions(const TImage * image, double sigma) {
  using ImageType = TImage;
  using FloatImageType = itk::Image<float, ImageType::ImageDimension>;
  using CasterType = itk::CastImageFilter<ImageType, FloatImageType>;

  auto caster = CasterType::New();
  caster->SetInput(image);
  caster->Update();
  
  //determine the number of labels in the segmentations
  using ImageCalculatorFilterType = itk::MinimumMaximumImageCalculator<FloatImageType>;
  auto imageCalculatorFilter
    = ImageCalculatorFilterType::New();
  imageCalculatorFilter->SetImage(caster->GetOutput());
  imageCalculatorFilter->Compute();
  auto maxLabel = static_cast<size_t>(imageCalculatorFilter->GetMaximum());
  auto minLabel = static_cast<size_t>(imageCalculatorFilter->GetMinimum());
  std::vector<cleaver::AbstractScalarField*> fields;

  // extract images from each label for an indicator function
  for (size_t i = minLabel, num = 0; i <= maxLabel; i++, num++) {
    // Pull out this label
    using ThreshType = itk::ThresholdImageFilter<FloatImageType>;
    auto thresh = ThreshType::New();
    thresh->SetInput(caster->GetOutput());
    thresh->SetOutsideValue(0);
    thresh->ThresholdOutside(static_cast<double>(i) - 0.001,
      static_cast<double>(i) + 0.001);
    thresh->Update();

    // Change the values to be from 0 to 1.
    using MultiplyImageFilterType = itk::MultiplyImageFilter<FloatImageType, FloatImageType, FloatImageType>;
    auto multiplyImageFilter =
      MultiplyImageFilterType::New();
    multiplyImageFilter->SetInput(thresh->GetOutput());
    multiplyImageFilter->SetConstant(1. / static_cast<double>(i));
    multiplyImageFilter->Update();

    auto inputImg = multiplyImageFilter->GetOutput();
    bool warning = checkImageSize(inputImg, sigma);

    // Do some blurring.
    using GaussianBlurType = itk::DiscreteGaussianImageFilter<FloatImageType, FloatImageType>;
    auto blur = GaussianBlurType::New();
    blur->SetInput(multiplyImageFilter->GetOutput());
    blur->SetVariance(sigma * sigma);
    blur->Update();

    //find the average value between
    auto calc =
      ImageCalculatorFilterType::New();
    calc->SetImage(blur->GetOutput());
    calc->Compute();
    float mx = calc->GetMaximum();
    float mn = calc->GetMinimum();
    auto md = (mx + mn) / 2.f;

    //create a distance map with that minimum value as the levelset
    using DMapType = itk::ApproximateSignedDistanceMapImageFilter<FloatImageType, FloatImageType>;
    auto dm = DMapType::New();
    dm->SetInput(blur->GetOutput());
    dm->SetInsideValue(md + 0.1f);
    dm->SetOutsideValue(md -0.1f);
    dm->Update();

    // Convert the image to a cleaver "abstract field".
    auto img = dm->GetOutput();
    auto region = img->GetLargestPossibleRegion();
    auto numPixel = region.GetNumberOfPixels();
    float *data = new float[numPixel];
    auto x = region.GetSize()[0], y = region.GetSize()[1], z = region.GetSize()[2];
    fields.push_back(new cleaver::FloatField(data, x, y, z));
    std::string name("SegmentationLabel");
    std::stringstream ss;
    ss << name << i;
    fields[num]->setName(ss.str());
    fields[num]->setWarning(warning);
    itk::ImageRegionConstIterator<FloatImageType> imageIterator(img, region);
    size_t pixel = 0;
    float min = static_cast<float>(imageIterator.Get());
    float max = static_cast<float>(imageIterator.Get());
    auto spacing = img->GetSpacing();
    std::string error = "none";
    while (!imageIterator.IsAtEnd()) {
      // Get the value of the current pixel.
      float val = static_cast<float>(imageIterator.Get());
      ((cleaver::FloatField*)fields[num])->data()[pixel++] = -val;
      ++imageIterator;

      //Error checking
      if (std::isnan(val) && error.compare("none") == 0)
      {
        error = "nan";
      }
      else if (val < min)
      {
        min = val;
      }
      else if (val > max)
      {
        max = val;
      }
    }

    if ((min >= 0 || max <= 0) && (error.compare("none") == 0))
    {
      error = "maxmin";
    }

    fields[num]->setError(error);
    ((cleaver::FloatField*)fields[num])->setScale(
      cleaver::vec3(spacing[0], spacing[1], spacing[2]));
  }
  return fields;
}

template<typename TImage>
std::vector<cleaver::AbstractScalarField*>
imagesToCleaverFloatFields(std::vector<const TImage *> images, double sigma)
{
  std::vector<cleaver::AbstractScalarField*> fields;
  size_t num = 0;
  for (auto image : images) 
  {
    using ImageType = TImage;
    using FloatImageType = itk::Image<float, ImageType::ImageDimension>;
    using CasterType = itk::CastImageFilter<ImageType, FloatImageType>;

    auto caster = CasterType::New();
    caster->SetInput(image);
    caster->Update();

    //Checking sigma vs the size of the image
    auto inputImg = caster->GetOutput();
    bool warning = checkImageSize(inputImg, sigma);

    //do some blurring
    using GaussianBlurType = itk::DiscreteGaussianImageFilter<FloatImageType, FloatImageType>;
    auto blur = GaussianBlurType::New();
    blur->SetInput(caster->GetOutput());
    blur->SetVariance(sigma * sigma);
    blur->Update();
    auto img = blur->GetOutput();
    //convert the image to a cleaver "abstract field"
    auto region = img->GetLargestPossibleRegion();
    auto numPixel = region.GetNumberOfPixels();
    float *data = new float[numPixel];
    auto x = region.GetSize()[0], y = region.GetSize()[1], z = region.GetSize()[2];
    fields.push_back(new cleaver::FloatField(data, x, y, z));
    std::string name("SegmentationLabel");
    fields[num]->setName(name);
    fields[num]->setWarning(warning);
    itk::ImageRegionConstIterator<FloatImageType> imageIterator(img, region);
    size_t pixel = 0;
    auto spacing = img->GetSpacing();
    float min = static_cast<float>(imageIterator.Get());
    float max = static_cast<float>(imageIterator.Get());
    std::string error = "none";
    while (!imageIterator.IsAtEnd()) {
      // Get the value of the current pixel
      float val = static_cast<float>(imageIterator.Get());
      ((cleaver::FloatField*)fields[num])->data()[pixel++] = val;
      ++imageIterator;

      //Error checking
      if (std::isnan(val) && error.compare("none") == 0)
      {
        error = "nan";
      }
      else if (val < min)
      {
        min = val;
      }
      else if (val > max)
      {
        max = val;
      }
    }

    if ((min >= 0 || max <= 0) && (error.compare("none") == 0))
    {
      error = "maxmin";
    }

    fields[num]->setError(error);
    ((cleaver::FloatField*)fields[num])->setScale(cleaver::vec3(spacing[0], spacing[1], spacing[2]));
    num++;
  }
  return fields;
}

template<typename TImage>
void
cleaverFloatFieldToImage(const cleaver::FloatField *field, TImage * image)
{
  using ImageType = TImage;
  auto dims = field->dataBounds().size;
  typename ImageType::IndexType start;
  start.Fill(0);
  typename ImageType::SizeType size;
  size[0] = static_cast<size_t>(dims[0]);
  size[1] = static_cast<size_t>(dims[1]);
  size[2] = static_cast<size_t>(dims[2]);
  typename ImageType::RegionType region(start, size);
  image->SetRegions(region);
  image->Allocate();
  image->FillBuffer(0);
  auto data = ((cleaver::FloatField*)field)->data();
  for (size_t i = 0; i < dims[0]; i++) {
    for (size_t j = 0; j < dims[1]; j++) {
      for (size_t k = 0; k < dims[2]; k++) {
        typename ImageType::IndexType pixelIndex;
        pixelIndex[0] = i;
        pixelIndex[1] = j;
        pixelIndex[2] = k;
        image->SetPixel(pixelIndex, static_cast<typename ImageType::PixelType>(data[i + size[0] * j + size[0] * size[1] * k]));
      }
    }
  }
}

} // end anonymous namespace

namespace itk
{

template <typename TInputImage, typename TOutputMesh>
CleaverImageToMeshFilter<TInputImage, TOutputMesh>
::CleaverImageToMeshFilter()
{
  this->ProcessObject::SetNumberOfRequiredInputs(1);
  this->ProcessObject::SetNumberOfRequiredOutputs(2);

  typename OutputMeshType::Pointer output = dynamic_cast<OutputMeshType *>(this->MakeOutput(1).GetPointer());
  this->ProcessObject::SetNthOutput(1, output.GetPointer());
}


template <typename TInputImage, typename TOutputMesh>
void
CleaverImageToMeshFilter<TInputImage, TOutputMesh>
::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);

  os << indent << "InputIsIndicatorFunction: ";
  if (this->m_InputIsIndicatorFunction)
    {
    os << "true";
    }
    else
    {
    os << "false";
    }
  os << std::endl;
  os << indent << "SamplingRate: " << this->m_SamplingRate << std::endl;
  os << indent << "Lipschitz: " << this->m_Lipschitz << std::endl;
  os << indent << "FeatureScaling: " << this->m_FeatureScaling << std::endl;
  os << indent << "Padding: " << this->m_Padding << std::endl;
  os << indent << "Alpha: " << this->m_Alpha << std::endl;
}


template <typename TInputImage, typename TOutputMesh>
void
CleaverImageToMeshFilter<TInputImage, TOutputMesh>
::GenerateData()
{
  std::vector<cleaver::AbstractScalarField*> fields;

  const bool segmentation = this->GetNumberOfIndexedInputs() < 2 && !this->GetInputIsIndicatorFunction();

  if (segmentation)
  {
    const InputImageType * input = this->GetInput();
    fields = segmentationToIndicatorFunctions(input, m_Sigma);
  }
  else
  {
    std::vector<const InputImageType *> inputImages;
    for (unsigned int ii = 0; ii < this->GetNumberOfIndexedInputs(); ii++)
    {
      inputImages.push_back(this->GetInput(ii));
    }
    fields = imagesToCleaverFloatFields(inputImages, m_Sigma);

    if (this->GetNumberOfIndexedInputs() == 1)
    {
      fields.push_back(new cleaver::InverseScalarField(fields[0]));
      fields.back()->setName(fields[0]->name() + "-inverse");
    }
  }

  //Error checking for indicator function values.
  for (int i = 0; i < fields.size(); i++)
  {
    //Skip if the segmentation value is 0 (background) or it is an inverse file
    std::size_t found = fields[i]->name().find("inverse");
    if ((segmentation && i == 0) || (found != std::string::npos))
    {
      continue;
    }
    //Check for critical errors
    auto error = ((cleaver::ScalarField<float>*)fields[i])->getError();
    if (error.compare("nan") == 0 || error.compare("maxmin") == 0)
    {
      itkExceptionMacro("No zero crossing in indicator function. Not a valid file or need a lower sigma value.");
    }
    //Check for warning
    auto warning = ((cleaver::ScalarField<float>*)fields[i])->getWarning();
    if (warning)
    {
      itkWarningMacro("Nrrd file read WARNING: Sigma is 10% of volume's size. Gaussian kernel may be truncated.");
    }
  }

  std::unique_ptr<cleaver::Volume> volume(new cleaver::Volume(fields));

  // Simple interface approximation
  const bool simple = false;
  cleaver::CleaverMesher mesher(simple);
  mesher.setVolume(volume.get());
  mesher.setAlphaInit(m_Alpha);

  // Other option: cleaver::Constant
  const cleaver::MeshType elementSizingElement = cleaver::Adaptive;
  const bool verbose = false;

  std::vector<cleaver::AbstractScalarField *> sizingField;
  sizingField.push_back(cleaver::SizingFieldCreator::createSizingFieldFromVolume(
    volume.get(),
    (float)(1.0 / this->m_Lipschitz),
    (float)this->m_SamplingRate,
    (float)this->m_FeatureScaling,
    (int)this->m_Padding,
    (elementSizingElement != cleaver::Constant),
    verbose));

  volume->setSizingField(sizingField[0]);

  mesher.setConstant(false);
  cleaver::TetMesh * bgMesh = mesher.createBackgroundMesh(verbose);

  // Apply Mesh Cleaving
  mesher.buildAdjacency(verbose);
  mesher.sampleVolume(verbose);
  mesher.computeAlphas(verbose);
  mesher.computeInterfaces(verbose);
  mesher.generalizeTets(verbose);
  mesher.snapsAndWarp(verbose);
  mesher.stencilTets(verbose);

  cleaver::TetMesh *mesh = mesher.getTetMesh();

  // Strip Exterior Tets
  cleaver::stripExteriorTets(mesh, volume.get(), verbose);

  // Fix jacobians if requested.
  mesh->fixVertexWindup(verbose);

  // mesh->writePly("/tmp/out.ply");

  // Compute Quality If Havn't Already
  // mesh->computeAngles();
  // std::cout << "Min Dihedral: " << mesh->min_angle << std::endl;
  // std::cout << "Max Dihedral: " << mesh->max_angle << std::endl;

  //         Create Pruned Vertex List
  class vec3_compare {
    public:
      bool operator()(const cleaver::vec3 &a, const cleaver::vec3 &b) const {
        if ((a.x < b.x) && (b.x - a.x) > 1e-9) return true;
        else if ((a.x > b.x) && (a.x - b.x) > 1e-9) return false;
        if ((a.y < b.y) && (b.y - a.y) > 1e-9) return true;
        else if ((a.y > b.y) && (a.y - b.y) > 1e-9) return false;
        if ((a.z < b.z) && (b.z - a.z) > 1e-9) return true;
        else if ((a.z > b.z) && (a.z - b.z) > 1e-9) return false;
        return false;
      }
  };

  using VertMap = std::map< const cleaver::vec3, size_t, vec3_compare >;
  VertMap vertMap;
  std::vector<cleaver::vec3> prunedVerts;
  size_t prunedPos = 0;
  for(size_t t=0; t < mesh->tets.size(); t++) {
    cleaver::Tet* tet = mesh->tets[t];

    cleaver::Vertex *v1 = tet->verts[0];
    cleaver::Vertex *v2 = tet->verts[1];
    cleaver::Vertex *v3 = tet->verts[2];
    cleaver::Vertex *v4 = tet->verts[3];

    cleaver::vec3 p1 = v1->pos();
    cleaver::vec3 p2 = v2->pos();
    cleaver::vec3 p3 = v3->pos();
    cleaver::vec3 p4 = v4->pos();

    if (!vertMap.count(p1)) {
      vertMap.insert(std::pair<cleaver::vec3,size_t>(p1,prunedPos));
      prunedPos++;
      prunedVerts.push_back(p1);
    }
    if (!vertMap.count(p2)) {
      vertMap.insert(std::pair<cleaver::vec3,size_t>(p2,prunedPos));
      prunedPos++;
      prunedVerts.push_back(p2);
    }
    if (!vertMap.count(p3)) {
      vertMap.insert(std::pair<cleaver::vec3,size_t>(p3,prunedPos));
      prunedPos++;
      prunedVerts.push_back(p3);
    }
    if (!vertMap.count(p4)) {
      vertMap.insert(std::pair<cleaver::vec3,size_t>(p4,prunedPos));
      prunedPos++;
      prunedVerts.push_back(p4);
    }
  }

  OutputMeshType * tetOutput = this->GetOutput(0); 
  using CellType = typename OutputMeshType::CellType;

  for (size_t ii = 0; ii < prunedVerts.size(); ii++)
  {
    typename OutputMeshType::PointType point;
    point[0] = prunedVerts[ii].x;
    point[1] = prunedVerts[ii].y;
    point[2] = prunedVerts[ii].z;
    tetOutput->SetPoint(ii, point);
  }

  using CellDataContainerType = typename OutputMeshType::CellDataContainer;
  auto outputCellData = CellDataContainerType::New();
  outputCellData->Reserve(mesh->tets.size());
  for(size_t ii = 0; ii < mesh->tets.size(); ii++)
  {
    const cleaver::Tet* t = mesh->tets.at(ii);

    cleaver::Vertex* v1 = t->verts[0];
    cleaver::Vertex* v2 = t->verts[1];
    cleaver::Vertex* v3 = t->verts[2];
    cleaver::Vertex* v4 = t->verts[3];
    const size_t i1 = vertMap.find(v1->pos())->second;
    const size_t i2 = vertMap.find(v2->pos())->second;
    const size_t i3 = vertMap.find(v3->pos())->second;
    const size_t i4 = vertMap.find(v4->pos())->second;

    typename CellType::CellAutoPointer cell;
    using TetrahedronCellType = TetrahedronCell<CellType>;
    cell.TakeOwnership(new TetrahedronCellType);

    cell->SetPointId(0, i1);
    cell->SetPointId(1, i2);
    cell->SetPointId(2, i3);
    cell->SetPointId(3, i4);

    tetOutput->SetCell(ii, cell);

    outputCellData->SetElement(ii, t->mat_label);
  }
  tetOutput->SetCellData(outputCellData);


  OutputMeshType * triangleOutput = this->GetOutput(1); 

  std::vector<size_t> interfaces;
  std::vector<size_t> triangleCellData;
  std::vector<size_t> keys;

  // determine output faces and vertices vertex counts
  for(size_t f = 0; f < mesh->faces.size(); f++)
  {
    const int t1Index = static_cast<int>(mesh->faces[f]->tets[0]);
    const int t2Index = static_cast<int>(mesh->faces[f]->tets[1]);

    if(t1Index < 0 || t2Index < 0){
      continue;
    }

    const cleaver::Tet *t1 = mesh->tets[t1Index];
    const cleaver::Tet *t2 = mesh->tets[t2Index];

    if(t1->mat_label != t2->mat_label)
    {
      interfaces.push_back(f);

      const int triangleCellDataKey = (1 << (int)t1->mat_label) + (1 << (int)t2->mat_label);
      int cellDataId = -1;
      for(size_t k = 0; k < keys.size(); k++)
      {
        if(keys[k] == triangleCellDataKey)
        {
          cellDataId = static_cast<int>(k);
          break;
        }
      }
      if(cellDataId == -1)
      {
        keys.push_back(triangleCellDataKey);
        cellDataId = static_cast<int>(keys.size() - 1);
      }

      triangleCellData.push_back(cellDataId);
    }
  }

  //         Create Pruned Vertex List
  VertMap triangleVertMap;
  std::vector<cleaver::vec3> trianglePrunedVerts;
  prunedPos = 0;
  for(size_t f = 0; f < interfaces.size(); f++)
  {
    cleaver::Face *face = mesh->faces[interfaces[f]];

    cleaver::Vertex *v1 = mesh->verts[face->verts[0]];
    cleaver::Vertex *v2 = mesh->verts[face->verts[1]];
    cleaver::Vertex *v3 = mesh->verts[face->verts[2]];

    cleaver::vec3 p1 = v1->pos();
    cleaver::vec3 p2 = v2->pos();
    cleaver::vec3 p3 = v3->pos();

    if (!triangleVertMap.count(p1)) {
      triangleVertMap.insert(std::pair<cleaver::vec3,size_t>(p1,prunedPos));
      prunedPos++;
      trianglePrunedVerts.push_back(p1);
    }
    if (!triangleVertMap.count(p2)) {
      triangleVertMap.insert(std::pair<cleaver::vec3,size_t>(p2,prunedPos));
      prunedPos++;
      trianglePrunedVerts.push_back(p2);
    }
    if (!triangleVertMap.count(p3)) {
      triangleVertMap.insert(std::pair<cleaver::vec3,size_t>(p3,prunedPos));
      prunedPos++;
      trianglePrunedVerts.push_back(p3);
    }
  }

  for (size_t ii = 0; ii < trianglePrunedVerts.size(); ii++)
    {
      typename OutputMeshType::PointType point;
      point[0] = trianglePrunedVerts[ii].x;
      point[1] = trianglePrunedVerts[ii].y;
      point[2] = trianglePrunedVerts[ii].z;
      triangleOutput->SetPoint(ii, point);
    }

  auto triangleOutputCellData = CellDataContainerType::New();
  triangleOutputCellData->Reserve(interfaces.size());
  for(size_t f = 0; f < interfaces.size(); f++)
  {
    cleaver::Face *face = mesh->faces[interfaces[f]];

    cleaver::Vertex *v1 = mesh->verts[face->verts[0]];
    cleaver::Vertex *v2 = mesh->verts[face->verts[1]];
    cleaver::Vertex *v3 = mesh->verts[face->verts[2]];
    size_t i1 = triangleVertMap.find(v1->pos())->second;
    size_t i2 = triangleVertMap.find(v2->pos())->second;
    size_t i3 = triangleVertMap.find(v3->pos())->second;

    typename CellType::CellAutoPointer cell;
    using TriangleCellType = TriangleCell<CellType>;
    cell.TakeOwnership(new TriangleCellType);

    cell->SetPointId(0, i1);
    cell->SetPointId(1, i2);
    cell->SetPointId(2, i3);

    triangleOutput->SetCell(f, cell);

    triangleOutputCellData->SetElement(f, triangleCellData[f]);
  }
  triangleOutput->SetCellData(triangleOutputCellData);

  for (auto field: fields)
  {
    delete field;
  }
}

} // end namespace itk

#endif // itkCleaverImageToMeshFilter_hxx

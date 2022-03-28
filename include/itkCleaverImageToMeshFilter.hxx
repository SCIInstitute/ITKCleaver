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

#include "itkTetrahedronCell.h"

#include "cleaver/InverseField.h"
#include "cleaver/Cleaver.h"
#include "cleaver/CleaverMesher.h"
#include "cleaver/SizingFieldCreator.h"

#include "itkCleaverUtils.h"

namespace itk
{

template <typename TInputImage, typename TOutputMesh>
CleaverImageToMeshFilter<TInputImage, TOutputMesh>
::CleaverImageToMeshFilter()
{
  this->SetNumberOfRequiredInputs(1);
  this->SetNumberOfRequiredOutputs(1);
}


template <typename TInputImage, typename TOutputMesh>
void
CleaverImageToMeshFilter<TInputImage, TOutputMesh>
::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);

  os << indent << "InputIsIndicatorFunction: ";
  os << this->m_InputIsIndicatorFunction ? "true" : "false";
  os << std::endl;
  os << indent << "SamplingRate: " << this->m_SamplingRate << std::endl;
  os << indent << "Lipschitz: " << this->m_Lipschitz << std::endl;
  os << indent << "FeatureScaling: " << this->m_FeatureScaling << std::endl;
  os << indent << "Padding: " << this->m_Padding << std::endl;
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

  // Compute Quality If Havn't Already
  mesh->computeAngles();
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

  OutputMeshType * output = this->GetOutput(); 
  for (size_t ii = 0; ii < prunedVerts.size(); ii++)
  {
    typename OutputMeshType::PointType point;
    point[0] = prunedVerts[ii].x;
    point[1] = prunedVerts[ii].y;
    point[2] = prunedVerts[ii].z;
    output->SetPoint(ii, point);
  }

  IdentifierType cellId = 0;

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

    using CellType = typename OutputMeshType::CellType;
    typename CellType::CellAutoPointer cell;
    using TetrahedronCellType = TetrahedronCell<CellType>;
    cell.TakeOwnership(new TetrahedronCellType);

    cell->SetPointId(0, i1);
    cell->SetPointId(1, i2);
    cell->SetPointId(2, i3);
    cell->SetPointId(3, i4);

    output->SetCell(cellId, cell);
    cellId++;
  }

  using CellDataContainerType = typename OutputMeshType::CellDataContainer;
  auto outputCellData = CellDataContainerType::New();
  outputCellData->Reserve(mesh->tets.size());
  for(size_t ii = 0; ii < mesh->tets.size(); ii++)
  {
    cleaver::Tet* t = mesh->tets.at(ii);
    outputCellData->SetElement(ii, t->mat_label);
  }
  output->SetCellData(outputCellData);

  // using TriangleCellType = itk::TriangleCell<CellInterfaceType>;

  for (auto field: fields)
  {
    delete field;
  }
}

} // end namespace itk

#endif // itkCleaverImageToMeshFilter_hxx

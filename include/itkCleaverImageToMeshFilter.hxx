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

#include "itkMinimumMaximumImageCalculator.h"
#include "itkThresholdImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkDiscreteGaussianImageFilter.h"
#include "itkMultiplyImageFilter.h"
#include "itkSubtractImageFilter.h"
#include "itkApproximateSignedDistanceMapImageFilter.h"
#include <sstream>
#include <cmath>

#include "cleaver/InverseField.h"
#include "cleaver/Cleaver.h"
#include "cleaver/CleaverMesher.h"

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
loadImages(std::vector<const TImage *> images, double sigma)
{
  std::vector<cleaver::AbstractScalarField*> fields;
  size_t num = 0;
  for (auto image : images) {
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

} // end anonymous namespace

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
    fields = loadImages(inputImages, m_Sigma);

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

  //OutputMeshType *      output = this->GetOutput();
  //using InputRegionType = typename InputImageType::RegionType;
  //InputRegionType inputRegion = InputRegionType(outputRegion.GetSize());

  //itk::ImageRegionConstIterator<InputImageType> in(input, inputRegion);
  //itk::ImageRegionIterator<OutputMeshType>     out(output, outputRegion);

  //for (in.GoToBegin(), out.GoToBegin(); !in.IsAtEnd() && !out.IsAtEnd(); ++in, ++out)
  //{
    //out.Set(in.Get());
  //}
}

} // end namespace itk

#endif // itkCleaverImageToMeshFilter_hxx

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
#ifndef itkCleaverUtils_h
#define itkCleaverUtils_h

#include "cleaver/Cleaver.h"

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

namespace itk
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

} // end namespace itk

#endif // itkCleaverUtils_h

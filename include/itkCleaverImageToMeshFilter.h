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
#ifndef itkCleaverImageToMeshFilter_h
#define itkCleaverImageToMeshFilter_h

#include "itkImageToMeshFilter.h"

namespace itk
{

/** \class CleaverImageToMeshFilter
 *
 * \brief Filters a image by iterating over its pixels.
 *
 * Filters a image by iterating over its pixels in a multi-threaded way
 * and {to be completed by the developer}.
 *
 * \ingroup Cleaver
 *
 */
template <typename TInputImage, typename TOutputMesh>
class CleaverImageToMeshFilter : public ImageToMeshFilter<TInputImage, TOutputMesh>
{
public:
  ITK_DISALLOW_COPY_AND_ASSIGN(CleaverImageToMeshFilter);

  static constexpr unsigned int InputImageDimension = TInputImage::ImageDimension;
  static constexpr unsigned int OutputMeshDimension = TOutputMesh::PointDimension;

  using InputImageType = TInputImage;
  using OutputMeshType = TOutputMesh;
  using InputPixelType = typename InputImageType::PixelType;
  using OutputPixelType = typename OutputMeshType::PixelType;

  /** Standard class typedefs. */
  using Self = CleaverImageToMeshFilter<InputImageType, OutputMeshType>;
  using Superclass = ImageToMeshFilter<InputImageType, OutputMeshType>;
  using Pointer = SmartPointer<Self>;
  using ConstPointer = SmartPointer<const Self>;

  /** Run-time type information. */
  itkTypeMacro(CleaverImageToMeshFilter, ImageToMeshFilter);

  /** Standard New macro. */
  itkNewMacro(Self);

protected:
  CleaverImageToMeshFilter();
  ~CleaverImageToMeshFilter() override = default;

  void PrintSelf(std::ostream & os, Indent indent) const override;

  using OutputRegionType = typename OutputMeshType::RegionType;

  void GenerateData() override;

private:
};
} // namespace itk

#ifndef ITK_MANUAL_INSTANTIATION
#  include "itkCleaverImageToMeshFilter.hxx"
#endif

#endif // itkCleaverImageToMeshFilter

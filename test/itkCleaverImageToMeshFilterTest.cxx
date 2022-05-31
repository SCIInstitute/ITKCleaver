/*=========================================================================
 *
 *  Copyright NumFOCUS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         https://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#include "itkCleaverImageToMeshFilter.h"

#include "itkCommand.h"
#include "itkImageFileReader.h"
#include "itkMeshFileWriter.h"
#include "itkTestingMacros.h"
#include "itkMesh.h"

namespace
{
class ShowProgress : public itk::Command
{
public:
  itkNewMacro(ShowProgress);

  void
  Execute(itk::Object * caller, const itk::EventObject & event) override
  {
    Execute((const itk::Object *)caller, event);
  }

  void
  Execute(const itk::Object * caller, const itk::EventObject & event) override
  {
    if (!itk::ProgressEvent().CheckEvent(&event))
    {
      return;
    }
    const auto * processObject = dynamic_cast<const itk::ProcessObject *>(caller);
    if (!processObject)
    {
      return;
    }
    std::cout << " " << processObject->GetProgress();
  }
};
} // namespace

int itkCleaverImageToMeshFilterTest(int argc, char * argv[])
{
  if (argc < 3)
  {
    std::cerr << "Missing parameters." << std::endl;
    std::cerr << "Usage: " << itkNameOfTestExecutableMacro(argv);
    std::cerr << " outputMesh";
    std::cerr << " inputImage [inputImage2] [inputImage3] [...]";
    std::cerr << std::endl;
    return EXIT_FAILURE;
  }
  const char * outputMeshFileName = argv[1];

  constexpr unsigned int Dimension = 3;
  using PixelType = float;
  using ImageType = itk::Image<PixelType, Dimension>;
  using MeshType = itk::Mesh<PixelType, Dimension>;

  using FilterType = itk::CleaverImageToMeshFilter<ImageType, MeshType>;
  FilterType::Pointer filter = FilterType::New();

  ITK_EXERCISE_BASIC_OBJECT_METHODS(filter, CleaverImageToMeshFilter, ImageToMeshFilter);

  for (int ii = 2; ii < argc; ii++)
    {
    const char * inputImageFileName = argv[ii];
    auto image = itk::ReadImage<ImageType>(inputImageFileName);
    filter->SetInput(ii-2, image);
    }

  bool isIndicator = true;
  filter->SetInputIsIndicatorFunction(isIndicator);
  ITK_TEST_SET_GET_VALUE(isIndicator, filter->GetInputIsIndicatorFunction());
  filter->SetInputIsIndicatorFunction(false);

  ShowProgress::Pointer showProgress = ShowProgress::New();
  filter->AddObserver(itk::ProgressEvent(), showProgress);

  filter->Update();

  std::cout << "\nTetrahedral mesh output: " << std::endl;
  filter->GetOutput(0)->Print(std::cout);

  std::cout << "\nTriangle mesh output: " << std::endl;
  filter->GetOutput(1)->Print(std::cout);

  using WriterType = itk::MeshFileWriter<MeshType>;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(outputMeshFileName);
  writer->SetInput(filter->GetOutput(1));

  ITK_TRY_EXPECT_NO_EXCEPTION(writer->Update());

  std::cout << "Test finished." << std::endl;
  return EXIT_SUCCESS;
}

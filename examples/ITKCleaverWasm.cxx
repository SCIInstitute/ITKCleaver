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
#include "itkCleaverImageToMeshFilter.h"
#include "itkPipeline.h"
#include "itkInputImage.h"
#include "itkOutputMesh.h"
#include "itkSupportInputImageTypesNoVectorImage.h"
#include "itkMesh.h"

template<typename TImage>
int
Mesher(itk::wasm::Pipeline & pipeline, std::vector<const TImage *> & inputImages)
{
  using ImageType = TImage;

  using MeshType = itk::Mesh<typename ImageType::PixelType, 3>;
  using OutputMeshType = itk::wasm::OutputMesh<MeshType>;
  OutputMeshType outputTriangleMesh;
  pipeline.add_option("-t,--triangle", outputTriangleMesh, "Output triangle mesh");

  double sigma = 1.0;
  pipeline.add_option("-s,--sigma", sigma, "Blending function sigma for input(s) to remove alias artifacts.");

  double samplingRate = 1.0;
  pipeline.add_option("-r,--sampling-rate", samplingRate, "Sizing field sampling rate. The default sample rate will be the dimensions of the volume. Smaller sampling creates coarser meshes.");

  double lipschitz = 0.2;
  pipeline.add_option("-l,--lipschitz", lipschitz, "Sizing field rate of change. the maximum rate of change of element size throughout a mesh.");

  double featureScaling = 1.0;
  pipeline.add_option("-f,--feature-scaling", featureScaling, "Sizing field feature scaling. Scales features of the mesh effecting element size. Higher feature scaling creates coaser meshes.");

  int padding = 0;
  pipeline.add_option("-p,--padding", padding, "Sizing field padding. Adds a volume buffer around the data. Useful when volumes intersect near the boundary.");

  ITK_WASM_PARSE(pipeline);

  using FilterType = itk::CleaverImageToMeshFilter<ImageType, MeshType>;
  typename FilterType::Pointer filter = FilterType::New();

  for(size_t ii = 0; ii < inputImages.size(); ii++)
  {
    filter->SetInput(ii, inputImages[ii]);
  }

  filter->SetSigma(sigma);
  filter->SetSamplingRate(samplingRate);
  filter->SetLipschitz(lipschitz);
  filter->SetFeatureScaling(featureScaling);
  filter->SetPadding(padding);

  ITK_WASM_CATCH_EXCEPTION(pipeline, filter->Update());

  outputTriangleMesh.Set(filter->GetOutput(1));

  return EXIT_SUCCESS;
}

template<typename TImage>
class PipelineFunctor
{
public:
  int operator()(itk::wasm::Pipeline & pipeline)
  {
    using ImageType = TImage;

    using InputImageType = itk::wasm::InputImage<ImageType>;
    std::vector<InputImageType> inputImages;
    pipeline.add_option("-i,--input", inputImages, "Input label image or multiple indicator function images");

    ITK_WASM_PRE_PARSE(pipeline);

    std::vector<const ImageType *> loadedInputImages;
    for(auto image: inputImages)
    {
      loadedInputImages.push_back(image.Get());
    }

    int result = Mesher<ImageType>(pipeline, loadedInputImages);
    return result;
  }
};

int main( int argc, char * argv[] )
{
  itk::wasm::Pipeline pipeline("Create a multi-material mesh suitable for simulation/modeling from an input label image or indicator function images", argc, argv);

  return itk::wasm::SupportInputImageTypesNoVectorImage<PipelineFunctor,
   //uint8_t,
   //int8_t,
   uint16_t,
   //int16_t,
   //float,
   //double
   float
   >
  ::Dimensions<3U>("Input", pipeline);
}

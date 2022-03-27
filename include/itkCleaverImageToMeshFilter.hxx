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
  this->m_InputIsIndicatorFunction ? "true" : "false";
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

  OutputMeshType *      output = this->GetOutput();

  for (auto field: fields)
  {
    delete field;
  }
}

} // end namespace itk

#endif // itkCleaverImageToMeshFilter_hxx

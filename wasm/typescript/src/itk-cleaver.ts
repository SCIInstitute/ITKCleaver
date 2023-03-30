import {
  Mesh,
  Image,
  InterfaceTypes,
  PipelineOutput,
  PipelineInput,
  runPipeline
} from 'itk-wasm'

import ItkCleaverOptions from './itk-cleaver-options.js'
import ItkCleaverResult from './itk-cleaver-result.js'


import { getPipelinesBaseUrl } from './pipelines-base-url.js'


import { getPipelineWorkerUrl } from './pipeline-worker-url.js'

/**
 * Create a multi-material mesh suitable for simulation/modeling from an input label image or indicator function images
 *
 *
 * @returns {Promise<ItkCleaverResult>} - result object
 */
async function itkCleaver(
  webWorker: null | Worker,
  options: ItkCleaverOptions = { input: [], }
) : Promise<ItkCleaverResult> {

  const desiredOutputs: Array<PipelineOutput> = [
    { type: InterfaceTypes.Mesh },
  ]
  const inputs: Array<PipelineInput> = [
  ]

  const args = []
  // Inputs
  // Outputs
  args.push('0')
  // Options
  args.push('--memory-io')
  if (typeof options.input !== "undefined") {
    if(options.input.length < 1) {
      throw new Error('"input" option must have a length > 1')
    }
    args.push('--input')
    options.input.forEach((value) => {
      const inputCountString = inputs.length.toString()
      inputs.push({ type: InterfaceTypes.Image, data: value as Image})
      args.push(inputCountString)
    })
  }
  if (typeof options.sigma !== "undefined") {
    args.push('--sigma', options.sigma.toString())
  }
  if (typeof options.samplingRate !== "undefined") {
    args.push('--sampling-rate', options.samplingRate.toString())
  }
  if (typeof options.lipschitz !== "undefined") {
    args.push('--lipschitz', options.lipschitz.toString())
  }
  if (typeof options.featureScaling !== "undefined") {
    args.push('--feature-scaling', options.featureScaling.toString())
  }
  if (typeof options.padding !== "undefined") {
    args.push('--padding', options.padding.toString())
  }

  const pipelinePath = 'itk-cleaver'

  const {
    webWorker: usedWebWorker,
    returnValue,
    stderr,
    outputs
  } = await runPipeline(webWorker, pipelinePath, args, desiredOutputs, inputs, { pipelineBaseUrl: getPipelinesBaseUrl(), pipelineWorkerUrl: getPipelineWorkerUrl() })
  if (returnValue !== 0) {
    throw new Error(stderr)
  }

  const result = {
    webWorker: usedWebWorker as Worker,
    triangle: outputs[0].data as Mesh,
  }
  return result
}

export default itkCleaver

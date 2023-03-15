import {
  Mesh,
  Image,
  InterfaceTypes,
  PipelineOutput,
  PipelineInput,
  runPipelineNode
} from 'itk-wasm'

import ItkCleaverOptions from './itk-cleaver-options.js'
import ItkCleaverNodeResult from './itk-cleaver-node-result.js'


import path from 'path'

/**
 * Create a multi-material mesh suitable for simulation/modeling from an input label image or indicator function images
 *
 *
 * @returns {Promise<ItkCleaverNodeResult>} - result object
 */
async function itkCleaverNode(
  options: ItkCleaverOptions = {}
) : Promise<ItkCleaverNodeResult> {

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
    const inputCountString = inputs.length.toString()
    inputs.push({ type: InterfaceTypes.Image, data: options.input as Image})
    args.push('--input', inputCountString)
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

  const pipelinePath = path.join(path.dirname(import.meta.url.substring(7)), '..', 'pipelines', 'itk-cleaver')

  const {
    returnValue,
    stderr,
    outputs
  } = await runPipelineNode(pipelinePath, args, desiredOutputs, inputs)
  if (returnValue !== 0) {
    throw new Error(stderr)
  }

  const result = {
    triangle: outputs[0].data as Mesh,
  }
  return result
}

export default itkCleaverNode

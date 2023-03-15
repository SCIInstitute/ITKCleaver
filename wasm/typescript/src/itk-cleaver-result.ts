import { Mesh } from 'itk-wasm'

interface ItkCleaverResult {
  /** WebWorker used for computation */
  webWorker: Worker | null

  /** Output triangle mesh */
  triangle: Mesh

}

export default ItkCleaverResult

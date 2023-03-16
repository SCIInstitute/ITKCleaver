import { Image } from 'itk-wasm'

interface ItkCleaverOptions {
  /** Input label image or multiple indicator function images */
  input: Image[]

  /** Blending function sigma for input(s) to remove alias artifacts. */
  sigma?: number

  /** Sizing field sampling rate. The default sample rate will be the dimensions of the volume. Smaller sampling creates coarser meshes. */
  samplingRate?: number

  /** Sizing field rate of change. the maximum rate of change of element size throughout a mesh. */
  lipschitz?: number

  /** Sizing field feature scaling. Scales features of the mesh effecting element size. Higher feature scaling creates coaser meshes. */
  featureScaling?: number

  /** Sizing field padding. Adds a volume buffer around the data. Useful when volumes intersect near the boundary. */
  padding?: number

}

export default ItkCleaverOptions

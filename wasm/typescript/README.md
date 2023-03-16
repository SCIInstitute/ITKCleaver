# @itk-wasm/cleaver

[![npm version](https://badge.fury.io/js/@itk-wasm%2Fcleaver.svg)](https://www.npmjs.com/package/@itk-wasm/cleaver)

The Cleaver multimaterial tetrahedral meshing tool.

## Installation

```sh
npm install @itk-wasm/cleaver
```

## Usage

### Browser interface

Import:

```js
import {
  itkCleaver,
  setPipelinesBaseUrl,
  getPipelinesBaseUrl,
  setPipelineWorkerUrl,
  getPipelineWorkerUrl,
} from "@itk-wasm/cleaver"
```

#### itkCleaver

*Create a multi-material mesh suitable for simulation/modeling from an input label image or indicator function images*

```ts
async function itkCleaver(
  webWorker: null | Worker,
  options: ItkCleaverOptions = {}
) : Promise<ItkCleaverResult>
```

| Parameter | Type | Description |
| :-------: | :--: | :---------- |

**`ItkCleaverOptions` interface:**

|     Property     |   Type   | Description                                                                                                                        |
| :--------------: | :------: | :--------------------------------------------------------------------------------------------------------------------------------- |
|      `input`     |  *Image[]* | Input label image or multiple indicator function images                                                                            |
|      `sigma`     | *number* | Blending function sigma for input(s) to remove alias artifacts.                                                                    |
|  `samplingRate`  | *number* | Sizing field sampling rate. The default sample rate will be the dimensions of the volume. Smaller sampling creates coarser meshes. |
|    `lipschitz`   | *number* | Sizing field rate of change. the maximum rate of change of element size throughout a mesh.                                         |
| `featureScaling` | *number* | Sizing field feature scaling. Scales features of the mesh effecting element size. Higher feature scaling creates coaser meshes.    |
|     `padding`    | *number* | Sizing field padding. Adds a volume buffer around the data. Useful when volumes intersect near the boundary.                       |

**`ItkCleaverResult` interface:**

|    Property   |   Type   | Description                    |
| :-----------: | :------: | :----------------------------- |
| **webWorker** | *Worker* | WebWorker used for computation |
|   `triangle`  |  *Mesh*  | Output triangle mesh           |

#### setPipelinesBaseUrl

*Set base URL for WebAssembly assets when vendored.*

```ts
function setPipelinesBaseUrl(
  baseUrl: string | URL
) : void
```

#### getPipelinesBaseUrl

*Get base URL for WebAssembly assets when vendored.*

```ts
function getPipelinesBaseUrl() : string | URL
```

#### setPipelineWorkerUrl

*Set base URL for the itk-wasm pipeline worker script when vendored.*

```ts
function setPipelineWorkerUrl(
  baseUrl: string | URL
) : void
```

#### getPipelineWorkerUrl

*Get base URL for the itk-wasm pipeline worker script when vendored.*

```ts
function getPipelineWorkerUrl() : string | URL
```

### Node interface

Import:

```js
import {
  itkCleaverNode,
  setPipelinesBaseUrl,
  getPipelinesBaseUrl,
  setPipelineWorkerUrl,
  getPipelineWorkerUrl,
} from "@itk-wasm/cleaver"
```

#### itkCleaverNode

*Create a multi-material mesh suitable for simulation/modeling from an input label image or indicator function images*

```ts
async function itkCleaverNode(
  options: ItkCleaverOptions = {}
) : Promise<ItkCleaverNodeResult>
```

| Parameter | Type | Description |
| :-------: | :--: | :---------- |

**`ItkCleaverNodeOptions` interface:**

|     Property     |   Type   | Description                                                                                                                        |
| :--------------: | :------: | :--------------------------------------------------------------------------------------------------------------------------------- |
|      `input`     |  *Image[]* | Input label image or multiple indicator function images                                                                            |
|      `sigma`     | *number* | Blending function sigma for input(s) to remove alias artifacts.                                                                    |
|  `samplingRate`  | *number* | Sizing field sampling rate. The default sample rate will be the dimensions of the volume. Smaller sampling creates coarser meshes. |
|    `lipschitz`   | *number* | Sizing field rate of change. the maximum rate of change of element size throughout a mesh.                                         |
| `featureScaling` | *number* | Sizing field feature scaling. Scales features of the mesh effecting element size. Higher feature scaling creates coaser meshes.    |
|     `padding`    | *number* | Sizing field padding. Adds a volume buffer around the data. Useful when volumes intersect near the boundary.                       |

**`ItkCleaverNodeResult` interface:**

|  Property  |  Type  | Description          |
| :--------: | :----: | :------------------- |
| `triangle` | *Mesh* | Output triangle mesh |

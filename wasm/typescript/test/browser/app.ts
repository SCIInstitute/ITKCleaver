import * as cleaver from '../../dist/bundles/cleaver.js'

// Use local, vendored WebAssembly module assets
const pipelinesBaseUrl = new URL('/pipelines', document.location.origin).href
cleaver.setPipelinesBaseUrl(pipelinesBaseUrl)
const pipelineWorkerUrl = new URL('/web-workers/pipeline.worker.js', document.location.origin).href
cleaver.setPipelineWorkerUrl(pipelineWorkerUrl)

import { readImageArrayBuffer } from "itk-wasm"

const packageFunctions = []
for (const [key, val] of Object.entries(cleaver)) {
  if (typeof val == 'function') {
    packageFunctions.push(key)
  }
}

const pipelineFunctionsList = document.getElementById('pipeline-functions-list')
pipelineFunctionsList.innerHTML = `
<li>
  ${packageFunctions.join('</li>\n<li>')}
</li>
`
console.log('Package functions:', packageFunctions)
console.log('cleaver module:', cleaver)
globalThis.cleaver = cleaver
globalThis.readImageArrayBuffer = readImageArrayBuffer
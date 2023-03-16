import test from 'ava'
import path from 'path'

import { readImageLocalFile } from 'itk-wasm'

import { itkCleaverNode } from '../../dist/bundles/cleaver-node.js'

test('itk-cleaver runs on a label image', async t => {
  const testFilePath = path.resolve('..', 'mickey.nrrd')
  const image = await readImageLocalFile(testFilePath)
  image.name = 'mickey'

  const { triangle: mesh } = await itkCleaverNode({ input: [image,] })

  t.is(mesh.meshType.dimension, 3)
  t.is(mesh.numberOfPoints, 1519)
  t.is(mesh.numberOfCells, 3082)
  t.is(mesh.cellBufferSize, 15410)
  t.is(mesh.points[0], 31.5)
  t.is(mesh.cells[0], 2)
  t.is(mesh.cells[1], 3)
  t.is(mesh.cells[2], 0)
})

test('itk-cleaver runs on indicator functions', async t => {
  const indicatorFunctions = []
  for(let ii = 1; ii < 5; ii++) {
    const num = ii.toString()
    const testFilePath = path.resolve('..', `spheres${num}.nrrd`)
    const image = await readImageLocalFile(testFilePath)
    image.name = `spheres${num}`
    indicatorFunctions.push(image)
  }

  const { triangle: mesh } = await itkCleaverNode({ input: indicatorFunctions })

  t.is(mesh.meshType.dimension, 3)
  t.is(mesh.numberOfPoints, 522)
  t.is(mesh.numberOfCells, 985)
  t.is(mesh.cellBufferSize, 4925)
  t.is(mesh.points[0], 24.399904251098633)
  t.is(mesh.cells[0], 2)
  t.is(mesh.cells[1], 3)
  t.is(mesh.cells[2], 0)
})
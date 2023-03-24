const demoServer = 'http://localhost:5173'

describe('@itk-wasm/cleaver', () => {
  beforeEach(() => {
    cy.visit(demoServer)
  })

  it('runs on a label image', () => {
    cy.window().then(async (win) => {
      const cleaver = win.cleaver
      const readImageArrayBuffer = win.readImageArrayBuffer

      const filename = 'mickey.nrrd'
      cy.fixture(filename, null).then(async (mickeyArray) => {
        const { webWorker, image } = await readImageArrayBuffer(null, mickeyArray.buffer, filename)
        image.name = 'mickey'

        const { triangle: mesh } = await cleaver.itkCleaver(webWorker, { input: [image,] })
        webWorker.terminate()

        expect(mesh.meshType.dimension).to.equal(3)
        expect(mesh.numberOfPoints).to.equal(1519)
        expect(mesh.numberOfCells).to.equal(3082)
        expect(mesh.cellBufferSize).to.equal(15410)
        expect(mesh.points[0]).to.equal(31.5)
        expect(mesh.cells[0]).to.equal(2)
        expect(mesh.cells[1]).to.equal(3)
        expect(mesh.cells[2]).to.equal(0)
      })
    })
  })
})
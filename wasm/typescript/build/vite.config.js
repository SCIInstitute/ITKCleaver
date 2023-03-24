import { defineConfig } from 'vite'
import { viteStaticCopy } from 'vite-plugin-static-copy'
import path from 'path'

const itkConfig = path.resolve(__dirname, '..', 'test', 'browser', 'itkConfig.js')

export default defineConfig({
  root: path.join('test', 'browser'),
  build: {
    outDir: '../../demo',
    emptyOutDir: true,
  },
  plugins: [
    // put lazy loaded JavaScript and Wasm bundles in dist directory
    viteStaticCopy({
      targets: [
        { src: '../../dist/pipelines/*', dest: 'pipelines' },
        { src: '../../dist/web-workers/*', dest: 'web-workers' },
        {
          src: '../../node_modules/itk-image-io/*',
          dest: 'image-io',
        },
      ],
    })
  ],
  resolve: {
    // where itk-wasm code has 'import ../itkConfig.js` point to the path of itkConfig
    alias: {
      '../itkConfig.js': itkConfig,
      '../../itkConfig.js': itkConfig
    }
  },
})

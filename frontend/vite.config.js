import { defineConfig } from 'vite';

// Bundles vendor libraries (OL, Chart.js, Tabulator) as a single IIFE that
// sets window globals (window.ol, window.Chart, window.Tabulator).
// Also processes vendor.css → style.css via the assetFileNames option.
export default defineConfig({
  build: {
    outDir: 'dist',
    emptyOutDir: false,
    assetsInlineLimit: 0,
    minify: 'terser',
    terserOptions: {
      compress: true,
      mangle: true,
      format: { comments: false },
    },
    lib: {
      entry: 'src/libs.js',
      name: 'libs',
      fileName: () => 'libs.js',
      formats: ['iife'],
    },
    rollupOptions: {
      output: {
        assetFileNames: 'bundle.css',
      },
    },
  },
});

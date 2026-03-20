import { defineConfig } from 'vite';

export default defineConfig({
  build: {
    outDir: '../Source/HTML',
    emptyOutDir: false,
    assetsInlineLimit: 0,
    minify: false,
    lib: {
      entry: 'src/vendor.js',
      name: 'vendor',
      fileName: () => 'vendor.js',
      formats: ['iife'],
    },
    rollupOptions: {
      output: {
        assetFileNames: 'vendor.css',
      },
    },
  },
});

import { defineConfig } from 'vite';

export default defineConfig({
  build: {
    outDir: '../Source/HTML',
    emptyOutDir: false,
    assetsInlineLimit: 0,
    minify: 'terser',
    terserOptions: {
      compress: true,
      mangle: true,
      format: { comments: false },
    },
    lib: {
      entry: 'src/vendor.js',
      name: 'libs',
      fileName: () => 'libs.js',
      formats: ['iife'],
    },
    rollupOptions: {
      output: {
        assetFileNames: 'style.css',
      },
    },
  },
});

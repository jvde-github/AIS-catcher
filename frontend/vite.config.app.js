import { defineConfig } from 'vite';

export default defineConfig({
  build: {
    outDir: '../Source/HTML',
    emptyOutDir: false,
    minify: 'terser',
    terserOptions: {
      compress: true,
      mangle: true,
      format: { comments: false },
    },
    lib: {
      entry: 'src/app.js',
      name: 'app',
      fileName: () => 'script.js',
      formats: ['iife'],
    },
    rollupOptions: {
      output: {
        // script.js uses undeclared variable assignments (e.g. getDimVal = ...)
        // that only work in sloppy mode; strict mode would throw ReferenceError.
        strict: false,
      },
    },
  },
});

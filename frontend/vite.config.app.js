import { defineConfig } from 'vite';

// Bundles script.js (ES module entry) + its imports (events.js, future tab modules)
// into a single minified ES module served as script.js.
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
      entry: 'src/script.js',
      fileName: () => 'script.js',
      formats: ['es'],
    },
  },
});

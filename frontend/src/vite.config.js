import { defineConfig } from 'vite';
import path from 'path';

export default defineConfig({
  // Force non-ASCII chars to \uXXXX escapes — prevents esbuild from emitting
  // literal Unicode into new RegExp() string args (which V8 rejects as invalid ranges)
  esbuild: {
    charset: 'ascii',
  },
  build: {
    lib: {
      entry: path.resolve(__dirname, 'index.js'),
      name: '_',
      formats: ['iife'],
      fileName: () => 'lib.js',
    },
    outDir: path.resolve(__dirname, '../dist'),
    emptyOutDir: false,
    // Inline all assets (flag SVGs etc.) into the CSS — keeps output self-contained
    assetsInlineLimit: Number.MAX_SAFE_INTEGER,
    rollupOptions: {
      output: {
        assetFileNames: (assetInfo) =>
          assetInfo.name?.endsWith('.css') ? 'lib.css' : (assetInfo.name ?? 'asset'),
      },
    },
  },
});

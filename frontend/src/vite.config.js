import { defineConfig } from 'vite';
import path from 'path';

export default defineConfig({
  // ASCII-only output — prevents esbuild from emitting literal Unicode into
  // `new RegExp()` string args, which V8 rejects as invalid ranges.
  esbuild: { charset: 'ascii' },
  build: {
    outDir: path.resolve(__dirname, '../dist'),
    emptyOutDir: false,
    assetsInlineLimit: Number.MAX_SAFE_INTEGER,
    cssCodeSplit: false,
    // Skip the modulepreload polyfill — modern browsers have native support.
    modulePreload: false,
    // OpenLayers is ~580 KB and eager (map is the default tab). Silence the
    // 500 KB warning rather than chase a split that wouldn't help load time.
    chunkSizeWarningLimit: 700,
    rollupOptions: {
      input: { script: path.resolve(__dirname, 'script.js') },
      output: {
        format: 'es',
        entryFileNames: '[name].js',
        chunkFileNames: 'lib-[name]-[hash].js',
        assetFileNames: (assetInfo) =>
          assetInfo.name?.endsWith('.css') ? 'lib.css' : (assetInfo.name ?? 'asset'),
        manualChunks(id) {
          // core/ MUST be its own chunk — otherwise lazy tab chunks
          // statically import script.js and the browser instantiates it
          // twice (different URL than the original <script> tag).
          if (id.includes('/frontend/src/core/')) return 'core';

          if (id.includes('node_modules/chart.js') ||
              id.includes('node_modules/chartjs-plugin-annotation')) return 'chart';
          if (id.includes('node_modules/ol/')) return 'ol';
          if (id.includes('node_modules/tabulator-tables')) return 'tabulator';
          if (id.includes('node_modules/marked')) return 'marked';
        },
      },
    },
  },
});

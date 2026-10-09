import { defineConfig } from 'vite';
import fs from 'fs';
import path from 'path';

// OpenLayers as node resolves it from here: the viewer's own copy if it has one, else the workspace's
const nodeModules = [path.resolve(__dirname, 'node_modules'), path.resolve(__dirname, '../node_modules')].find((d) => fs.existsSync(path.join(d, 'ol')));

export default defineConfig({
  // ASCII-only output — prevents esbuild from emitting literal Unicode into
  // `new RegExp()` string args, which V8 rejects as invalid ranges.
  esbuild: { charset: 'ascii' },
  resolve: { alias: {
    ol: path.join(nodeModules, 'ol'),
    'ol-mapbox-style': path.join(nodeModules, 'ol-mapbox-style/src/index.js'),
  } },
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
      input: {
        script: path.resolve(__dirname, 'bootstrap.js'),
        'control-app': path.resolve(__dirname, '../control/js/app.js'),
      },
      output: {
        format: 'es',
        entryFileNames: '[name].js',
        chunkFileNames: 'lib-[name]-[hash].js',
        assetFileNames: (assetInfo) =>
          assetInfo.name?.endsWith('.css') ? 'lib.css' : (assetInfo.name ?? 'asset'),
        manualChunks(id) {
          // Bootstrap and the viewer share configuration code without either
          // importing the other's entry point (which would run startup twice).
          if (id.includes('vite/preload-helper')) return 'preload';
          if (id.endsWith('/features/server-config/server-config.js')) return 'configuration';
          // core/ and the packages are one chunk, shared by the viewer, its
          // lazy tabs and the hub. A lazy tab imports the rest of the viewer
          // from the chunk bootstrap.js loads, never from the entry the page
          // names (script.js?hash=...), which the browser would run twice.
          if (id.includes('/frontend/src/core/') || id.includes('/frontend/packages/')) return 'core';
          if (id.includes('/frontend/src/overlays/pollingtile')) return 'core';

          if (id.includes('node_modules/chart.js') ||
              id.includes('node_modules/chartjs-plugin-annotation')) return 'chart';
          if (id.includes('node_modules/ol/')) return 'ol';
          if (id.includes('node_modules/tabulator-tables')) return 'tabulator';
          if (id.includes('node_modules/marked')) return 'marked';
          if (id.includes('node_modules/ol-mapbox-style') || id.includes('node_modules/@maplibre') ||
              id.includes('node_modules/mapbox-to-css-font')) return 'mapbox-style';
        },
      },
    },
  },
});

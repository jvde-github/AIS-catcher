import js from '@eslint/js';
import globals from 'globals';

export default [
  {
    ignores: ['../dist/**', 'node_modules/**', 'lib.js', 'package-lock.json'],
  },
  js.configs.recommended,
  {
    files: ['index.js'],
    languageOptions: {
      ecmaVersion: 2022,
      sourceType: 'module',
      globals: { ...globals.browser },
    },
  },
  {
    files: ['script.js'],
    languageOptions: {
      ecmaVersion: 2022,
      sourceType: 'script',
      globals: {
        ...globals.browser,

        // bundled by index.js → window
        marked: 'readonly',
        Chart: 'readonly',
        Tabulator: 'readonly',
        ol: 'readonly',

        // server-injected (Source/Application/WebViewer.cpp)
        build_string: 'readonly',
        build_version: 'readonly',
        context: 'writable',
        plugins: 'readonly',
        server_message: 'writable',
        webcontrol_http: 'readonly',
        param_share_loc: 'readonly',
        message_save: 'readonly',
        realtime_enabled: 'readonly',
        log_enabled: 'readonly',
        decoder_enabled: 'readonly',
        server_receivers: 'readonly',

        // assigned via window[varName] = ... (initPlots) or window.loadPlugins
        chart_seconds: 'writable',
        chart_minutes: 'writable',
        chart_hours: 'writable',
        chart_days: 'writable',
        chart_ppm: 'writable',
        chart_ppm_minute: 'writable',
        chart_distance_hour: 'writable',
        chart_distance_day: 'writable',
        chart_minute_vessel: 'writable',
        chart_hour_vessel: 'writable',
        chart_day_vessel: 'writable',
        chart_radar_hour: 'writable',
        chart_radar_day: 'writable',
        chart_level: 'writable',
        chart_level_hour: 'writable',
        chart_json: 'readonly',
        loadPlugins: 'writable',
      },
    },
    rules: {
      'no-var': 'warn',
      'no-redeclare': 'warn',
      'eqeqeq': ['warn', 'smart'],
      'no-unused-vars': ['warn', { args: 'none', ignoreRestSiblings: true }],
      'no-undef': 'warn',
      'no-empty': ['warn', { allowEmptyCatch: true }],
      'no-constant-condition': ['warn', { checkLoops: false }],
      'no-prototype-builtins': 'warn',
      'no-case-declarations': 'warn',
    },
  },
];

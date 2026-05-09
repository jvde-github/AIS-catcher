import js from '@eslint/js';
import globals from 'globals';

export default [
  {
    ignores: ['../dist/**', 'node_modules/**', 'lib.js', 'package-lock.json'],
  },
  js.configs.recommended,
  {
    files: ['script.js', 'core/**/*.js', 'tabs/**/*.js', 'overlays/**/*.js'],
    languageOptions: {
      ecmaVersion: 2022,
      sourceType: 'module',
      globals: {
        ...globals.browser,

        // server-injected (Source/Application/WebViewer.cpp)
        // window.__SERVER_CONFIG__ is the single source of truth; read via the
        // top-of-file `config` const. Listed here so eslint accepts the raw
        // window property reference.
        __SERVER_CONFIG__: 'readonly',

        // dead code (the JSON.parse(chart_json) branch in updatePlots is in an
        // `if (true) {} else { ... }` and never runs); kept here so lint stays
        // quiet until the dead branch is removed.
        chart_json: 'readonly',
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

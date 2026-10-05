import js from '@eslint/js';
import globals from 'globals';

/* The rule that holds the layout together: core <- ui <- map <- apps. A
   package may import the ones before it, never after, and core has no DOM. */
const noImports = (patterns) => ({ 'no-restricted-imports': ['error', { patterns }] });

const rules = {
  'no-var': 'warn',
  'no-redeclare': 'warn',
  'eqeqeq': ['warn', 'smart'],
  'no-unused-vars': ['warn', { args: 'none', ignoreRestSiblings: true }],
  'no-undef': 'warn',
  'no-empty': ['warn', { allowEmptyCatch: true }],
  'no-constant-condition': ['warn', { checkLoops: false }],
  'no-prototype-builtins': 'warn',
  'no-case-declarations': 'warn',
};

export default [
  { ignores: ['dist/**', '**/node_modules/**'] },
  js.configs.recommended,
  {
    files: ['packages/**/*.js', 'src/**/*.js', 'control/js/*.js', 'test/**/*.js'],
    languageOptions: {
      ecmaVersion: 2022,
      sourceType: 'module',
      globals: {
        ...globals.browser,
        // server-injected (Source/Web/WebViewer.cpp); read through the top-of-file `config`
        __SERVER_CONFIG__: 'readonly',
        // dead code in updatePlots (an `if (true) {} else { ... }` branch); kept quiet until removed
        chart_json: 'readonly',
      },
    },
    rules,
  },
  // node scripts: the build tools and the tests
  { files: ['test/**/*.js', 'test/**/*.mjs', 'src/tools/*.mjs'], languageOptions: { globals: { ...globals.node } } },
  {
    files: ['packages/core/**/*.js'],
    rules: {
      ...noImports(['@aiscatcher/ui', '@aiscatcher/ui/*', '@aiscatcher/map', '@aiscatcher/map/*', 'ol', 'ol/*']),
      'no-restricted-globals': ['error', 'window', 'document', 'localStorage', 'sessionStorage', 'navigator'],
    },
  },
  { files: ['packages/ui/**/*.js'], rules: noImports(['@aiscatcher/map', '@aiscatcher/map/*', 'ol', 'ol/*']) },
];

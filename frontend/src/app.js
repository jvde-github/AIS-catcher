// Import the explicit public API from script.js.
// ES module live bindings mean api.card_mmsi etc. always reflect the current value.
// We expose each export as a window getter so HTML event handlers can read them.
import * as api from './script.js';

for (const key of Object.keys(api)) {
  Object.defineProperty(window, key, {
    get: () => api[key],
    configurable: true,
  });
}

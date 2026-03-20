"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
/* global globalThis */
/* eslint-disable import/no-mutable-exports */
const index_js_1 = require("../compression/index.js");
const worker = /** @type {Worker} */ ( /** @type {unknown} */(globalThis));
worker.addEventListener('message', async (e) => {
    const { compression, decoderParameters, buffer, ...extra } = e.data;
    try {
        const decoder = await (0, index_js_1.getDecoder)(compression, decoderParameters);
        const decoded = await decoder.decode(buffer);
        worker.postMessage({ decoded, ...extra }, [decoded]);
    }
    catch (error) {
        if (error instanceof Error) {
            worker.postMessage({ error: error.message, ...extra });
        }
        else {
            worker.postMessage({ error: String(error), ...extra });
        }
    }
});
//# sourceMappingURL=decoder.js.map
//# sourceMappingURL=decoder.js.map
"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.addDecoder = addDecoder;
exports.getDecoderParameters = getDecoderParameters;
exports.getDecoder = getDecoder;
exports.preferWorker = preferWorker;
/** @import BaseDecoder, {BaseDecoderParameters} from "./basedecoder.js" */
/**
 * @typedef {Object} RegistryEntry
 * @property {function():Promise<typeof BaseDecoder>} importFn
 * @property {function(import("../imagefiledirectory.js").ImageFileDirectory):Promise<BaseDecoderParameters>} decoderParameterFn
 * @property {boolean} preferWorker
 */
/** @type {Map<number | undefined, RegistryEntry>} */
const registry = new Map();
/**
 * Default decoder parameter retrieval function
 * @param {import("../imagefiledirectory.js").ImageFileDirectory} fileDirectory
 * @returns {Promise<BaseDecoderParameters>}
 */
async function defaultDecoderParameterFn(fileDirectory) {
    const isTiled = !fileDirectory.hasTag('StripOffsets');
    return /** @type {BaseDecoderParameters} */ ({
        tileWidth: isTiled
            ? await fileDirectory.loadValue('TileWidth')
            : await fileDirectory.loadValue('ImageWidth'),
        tileHeight: isTiled
            ? await fileDirectory.loadValue('TileLength')
            : (await fileDirectory.loadValue('RowsPerStrip')
                || await fileDirectory.loadValue('ImageLength')),
        planarConfiguration: await fileDirectory.loadValue('PlanarConfiguration'),
        bitsPerSample: await fileDirectory.loadValue('BitsPerSample'),
        predictor: await fileDirectory.loadValue('Predictor') || 1,
    });
}
/**
 * Register a decoder for a specific compression method or a range of compressions
 * @param {(number|undefined|(number|undefined)[])} cases ids of the compression methods to register for
 * @param {function():Promise<typeof BaseDecoder>} importFn the function to import the decoder
 * @param {function(import("../imagefiledirectory.js").ImageFileDirectory):Promise<BaseDecoderParameters>} decoderParameterFn
 * @param {boolean} preferWorker_ Whether to prefer running the decoder in a worker
 */
function addDecoder(cases, importFn, decoderParameterFn = defaultDecoderParameterFn, preferWorker_ = true) {
    if (!Array.isArray(cases)) {
        cases = [cases]; // eslint-disable-line no-param-reassign
    }
    cases.forEach((c) => {
        registry.set(c, { importFn, decoderParameterFn, preferWorker: preferWorker_ });
    });
}
/**
 * Get the required decoder parameters for a specific compression method
 * @param {number|undefined} compression
 * @param {import('../imagefiledirectory.js').ImageFileDirectory} fileDirectory
 */
async function getDecoderParameters(compression, fileDirectory) {
    if (!registry.has(compression)) {
        throw new Error(`Unknown compression method identifier: ${compression}`);
    }
    const { decoderParameterFn } = /** @type {RegistryEntry} */ (registry.get(compression));
    return decoderParameterFn(fileDirectory);
}
/**
 * Get a decoder for a specific compression and parameters
 * @param {number} compression the compression method identifier
 * @param {BaseDecoderParameters} decoderParameters the parameters for the decoder
 * @returns {Promise<import('./basedecoder.js').default>}
 */
async function getDecoder(compression, decoderParameters) {
    if (!registry.has(compression)) {
        throw new Error(`Unknown compression method identifier: ${compression}`);
    }
    const { importFn } = /** @type {RegistryEntry} */ (registry.get(compression));
    const Decoder = await importFn();
    return new Decoder(decoderParameters);
}
/**
 * Whether to prefer running the decoder in a worker
 * @param {number|undefined} compression the compression method identifier
 * @returns {boolean}
 */
function preferWorker(compression) {
    if (!registry.has(compression)) {
        throw new Error(`Unknown compression method identifier: ${compression}`);
    }
    return /** @type {RegistryEntry} */ (registry.get(compression)).preferWorker;
}
const defaultDecoderDefinitions = [
    // No compression
    {
        cases: [undefined, 1],
        importFn: () => Promise.resolve().then(() => __importStar(require('./raw.js'))).then((m) => m.default),
        preferWorker: false,
    },
    // LZW
    {
        cases: 5,
        importFn: () => Promise.resolve().then(() => __importStar(require('./lzw.js'))).then((m) => m.default),
    },
    // Old-style JPEG
    {
        cases: 6,
        importFn: () => {
            throw new Error('old style JPEG compression is not supported.');
        },
    },
    // JPEG
    {
        cases: 7,
        importFn: () => Promise.resolve().then(() => __importStar(require('./jpeg.js'))).then((m) => m.default),
        /**
         * @param {import("../imagefiledirectory.js").ImageFileDirectory} fileDirectory
         */
        decoderParameterFn: async (fileDirectory) => {
            return {
                ...await defaultDecoderParameterFn(fileDirectory),
                JPEGTables: await fileDirectory.loadValue('JPEGTables'),
            };
        },
    },
    // Deflate / Adobe Deflate
    {
        cases: [8, 32946],
        importFn: () => Promise.resolve().then(() => __importStar(require('./deflate.js'))).then((m) => m.default),
    },
    // PackBits
    {
        cases: 32773,
        importFn: () => Promise.resolve().then(() => __importStar(require('./packbits.js'))).then((m) => m.default),
    },
    // LERC
    {
        cases: 34887,
        importFn: () => Promise.resolve().then(() => __importStar(require('./lerc.js'))).then(async (m) => {
            await m.zstd.init();
            return m;
        })
            .then((m) => m.default),
        /**
         * @param {import("../imagefiledirectory.js").ImageFileDirectory} fileDirectory
         */
        decoderParameterFn: async (fileDirectory) => {
            return {
                ...await defaultDecoderParameterFn(fileDirectory),
                LercParameters: await fileDirectory.loadValue('LercParameters'),
            };
        },
    },
    // zstd
    {
        cases: 50000,
        importFn: () => Promise.resolve().then(() => __importStar(require('./zstd.js'))).then(async (m) => {
            await m.zstd.init();
            return m;
        })
            .then((m) => m.default),
    },
    // WebP Images
    {
        cases: 50001,
        importFn: () => Promise.resolve().then(() => __importStar(require('./webimage.js'))).then((m) => m.default),
        /**
         * @param {import("../imagefiledirectory.js").ImageFileDirectory} fileDirectory
         */
        decoderParameterFn: async (fileDirectory) => {
            return {
                ...await defaultDecoderParameterFn(fileDirectory),
                samplesPerPixel: Number(await fileDirectory.loadValue('SamplesPerPixel')) || 4,
            };
        },
        preferWorker: false,
    },
];
// Add default decoders to registry (end-user may override with other implementations)
for (const decoderDefinition of defaultDecoderDefinitions) {
    const { cases, importFn, decoderParameterFn, preferWorker: preferWorker_ } = decoderDefinition;
    addDecoder(cases, importFn, decoderParameterFn, preferWorker_);
}
//# sourceMappingURL=index.js.map
//# sourceMappingURL=index.js.map
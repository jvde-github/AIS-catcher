/**
 * Register a decoder for a specific compression method or a range of compressions
 * @param {(number|undefined|(number|undefined)[])} cases ids of the compression methods to register for
 * @param {function():Promise<typeof BaseDecoder>} importFn the function to import the decoder
 * @param {function(import("../imagefiledirectory.js").ImageFileDirectory):Promise<BaseDecoderParameters>} decoderParameterFn
 * @param {boolean} preferWorker_ Whether to prefer running the decoder in a worker
 */
export function addDecoder(cases: (number | undefined | (number | undefined)[]), importFn: () => Promise<typeof BaseDecoder>, decoderParameterFn?: (arg0: import("../imagefiledirectory.js").ImageFileDirectory) => Promise<BaseDecoderParameters>, preferWorker_?: boolean): void;
/**
 * Get the required decoder parameters for a specific compression method
 * @param {number|undefined} compression
 * @param {import('../imagefiledirectory.js').ImageFileDirectory} fileDirectory
 */
export function getDecoderParameters(compression: number | undefined, fileDirectory: import("../imagefiledirectory.js").ImageFileDirectory): Promise<BaseDecoderParameters>;
/**
 * Get a decoder for a specific compression and parameters
 * @param {number} compression the compression method identifier
 * @param {BaseDecoderParameters} decoderParameters the parameters for the decoder
 * @returns {Promise<import('./basedecoder.js').default>}
 */
export function getDecoder(compression: number, decoderParameters: BaseDecoderParameters): Promise<import("./basedecoder.js").default>;
/**
 * Whether to prefer running the decoder in a worker
 * @param {number|undefined} compression the compression method identifier
 * @returns {boolean}
 */
export function preferWorker(compression: number | undefined): boolean;
export type RegistryEntry = {
    importFn: () => Promise<typeof BaseDecoder>;
    decoderParameterFn: (arg0: import("../imagefiledirectory.js").ImageFileDirectory) => Promise<BaseDecoderParameters>;
    preferWorker: boolean;
};
import type BaseDecoder from "./basedecoder.js";
import type { BaseDecoderParameters } from "./basedecoder.js";
//# sourceMappingURL=index.d.ts.map
import { TypedArrayWithDimensions } from './geotiff.js';
export declare function fromWhiteIsZero(raster: TypedArrayWithDimensions, max: number): Uint8Array;
export declare function fromBlackIsZero(raster: TypedArrayWithDimensions, max: number): Uint8Array;
export declare function fromPalette(raster: TypedArrayWithDimensions, colorMap: Uint16Array): Uint8Array;
export declare function fromCMYK(cmykRaster: TypedArrayWithDimensions): Uint8Array;
export declare function fromYCbCr(yCbCrRaster: TypedArrayWithDimensions): Uint8ClampedArray;
export declare function fromCIELab(cieLabRaster: TypedArrayWithDimensions): Uint8Array;
//# sourceMappingURL=rgb.d.ts.map
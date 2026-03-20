export type DecoderExports = {
	memory: Uint8Array;

	ZSTD_findDecompressedSize: (compressedPtr: number, compressedSize: number) => bigint;
	ZSTD_decompress: (
		uncompressedPtr: number,
		uncompressedSize: number,
		compressedPtr: number,
		compressedSize: number,
	) => number;
	malloc: (ptr: number) => number;
	free: (ptr: number) => void;
};

export type StreamDecoderExports = DecoderExports & {
	ZSTD_createDCtx: () => number;
	ZSTD_decompressStream: (dctx: number, output: number, input: number) => number;
	ZSTD_freeDCtx: (dctx: number) => void;
	ZSTD_DStreamInSize: () => number;
	ZSTD_DStreamOutSize: () => number;
};

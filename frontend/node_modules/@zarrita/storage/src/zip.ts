import type { Reader, ZipEntry, ZipInfo } from "unzipit";
import { unzip } from "unzipit";
import type { AbsolutePath, AsyncReadable, RangeQuery } from "./types.js";
import { assert, fetch_range, strip_prefix } from "./util.js";

/**
 * Internal type for accessing unzipit internals that are not exposed in TypeScript types.
 * These properties exist at runtime on ZipEntry objects.
 */
interface ZipEntryInternal extends ZipEntry {
	/** The underlying Reader used to read the zip file */
	_reader: Reader;
	/** Raw entry data from the zip central directory */
	_rawEntry: {
		/** Offset of the local file header in the zip file */
		relativeOffsetOfLocalHeader: number;
		/** 0 = stored (no compression), 8 = deflate */
		compressionMethod: number;
		/** Size of compressed data */
		compressedSize: number;
		/** Size of uncompressed data */
		uncompressedSize: number;
	};
	/** Compression method (0 = stored, 8 = deflate) */
	compressionMethod: number;
}

/**
 * Type guard to verify a ZipEntry has the internal properties we rely on.
 * This protects against changes in unzipit internals.
 */
function isZipEntryInternal(entry: ZipEntry): entry is ZipEntryInternal {
	if (!("compressionMethod" in entry) || !("_rawEntry" in entry)) {
		return false;
	}

	const rawEntry = (entry as ZipEntryInternal)._rawEntry;
	return (
		typeof (entry as ZipEntryInternal).compressionMethod === "number" &&
		typeof rawEntry === "object" &&
		rawEntry !== null &&
		"relativeOffsetOfLocalHeader" in rawEntry &&
		typeof rawEntry.relativeOffsetOfLocalHeader === "number"
	);
}

export class BlobReader implements Reader {
	constructor(public blob: Blob) {}
	async getLength(): Promise<number> {
		return this.blob.size;
	}
	async read(offset: number, length: number): Promise<Uint8Array> {
		const blob = this.blob.slice(offset, offset + length);
		return new Uint8Array(await blob.arrayBuffer());
	}
}

/** Options for {@linkcode ZipFileStore}. */
interface ZipFileStoreOptions {
	/**
	 * Optional function to transform entries after unzipping.
	 *
	 * Useful for modifying or restructuring the paths of extracted zip entries.
	 */
	transformEntries?: (entries: ZipInfo["entries"]) => ZipInfo["entries"];
}

export class HTTPRangeReader implements Reader {
	private length?: number;
	#overrides: RequestInit;
	constructor(
		public url: string | URL,
		opts: { overrides?: RequestInit } = {},
	) {
		this.#overrides = opts.overrides ?? {};
	}

	async getLength(): Promise<number> {
		if (this.length === undefined) {
			const req = await fetch(this.url as string, {
				...this.#overrides,
				method: "HEAD",
			});
			assert(
				req.ok,
				`failed http request ${this.url}, status: ${req.status}: ${req.statusText}`,
			);
			this.length = Number(req.headers.get("content-length"));
			if (Number.isNaN(this.length)) {
				throw Error("could not get length");
			}
		}
		return this.length;
	}

	async read(offset: number, size: number): Promise<Uint8Array> {
		if (size === 0) {
			return new Uint8Array(0);
		}
		const req = await fetch_range(this.url, offset, size, this.#overrides);
		assert(
			req.ok,
			`failed http request ${this.url}, status: ${req.status} offset: ${offset} size: ${size}: ${req.statusText}`,
		);
		return new Uint8Array(await req.arrayBuffer());
	}
}

/** @experimental */
class ZipFileStore<R extends Reader = Reader> implements AsyncReadable {
	private info: Promise<ZipInfo>;
	private reader: R;

	constructor(reader: R, opts: ZipFileStoreOptions = {}) {
		this.reader = reader;
		this.info = unzip(reader).then((info) => {
			if (opts.transformEntries) {
				info.entries = opts.transformEntries(info.entries);
			}
			return info;
		});
	}

	/**
	 * Compute the byte offset where entry data begins in the zip file.
	 * This requires reading the local file header to get filename and extra field lengths.
	 */
	private async getEntryDataOffset(entry: ZipEntryInternal): Promise<number> {
		const localHeaderOffset = entry._rawEntry.relativeOffsetOfLocalHeader;
		// Read local file header (30 bytes minimum)
		const header = await this.reader.read(localHeaderOffset, 30);
		// File name length at offset 26 (2 bytes, little-endian)
		const fileNameLength = header[26] + header[27] * 256;
		// Extra field length at offset 28 (2 bytes, little-endian)
		const extraFieldLength = header[28] + header[29] * 256;
		// Data starts after: local header (30) + filename + extra field
		return localHeaderOffset + 30 + fileNameLength + extraFieldLength;
	}

	async get(key: AbsolutePath): Promise<Uint8Array | undefined> {
		let entry = (await this.info).entries[strip_prefix(key)];
		if (!entry) return;
		return new Uint8Array(await entry.arrayBuffer());
	}

	async getRange(
		key: AbsolutePath,
		range: RangeQuery,
	): Promise<Uint8Array | undefined> {
		const entry = (await this.info).entries[strip_prefix(key)];
		if (!entry) return undefined;

		if (!isZipEntryInternal(entry)) {
			throw new Error(
				"ZipFileStore.getRange requires internal unzipit properties that are not available. " +
					"This may indicate an incompatible version of unzipit.",
			);
		}

		// For compressed entries, fall back to reading full entry and slicing
		if (entry.compressionMethod !== 0) {
			const bytes = await this.get(key);
			if (!bytes) return undefined;
			if ("suffixLength" in range) {
				return bytes.slice(-range.suffixLength);
			}
			return bytes.slice(range.offset, range.offset + range.length);
		}

		// For uncompressed (stored) entries, read directly from underlying reader
		const dataOffset = await this.getEntryDataOffset(entry);

		if ("suffixLength" in range) {
			const start = dataOffset + entry.size - range.suffixLength;
			return this.reader.read(start, range.suffixLength);
		}
		return this.reader.read(dataOffset + range.offset, range.length);
	}

	async has(key: AbsolutePath): Promise<boolean> {
		return strip_prefix(key) in (await this.info).entries;
	}

	static fromUrl(
		href: string | URL,
		opts: { overrides?: RequestInit } & ZipFileStoreOptions = {},
	): ZipFileStore<HTTPRangeReader> {
		return new ZipFileStore(new HTTPRangeReader(href, opts), opts);
	}

	static fromBlob(
		blob: Blob,
		opts: ZipFileStoreOptions = {},
	): ZipFileStore<BlobReader> {
		return new ZipFileStore(new BlobReader(blob), opts);
	}
}

export default ZipFileStore;

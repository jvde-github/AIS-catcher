import { Buffer } from "node:buffer";
import * as fs from "node:fs";
import * as path from "node:path";

import type { AbsolutePath, AsyncMutable, RangeQuery } from "./types.js";
import { strip_prefix } from "./util.js";

function is_error_no_entry(err: unknown): err is { code: "ENOENT" } {
	const is_object = typeof err === "object" && err !== null;
	return is_object && "code" in err && err.code === "ENOENT";
}

class FileSystemStore implements AsyncMutable {
	constructor(public root: string) {}

	async get(key: AbsolutePath): Promise<Uint8Array | undefined> {
		let fp = path.join(this.root, strip_prefix(key));
		return fs.promises.readFile(fp).catch((err) => {
			if (err.code === "ENOENT") return undefined;
			throw err;
		});
	}

	async getRange(
		key: AbsolutePath,
		range: RangeQuery,
	): Promise<Uint8Array | undefined> {
		let fp = path.join(this.root, strip_prefix(key));
		let filehandle: fs.promises.FileHandle | undefined;
		try {
			filehandle = await fs.promises.open(fp, "r");
			if ("suffixLength" in range) {
				let stats = await filehandle.stat();
				let data = Buffer.alloc(range.suffixLength);
				await filehandle.read(
					data,
					0,
					range.suffixLength,
					stats.size - range.suffixLength,
				);
				return data;
			}
			let data = Buffer.alloc(range.length);
			await filehandle.read(data, 0, range.length, range.offset);
			return data;
		} catch (err: unknown) {
			// return undefined is no file or directory
			if (is_error_no_entry(err)) {
				return undefined;
			}
			throw err;
		} finally {
			await filehandle?.close();
		}
	}

	async has(key: AbsolutePath): Promise<boolean> {
		const fp = path.join(this.root, strip_prefix(key));
		return fs.promises
			.access(fp)
			.then(() => true)
			.catch(() => false);
	}

	async set(key: AbsolutePath, value: Uint8Array): Promise<void> {
		const fp = path.join(this.root, strip_prefix(key));
		await fs.promises.mkdir(path.dirname(fp), { recursive: true });
		await fs.promises.writeFile(fp, value, null);
	}

	async delete(key: AbsolutePath): Promise<boolean> {
		const fp = path.join(this.root, strip_prefix(key));
		await fs.promises.unlink(fp);
		return true;
	}
}

export default FileSystemStore;

import type { Readable } from "@zarrita/storage";
import { KeyError, NodeNotFoundError } from "./errors.js";
import { Array, Group, Location } from "./hierarchy.js";
import type {
	ArrayMetadata,
	Attributes,
	DataType,
	GroupMetadata,
} from "./metadata.js";
import {
	ensure_correct_scalar,
	json_decode_object,
	rethrow_unless,
	v2_to_v3_array_metadata,
	v2_to_v3_group_metadata,
} from "./util.js";

let VERSION_COUNTER = create_version_counter();
function create_version_counter() {
	let version_counts = new WeakMap<Readable, { v2: number; v3: number }>();
	function get_counts(store: Readable) {
		let counts = version_counts.get(store) ?? { v2: 0, v3: 0 };
		version_counts.set(store, counts);
		return counts;
	}
	return {
		increment(store: Readable, version: "v2" | "v3") {
			get_counts(store)[version] += 1;
		},
		version_max(store: Readable): "v2" | "v3" {
			let counts = get_counts(store);
			return counts.v3 > counts.v2 ? "v3" : "v2";
		},
	};
}

async function load_attrs(location: Location<Readable>): Promise<Attributes> {
	let meta_bytes = await location.store.get(location.resolve(".zattrs").path);
	if (!meta_bytes) return {};
	return json_decode_object(meta_bytes);
}

function open_v2<Store extends Readable>(
	location: Location<Store> | Store,
	options: { kind: "group"; attrs?: boolean },
): Promise<Group<Store>>;

function open_v2<Store extends Readable>(
	location: Location<Store> | Store,
	options: { kind: "array"; attrs?: boolean },
): Promise<Array<DataType, Store>>;

function open_v2<Store extends Readable>(
	location: Location<Store> | Store,
	options?: { kind?: "array" | "group"; attrs?: boolean },
): Promise<Array<DataType, Store> | Group<Store>>;

async function open_v2<Store extends Readable>(
	location: Location<Store> | Store,
	options: { kind?: "array" | "group"; attrs?: boolean } = {},
) {
	let loc = "store" in location ? location : new Location(location);
	let attrs = {};
	if (options.attrs ?? true) attrs = await load_attrs(loc);
	if (options.kind === "array") return open_array_v2(loc, attrs);
	if (options.kind === "group") return open_group_v2(loc, attrs);
	return open_array_v2(loc, attrs).catch((err) => {
		rethrow_unless(err, NodeNotFoundError);
		return open_group_v2(loc, attrs);
	});
}

async function open_array_v2<Store extends Readable>(
	location: Location<Store>,
	attrs: Attributes,
) {
	let { path } = location.resolve(".zarray");
	let meta = await location.store.get(path);
	if (!meta) {
		throw new NodeNotFoundError("v2 array", {
			cause: new KeyError(path),
		});
	}
	VERSION_COUNTER.increment(location.store, "v2");
	return new Array(
		location.store,
		location.path,
		v2_to_v3_array_metadata(json_decode_object(meta), attrs),
	);
}

async function open_group_v2<Store extends Readable>(
	location: Location<Store>,
	attrs: Attributes,
) {
	let { path } = location.resolve(".zgroup");
	let meta = await location.store.get(path);
	if (!meta) {
		throw new NodeNotFoundError("v2 group", {
			cause: new KeyError(path),
		});
	}
	VERSION_COUNTER.increment(location.store, "v2");
	return new Group(
		location.store,
		location.path,
		v2_to_v3_group_metadata(json_decode_object(meta), attrs),
	);
}

async function _open_v3<Store extends Readable>(location: Location<Store>) {
	let { store, path } = location.resolve("zarr.json");
	let meta = await location.store.get(path);
	if (!meta) {
		throw new NodeNotFoundError("v3 array or group", {
			cause: new KeyError(path),
		});
	}
	let meta_doc: ArrayMetadata<DataType> | GroupMetadata =
		json_decode_object(meta);
	if (meta_doc.node_type === "array") {
		meta_doc.fill_value = ensure_correct_scalar(meta_doc);
	}
	return meta_doc.node_type === "array"
		? new Array(store, location.path, meta_doc)
		: new Group(store, location.path, meta_doc);
}

function open_v3<Store extends Readable>(
	location: Location<Store> | Store,
	options: { kind: "group" },
): Promise<Group<Store>>;

function open_v3<Store extends Readable>(
	location: Location<Store> | Store,
	options: { kind: "array" },
): Promise<Array<DataType, Store>>;

function open_v3<Store extends Readable>(
	location: Location<Store> | Store,
): Promise<Array<DataType, Store> | Group<Store>>;

function open_v3<Store extends Readable>(
	location: Location<Store> | Store,
): Promise<Array<DataType, Store> | Group<Store>>;

async function open_v3<Store extends Readable>(
	location: Location<Store>,
	options: { kind?: "array" | "group" } = {},
): Promise<Array<DataType, Store> | Group<Store>> {
	let loc = "store" in location ? location : new Location(location);
	let node = await _open_v3(loc);
	VERSION_COUNTER.increment(loc.store, "v3");
	if (options.kind === undefined) return node;
	if (options.kind === "array" && node instanceof Array) return node;
	if (options.kind === "group" && node instanceof Group) return node;
	let kind = node instanceof Array ? "array" : "group";
	throw new Error(`Expected node of kind ${options.kind}, found ${kind}.`);
}

export function open<Store extends Readable>(
	location: Location<Store> | Store,
	options: { kind: "group" },
): Promise<Group<Store>>;

export function open<Store extends Readable>(
	location: Location<Store> | Store,
	options: { kind: "array" },
): Promise<Array<DataType, Store>>;

export async function open<Store extends Readable>(
	location: Location<Store> | Store,
	options: { kind?: "array" | "group" },
): Promise<Array<DataType, Store> | Group<Store>>;

export function open<Store extends Readable>(
	location: Location<Store> | Store,
): Promise<Array<DataType, Store> | Group<Store>>;

export function open<Store extends Readable>(
	location: Location<Store> | Store,
): Promise<Array<DataType, Store> | Group<Store>>;

export async function open<Store extends Readable>(
	location: Location<Store> | Store,
	options: { kind?: "array" | "group" } = {},
): Promise<Array<DataType, Store> | Group<Store>> {
	let store = "store" in location ? location.store : location;
	let version_max = VERSION_COUNTER.version_max(store);
	// Use the open function for the version with the most successful opens.
	// Note that here we use the dot syntax to access the open functions
	// because this enables us to use vi.spyOn during testing.
	let open_primary = version_max === "v2" ? open.v2 : open.v3;
	let open_secondary = version_max === "v2" ? open.v3 : open.v2;
	return open_primary(location, options).catch((err) => {
		rethrow_unless(err, NodeNotFoundError);
		return open_secondary(location, options);
	});
}

open.v2 = open_v2;
open.v3 = open_v3;

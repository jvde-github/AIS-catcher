import { Array, Group, Location } from "./hierarchy.js";
import { json_encode_object } from "./util.js";
export async function create(location, options = {}) {
    let loc = "store" in location ? location : new Location(location);
    if ("shape" in options) {
        let arr = await create_array(loc, options);
        return arr;
    }
    return create_group(loc, options);
}
async function create_group(location, options = {}) {
    let metadata = {
        zarr_format: 3,
        node_type: "group",
        attributes: options.attributes ?? {},
    };
    await location.store.set(location.resolve("zarr.json").path, json_encode_object(metadata));
    return new Group(location.store, location.path, metadata);
}
async function create_array(location, options) {
    let metadata = {
        zarr_format: 3,
        node_type: "array",
        shape: options.shape,
        data_type: options.data_type,
        chunk_grid: {
            name: "regular",
            configuration: {
                chunk_shape: options.chunk_shape,
            },
        },
        chunk_key_encoding: {
            name: "default",
            configuration: {
                separator: options.chunk_separator ?? "/",
            },
        },
        codecs: options.codecs ?? [],
        fill_value: options.fill_value ?? null,
        attributes: options.attributes ?? {},
    };
    await location.store.set(location.resolve("zarr.json").path, json_encode_object(metadata));
    return new Array(location.store, location.path, metadata);
}
//# sourceMappingURL=create.js.map
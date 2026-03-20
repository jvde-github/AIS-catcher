import { assert, get_strides, json_decode_object } from "../util.js";
// Reference: https://stackoverflow.com/a/21897413
function throw_on_nan_replacer(_key, value) {
    assert(!Number.isNaN(value), "JsonCodec allow_nan is false but NaN was encountered during encoding.");
    assert(value !== Number.POSITIVE_INFINITY, "JsonCodec allow_nan is false but Infinity was encountered during encoding.");
    assert(value !== Number.NEGATIVE_INFINITY, "JsonCodec allow_nan is false but -Infinity was encountered during encoding.");
    return value;
}
// Reference: https://gist.github.com/davidfurlong/463a83a33b70a3b6618e97ec9679e490
function sort_keys_replacer(_key, value) {
    return value instanceof Object && !Array.isArray(value)
        ? Object.keys(value)
            .sort()
            .reduce((sorted, key) => {
            sorted[key] = value[key];
            return sorted;
        }, {})
        : value;
}
export class JsonCodec {
    configuration;
    kind = "array_to_bytes";
    #encoder_config;
    #decoder_config;
    constructor(configuration = {}) {
        this.configuration = configuration;
        // Reference: https://github.com/zarr-developers/numcodecs/blob/0878717a3613d91a453fe3d3716aa9c67c023a8b/numcodecs/json.py#L36
        const { encoding = "utf-8", skipkeys = false, ensure_ascii = true, check_circular = true, allow_nan = true, sort_keys = true, indent, strict = true, } = configuration;
        let separators = configuration.separators;
        if (!separators) {
            // ensure separators are explicitly specified, and consistent behaviour across
            // Python versions, and most compact representation if indent is None
            if (!indent) {
                separators = [",", ":"];
            }
            else {
                separators = [", ", ": "];
            }
        }
        this.#encoder_config = {
            encoding,
            skipkeys,
            ensure_ascii,
            check_circular,
            allow_nan,
            indent,
            separators,
            sort_keys,
        };
        this.#decoder_config = { strict };
    }
    static fromConfig(configuration) {
        return new JsonCodec(configuration);
    }
    encode(buf) {
        const { indent, encoding, ensure_ascii, check_circular, allow_nan, sort_keys, } = this.#encoder_config;
        assert(encoding === "utf-8", "JsonCodec does not yet support non-utf-8 encoding.");
        const replacer_functions = [];
        // By default, for JSON.stringify,
        // a TypeError will be thrown if one attempts to encode an object with circular references
        assert(check_circular, "JsonCodec does not yet support skipping the check for circular references during encoding.");
        if (!allow_nan) {
            // Throw if NaN/Infinity/-Infinity are encountered during encoding.
            replacer_functions.push(throw_on_nan_replacer);
        }
        if (sort_keys) {
            // We can ensure keys are sorted but not really the opposite since
            // there is no guarantee of key ordering in JS.
            replacer_functions.push(sort_keys_replacer);
        }
        const items = Array.from(buf.data);
        items.push("|O");
        items.push(buf.shape);
        let replacer;
        if (replacer_functions.length) {
            replacer = (key, value) => {
                let new_value = value;
                for (let sub_replacer of replacer_functions) {
                    new_value = sub_replacer(key, new_value);
                }
                return new_value;
            };
        }
        let json_str = JSON.stringify(items, replacer, indent);
        if (ensure_ascii) {
            // If ensure_ascii is true (the default), the output is guaranteed
            // to have all incoming non-ASCII characters escaped.
            // If ensure_ascii is false, these characters will be output as-is.
            // Reference: https://stackoverflow.com/a/31652607
            json_str = json_str.replace(/[\u007F-\uFFFF]/g, (chr) => {
                const full_str = `0000${chr.charCodeAt(0).toString(16)}`;
                const sub_str = full_str.substring(full_str.length - 4);
                return `\\u${sub_str}`;
            });
        }
        return new TextEncoder().encode(json_str);
    }
    decode(bytes) {
        const { strict } = this.#decoder_config;
        // (i.e., allowing control characters inside strings)
        assert(strict, "JsonCodec does not yet support non-strict decoding.");
        const items = json_decode_object(bytes);
        const shape = items.pop();
        items.pop(); // Pop off dtype (unused)
        // O-d case
        assert(shape, "0D not implemented for JsonCodec.");
        const stride = get_strides(shape, "C");
        const data = items;
        return { data, shape, stride };
    }
}
//# sourceMappingURL=json2.js.map
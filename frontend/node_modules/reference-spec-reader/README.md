## reference-spec-reader

Experimental parser for [`ReferenceFileSystem` description](https://github.com/intake/fsspec-reference-maker).
This repository also provides a `ReferenceStore` implementation, a storage backend for
[`Zarr.js`](https://github.com/gzuidhof/zarr.js). An example of the V1 specification (JSON) is shown
below:

```json
{
  "version": 1,
  "templates": {
    "u": "server.domain/path",
    "f": "{{ c }}"
  },
  "gen": [
    {
      "key": "gen_key{{ i }}",
      "url": "http://{{ u }}_{{ i }}",
      "offset": "{{ (i + 1) * 1000 }}",
      "length": "1000",
      "dimensions": {
        "i": { "stop": 5 }
      }
    }
  ],
  "refs": {
    "key0": "data",
    "key1": ["http://target_url", 10000, 100],
    "key2": ["http://{{ u }}", 10000, 100],
    "key3": ["http://{{ f(c='text') }}", 10000, 100],
    "key4": ["http://{{ f(c='text') }}"]
  }
}
```

## API Reference

<a name="parse" href="#parse">#</a><b>parse</b>(<i>spec</i>[, <i>renderString</i>]) · [Source](https://github.com/manzt/reference-spec-reader/blob/master/src/parse.js)

Parses both `v0` and `v1` references into `Map<string, Ref>`. A `Ref` is a union type of the following:

- `string`: Inline ascii/base64 encoded data.
- `[url: string]`: A url for a whole file.
- `[url: string | null, offset: number, length: number]`: A tuple describing a binary section of a url.

```javascript
const spec = await fetch('http://localhost:8080/ref.json').then(res => res.json());
const ref = parse(spec);
console.log(ref);
// Map(9) {
//  'key0' => 'data',
//  'key1' => [ 'http://target_url', 10000, 100 ],
//  'key2' => [ 'http://server.domain/path', 10000, 100 ],
//  'key3' => [ 'http://text', 10000, 100 ],
//  'key4' => [ 'http://text' ],
//  'gen_key0' => [ 'http://server.domain/path_0', 1000, 1000 ],
//  'gen_key1' => [ 'http://server.domain/path_1', 2000, 1000 ],
//  'gen_key2' => [ 'http://server.domain/path_2', 3000, 1000 ],
//  'gen_key3' => [ 'http://server.domain/path_3', 4000, 1000 ],
//  'gen_key4' => [ 'http://server.domain/path_4', 5000, 1000 ]
// }
```

This library includes a minimal built-in `render` method to render jinja-templates included in the `v1` spec.
This method can be overriden by providing a custom `renderString` function as a second argument.

```javascript
import nunjucks from 'nunjucks';
const spec = await fetch('http://localhost:8080/ref.json').then(res => res.json());
const ref = parse(spec, nunjucks.renderString);
```

<a name="fromJSON" href="#fromJSON">#</a>
<em>ReferenceStore</em>.<b>fromJSON</b>(<i>data</i>, [, <i>options</i>]) · [Source](https://github.com/manzt/reference-spec-reader/blob/master/src/store.js)

A `Zarr.js` store reference implementation. Uses `fetch` API.

- _data_: A string in a supported JSON format, or a corresponding Object instance. Must adhere to `v0` or `v1` reference specification.
- _options_:
  - _target_: A default target url for the reference.
  - _renderString_: A custom `renderString` function.

```javascript
// create store from an input JSON string (v0 references).
ReferenceStore.fromJSON(`{"key0":"data","key1":"base64:aGVsbG8sIHdvcmxk"}`);
```

```javascript
// create a store from an input JSON string loaded from `url`
ReferenceStore.fromJSON(await fetch(url).then(res => res.text()));
```

```javascript
// create a store from an input JSON object loaded from `url`
ReferenceStore.fromJSON(await fetch(url).then(res => res.json()));
```

```javascript
// create a store from an input JSON object loaded from `url` with default binary target
const res = await fetch('http://localhost:8080/data.tif.json');
ReferenceStore.fromJSON(await res.json(), { target: 'http://localhost:8080/data.tif' });
```

## Usage

This package is written as a pure ES Module and is intended for use in various JavaScript
runtimes. It can be imported from a CDN via URL directly, or installed as a dependency
from `pnpm` for use in Node.js or via a bundler.

```javascript
// Browser or Deno via a CDN
import { ReferenceStore, parse } from 'https://cdn.skypack.dev/reference-spec-reader@<version>';

// Node.js or bundler
import { ReferenceStore, parse } from 'referece-spec-reader';
```

## Development

I welcome any input, feedback, bug reports, and contributions. This project requires Node.js
(version 12 or later) for running the test suite and linting/formating the code.

### Installation

```bash
cd reference-spec-reader
pnpm install
```

### Running tests

```bash
pnpm test # runs unit tests only
pnpm run test:ci # runs CI test suite (type-checking, linting, and unit tests)
```

> NOTE: If making a PR, please run `pnpm fmt` to auto-format your changes. The linting
> check will fail for unformatted code.

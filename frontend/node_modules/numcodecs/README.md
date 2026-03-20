# numcodecs.js
[![Actions Status](https://github.com/manzt/numcodecs.js/workflows/tests/badge.svg)](https://github.com/manzt/numcodecs.js/actions)
[![NPM badge](https://img.shields.io/npm/v/numcodecs)](https://www.npmjs.com/package/numcodecs)

Buffer compression and transformation codecs for use in [Zarr.js](https://github.com/gzuidhof/zarr.js/) and beyond...

### Installation

```bash
npm install numcodecs
```

### Usage

```javascript
import { Blosc, GZip, Zlib, LZ4, Zstd } from 'numcodecs';

const codec = new Blosc();
// or Blosc.fromConfig({ clevel: 5, cname: 'lz4', shuffle: Blosc.SHUFFLE, blocksize: 0 });

const size = 100000;
const arr = new Uint32Array(size);
for (let i = 0; i < size; i++) {
  arr[i] = i;
}

const bytes = new Uint8Array(arr.buffer);
console.log(bytes);
// Uint8Array(400000) [0, 0, 0, 0,  1, 0, 0, 0,  2, 0, 0, 0, ... ]

const encoded = await codec.encode(bytes);
console.log(encoded);
// Uint8Array(3744) [2, 1, 33, 4, 128, 26, 6, 0, 0, 0, 4, 0, ... ]

const decoded = await codec.decode(encoded);
console.log(new Uint32Array(decoded.buffer));
// Uint32Array(100000) [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,  ... ]
```

### Author's note

This project is an incomplete TypeScript implementation of the buffer compression library
[`numcodecs`](https://github.com/zarr-developers/numcodecs). The following codecs
are currently supported:

- `blosc`
- `gzip`
- `lz4`
- `zlib`
- `zstd`


### Package exports

Each compressor is bundled as the default export of separate code-split submodules.
This makes it easy to import each module independently in your applications or from
a ESM-friendly CDN like [skypack](https://www.skypack.dev/).

- Node / bundlers
```javascript
// Main entry point (exports all codecs)
import { Zlib } from 'numcodecs';

// Submodule entry point (exports only `zlib`)
import Zlib from 'numcodecs/zlib';
```

- Browser / Deno
```javascript
// Main entry point (exports all codecs)
import { Zlib } from 'https://cdn.skypack.dev/numcodecs';

// Submodule entry point (exports only `zlib`)
import Zlib from 'https://cdn.skypack.dev/numcodecs/zlib';
```

### Development

```bash
$ git clone https://github.com/manzt/numcodecs.js.git
$ cd numcodecs.js
$ npm install && npm run test
```

The `<codec_name>.js` + `<codec_name>.wasm` source for each WASM-based codec are
generated with [Docker](https://www.docker.com/) with the following commands:

```bash
cd codecs/<codec_name>
npm run build
```

### Changelogs

For changes to be reflected in package changelogs, run `npx changeset` and
follow the prompts.

> **Note** not every PR requires a changeset. Since changesets are focused on
> releases and changelogs, changes to the repository that don't effect these
> won't need a changeset (e.g., documentation, tests).

### Release

The [Changesets GitHub action](https://github.com/changesets/action) will create
and update a PR that applies changesets and publishes new versions.

# zarrita.js <a href="https://github.com/manzt/zarrita.js"><img align="right" src="https://raw.githubusercontent.com/manzt/zarrita.js/main/docs/public/logo.svg" height="38"></img></a>

[![NPM](https://img.shields.io/npm/v/zarrita/next.svg?color=black)](https://www.npmjs.com/package/zarrita)
[![License](https://img.shields.io/npm/l/zarrita.svg?color=black)](https://github.com/manzt/zarrita.js/raw/main/LICENSE)
![GitHub Actions](https://github.com/manzt/zarrita.js/actions/workflows/ci.yml/badge.svg)

a minimal & modular Zarr implementation in TypeScript

- **Zero dependencies** (optionally
  [`scijs/ndarray`](https://github.com/scijs/ndarray))
- Runs natively in **Node**, **Browsers**, and **Deno** (ESM)
- Supports **v2** or **v3** protocols, C & F-order arrays, diverse data-types,
  and [ZEP2 Sharding](https://zarr.dev/zeps/draft/ZEP0002.html)
- Allows flexible **storage** backends and **compression** codecs
- Provides rich, in-editor **type information** via
  [template literal types](https://www.typescriptlang.org/docs/handbook/2/template-literal-types.html)

## Installation

```sh
npm install zarrita
```

Read
[the documentation](https://manzt.github.io/zarrita.js/get-started.html#getting-started)
to learn more about other environments.

## Usage

```javascript
import * as zarr from "zarrita";

const store = new zarr.FetchStore("http://localhost:8080/data.zarr");
const arr = await zarr.open(store, { kind: "array" }); // zarr.Array<DataType, FetchStore>

// read chunk
const chunk = await arr.getChunk([0, 0]);

// Option 1: Builtin getter, no dependencies
const full = await zarr.get(arr); // { data: Int32Array, shape: number[], stride: number[] }

// Option 2: scijs/ndarray getter, includes `ndarray` and `ndarray-ops` dependencies
import { get } from "@zarrita/ndarray";
const full = await get(arr); // ndarray.Ndarray<Int32Array>

// read region
const region = await get(arr, [null, zarr.slice(6)]);
```

Read [the documentation](https://manzt.github.io/zarrita.js) to learn more.

// Puts the page together: index.html is the shell, and a feature's markup sits
// beside its code, included where the shell says <!-- @include tabs/x/x.html -->.
//     node assemble-html.mjs src/index.html dist/index.html
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';

const [src, out] = process.argv.slice(2);
const root = dirname(resolve(src));
// an included file may include in its turn
const include = (text) => text.replace(/^([ \t]*)<!-- @include (\S+) -->$/gm, (_, indent, file) =>
    include(readFileSync(resolve(root, file), 'utf8')).replace(/\n$/, '').split('\n').map((l) => (l ? indent + l : l)).join('\n'));
const page = include(readFileSync(src, 'utf8'));
if (/<!-- @include/.test(page)) throw new Error('an include was not resolved');
writeFileSync(out, page);

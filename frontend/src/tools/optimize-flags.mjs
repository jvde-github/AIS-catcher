// Replaces every flag SVG in <flags-dir> whose gzipped size exceeds THRESHOLD
// with an 80x60 palette PNG (emblem-heavy flags are ~10x smaller as PNG at the
// ~21 px render size) and prints the converted country codes, one per line.
//
// Usage: node tools/optimize-flags.mjs <flags-dir>

import { readdirSync, readFileSync, writeFileSync, unlinkSync } from 'fs';
import { join, basename } from 'path';
import { gzipSync } from 'zlib';
import sharp from 'sharp';

const THRESHOLD = 4096; // gzipped bytes
const WIDTH = 80, HEIGHT = 60;

const dir = process.argv[2];
if (!dir) {
    console.error('usage: node tools/optimize-flags.mjs <flags-dir>');
    process.exit(1);
}

for (const file of readdirSync(dir).filter((f) => f.endsWith('.svg')).sort()) {
    const path = join(dir, file);
    if (gzipSync(readFileSync(path), { level: 9 }).length <= THRESHOLD) continue;

    const png = await sharp(path, { density: 30 })
        .resize(WIDTH, HEIGHT)
        .png({ palette: true, compressionLevel: 9, effort: 10 })
        .toBuffer();

    writeFileSync(join(dir, basename(file, '.svg') + '.png'), png);
    unlinkSync(path);
    console.log(basename(file, '.svg'));
}

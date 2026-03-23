#!/usr/bin/env bash
set -euo pipefail

OUT="$(cd "$(dirname "$0")" && pwd)/dist"
NM="$(cd "$(dirname "$0")" && pwd)/node_modules"
SRC="$(cd "$(dirname "$0")" && pwd)/src"

rm -rf "$OUT"
mkdir -p "$OUT/flags"

echo "==> Building vendor bundle (libs.js + style.css)..."
./node_modules/.bin/vite build

echo "==> Minifying script.js (global scope preserved)..."
# terser without module wrapping so var/function declarations stay global
# (required for onclick="..." handlers in index.html)
./node_modules/.bin/terser "$SRC/script.js" \
    --compress --mangle \
    --output "$OUT/script.js"

echo "==> Copying static assets..."
cp "$SRC/index.html"  "$OUT/index.html"
cp "$SRC/favicon.ico" "$OUT/favicon.ico"
cp "$SRC/icons.png"   "$OUT/icons.png"

echo "==> Copying flag-icons (4x3 only)..."
cp "$NM/flag-icons/css/flag-icons.min.css" "$OUT/flags/"
cp -r "$NM/flag-icons/flags/4x3" "$OUT/flags/"

echo "==> Done. Output: $OUT"

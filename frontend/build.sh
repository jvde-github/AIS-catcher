#!/usr/bin/env bash
set -euo pipefail

OUT="$(cd "$(dirname "$0")" && pwd)/dist"
NM="$(cd "$(dirname "$0")" && pwd)/node_modules"
SRC="$(cd "$(dirname "$0")" && pwd)/src"

hash8() { md5sum "$1" | cut -c1-8; }

rm -rf "$OUT"
mkdir -p "$OUT/flags"

echo "==> Building vendor bundle (libs.js + bundle.css)..."
./node_modules/.bin/vite build

echo "==> Building app bundle (script.js ES module)..."
./node_modules/.bin/vite build --config vite.config.app.js

echo "==> Adding content hashes..."
LIBS_HASH=$(hash8 "$OUT/libs.js")
CSS_HASH=$(hash8  "$OUT/bundle.css")
JS_HASH=$(hash8   "$OUT/script.js")

mv "$OUT/libs.js"    "$OUT/libs.${LIBS_HASH}.js"
mv "$OUT/bundle.css" "$OUT/bundle.${CSS_HASH}.css"
mv "$OUT/script.js"  "$OUT/script.${JS_HASH}.js"

echo "==> Copying static assets..."
cp "$SRC/favicon.ico" "$OUT/favicon.ico"
cp "$SRC/icons.png"   "$OUT/icons.png"

sed \
    -e "s|href=\"bundle.css\"|href=\"bundle.${CSS_HASH}.css\"|" \
    -e "s|src=\"libs.js\"|src=\"libs.${LIBS_HASH}.js\"|" \
    -e "s|src=\"script.js\"|src=\"script.${JS_HASH}.js\"|" \
    "$SRC/index.html" > "$OUT/index.html"

echo "==> Copying flag-icons (4x3 only)..."
cp "$NM/flag-icons/css/flag-icons.min.css" "$OUT/flags/"
cp -r "$NM/flag-icons/flags/4x3" "$OUT/flags/"

echo "==> Done. Output: $OUT"

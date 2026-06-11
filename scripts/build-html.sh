#!/bin/bash

SRC=frontend/src
DIST=frontend/dist

# Function to perform sed replacement based on OS
perform_sed() {
    local file=$1
    local pattern=$2
    local replacement=$3

    if [[ "$OSTYPE" == "darwin"* ]]; then
        sed -i '' "$pattern$replacement" "$file"
    else
        sed -i "$pattern$replacement" "$file"
    fi
}

# Wipe Vite-emitted chunks from prior builds (content-hashed names would
# otherwise accumulate and bloat WebDB.cpp).
rm -f "$DIST"/lib-*.js "$DIST"/lib.css "$DIST"/script.js
rm -rf "$DIST/tabs"

(cd "$SRC" && npm install --include=dev && npm run build)

mkdir -p "$DIST"
cp "$SRC/style.css" "$DIST/style.css"
cp "$SRC/favicon.ico" "$DIST/favicon.ico"
cp "$SRC/icons.png" "$DIST/icons.png"
cp "$SRC/index.html" "$DIST/index.html"

# Generate flag-icons.css (fix relative paths: url(../flags/) → url(flags/))
sed 's|url(\.\./flags/|url(flags/|g' \
    "$SRC/node_modules/flag-icons/css/flag-icons.min.css" \
    > "$DIST/flag-icons.css"

# Copy only the 4x3 flag SVGs for country codes the app can emit (AIS MID
# table + ADS-B ICAO range table).
rm -rf "$DIST/flags"
mkdir -p "$DIST/flags/4x3"
{
    grep -oE '\{[0-9]+, "[^"]+", "[A-Za-z]{2}"\}' Source/JSON/JSONAIS.cpp \
        | sed -E 's/.*"([A-Za-z]{2})"\}/\1/'
    grep -oE "\{'[A-Z]', *'[A-Z]'\}" Source/Aviation/ADSB.cpp \
        | sed -E "s/\{'([A-Z])', *'([A-Z])'\}/\1\2/"
} | tr '[:upper:]' '[:lower:]' | sort -u | while read -r code; do
    svg="$SRC/node_modules/flag-icons/flags/4x3/$code.svg"
    if [ -f "$svg" ]; then
        cp "$svg" "$DIST/flags/4x3/"
    else
        echo "WARNING: no flag SVG for country code '$code'" >&2
    fi
done

# Rasterize emblem-heavy flags to small PNG files and point their .fi-xx CSS at them.
node "$SRC/tools/optimize-flags.mjs" "$DIST/flags/4x3" | while read -r code; do
    echo ".fi-$code{background-image:url(flags/4x3/$code.png)}" >> "$DIST/flag-icons.css"
done

# Generate hashes from dist file content
if [[ "$OSTYPE" == "darwin"* ]]; then
    CSS_HASH=$(md5 -q "$DIST/style.css")
    JS_HASH=$(md5 -q "$DIST/script.js")
    LIB_CSS_HASH=$(md5 -q "$DIST/lib.css")
    FLAG_CSS_HASH=$(md5 -q "$DIST/flag-icons.css")
else
    CSS_HASH=$(md5sum "$DIST/style.css" | cut -d' ' -f1)
    JS_HASH=$(md5sum "$DIST/script.js" | cut -d' ' -f1)
    LIB_CSS_HASH=$(md5sum "$DIST/lib.css" | cut -d' ' -f1)
    FLAG_CSS_HASH=$(md5sum "$DIST/flag-icons.css" | cut -d' ' -f1)
fi

# Update hashes in dist/index.html
perform_sed "$DIST/index.html" "s|lib\.css?hash=[^\"]*|lib.css?hash=${LIB_CSS_HASH}|g" ''
perform_sed "$DIST/index.html" "s|flag-icons\.css?hash=[^\"]*|flag-icons.css?hash=${FLAG_CSS_HASH}|g" ''
perform_sed "$DIST/index.html" "s|style\.css?hash=[^\"]*|style.css?hash=${CSS_HASH}|g" ''
perform_sed "$DIST/index.html" "s|script\.js?hash=[^\"]*|script.js?hash=${JS_HASH}|g" ''


echo "Built frontend/dist — baking into WebDB..."
./scripts/build-web-db.sh "$DIST"

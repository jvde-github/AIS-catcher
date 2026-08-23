#!/bin/bash

SRC=frontend/src
SHARED=frontend/shared
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
rm -f "$DIST"/lib-*.js "$DIST"/lib.css "$DIST"/script.js "$DIST"/components.js "$DIST"/chrome.js "$DIST"/settings.js "$DIST"/tokens.css "$DIST"/icons.css "$DIST"/sprites.css "$DIST"/components.css "$DIST"/map.css
rm -rf "$DIST/tabs"

(cd "$SRC" && npm install --include=dev && npm run build)

# One stylesheet per host from the shared sheets, in index.css order; the
# shared JS is bundled by Vite (viewer) or loaded as a module (hub)
# the order is index.css's; a host that needs only a prefix of it passes a count
shared_css() {
    grep -o '"\./[^"]*"' "$SHARED/index.css" | sed 's|^"\./||; s|"$||' | head -n "${1:-99}" | while read -r f; do cat "$SHARED/$f"; echo; done
}

mkdir -p "$DIST"
shared_css > "$DIST/shared.css"
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

# Everything Vite does not bundle is minified in place; the sources stay readable
minify() {
    local f="$1"; shift
    "$SRC/node_modules/.bin/esbuild" "$f" --minify --log-level=warning --charset=utf8 "$@" --outfile="$f.min" && mv "$f.min" "$f"
}

file_hash() {
    if [[ "$OSTYPE" == "darwin"* ]]; then md5 -q "$1"; else md5sum "$1" | cut -d' ' -f1; fi
}

for f in shared.css style.css flag-icons.css; do minify "$DIST/$f"; done

# Cache-bust viewer assets. Anchored on the opening quote: an unanchored
# "icons.css" would also match "flag-icons.css"
for f in shared.css style.css script.js lib.css flag-icons.css; do
    perform_sed "$DIST/index.html" "s|\"${f//./\\.}?hash=[^\"]*|\"${f}?hash=$(file_hash "$DIST/$f")|g" ''
done


# Control hub UI (managed mode -E) — plain static files served by the
# control server under the control/ prefix
rm -rf "$DIST/control"
cp -R frontend/control "$DIST/control"
mkdir -p "$DIST/control/css"
# same tokens.css the viewer gets; the control server prefixes every path with
# "control", so the hub cannot reach the viewer's copy — it needs its own
shared_css 4 > "$DIST/control/css/shared.css"
cp "$SHARED/components.js" "$DIST/control/js/components.js"

minify "$DIST/control/css/shared.css"
minify "$DIST/control/js/components.js" --format=esm
minify "$DIST/control/js/shared-globals.js" --format=esm
for f in schema.js config-manager.js wizard.js app.js; do minify "$DIST/control/js/$f"; done

# Cache-bust hub assets (served with a 1-year cache header, like the viewer's)
for f in css/shared.css js/components.js js/shared-globals.js js/schema.js js/config-manager.js js/wizard.js js/app.js; do
    HASH=$(file_hash "$DIST/control/$f")
    # full path: an unanchored "icons.css" would also match "flag-icons.css"
    perform_sed "$DIST/control/index.html" "s|\"${f}?hash=[^\"]*|\"${f}?hash=${HASH}|g" ''
    perform_sed "$DIST/control/js/shared-globals.js" "s|\"\./${f#js/}?hash=[^\"]*|\"./${f#js/}?hash=${HASH}|g" ''
done

echo "Built frontend/dist — baking into WebDB..."
./scripts/build-web-db.sh "$DIST"

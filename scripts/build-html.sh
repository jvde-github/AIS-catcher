#!/bin/bash
set -e

FRONTEND=frontend
SRC=frontend/src
UI_CSS=frontend/packages/ui/css
NM=frontend/node_modules
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
rm -f "$DIST"/lib-*.js "$DIST"/lib.css "$DIST"/shared.css "$DIST"/style.css "$DIST"/script.js "$DIST"/control-shared.js "$DIST"/components.js "$DIST"/chrome.js "$DIST"/settings.js "$DIST"/tokens.css "$DIST"/icons.css "$DIST"/sprites.css "$DIST"/components.css "$DIST"/map.css
rm -rf "$DIST/tabs"

# install only when the lockfile is newer than node_modules
if [ ! -d "$NM" ] || [ "$FRONTEND/package-lock.json" -nt "$NM/.package-lock.json" ]; then
    (cd "$FRONTEND" && npm install --include=dev --prefer-offline --no-audit --no-fund)
fi
(cd "$SRC" && npm run build)

# The viewer's stylesheets are imports, bundled into lib.css by Vite. The hub
# takes the first sheets of the ui package concatenated in index.css order; a
# host that needs only a prefix of it passes a count
shared_css() {
    grep -o '"\./[^"]*"' "$UI_CSS/index.css" | sed 's|^"\./||; s|"$||' | head -n "${1:-99}" | while read -r f; do cat "$UI_CSS/$f"; echo; done
}

mkdir -p "$DIST"
cp "$SRC/favicon.ico" "$DIST/favicon.ico"
# icons.png is the pre-coloured sheet KML, GeoJSON and outside pages load; the map draws from the tinted one
cp "$SRC/icons.png" "$DIST/icons.png"
cp "$SRC/icons-tint.png" "$DIST/icons-tint.png"
cp "$SRC/icons-tint-2x.png" "$DIST/icons-tint-2x.png"
# the page: the shell plus each feature's markup from its folder
node "$SRC/tools/assemble-html.mjs" "$SRC/index.html" "$DIST/index.html"

# Generate flag-icons.css (fix relative paths: url(../flags/) → url(flags/))
sed 's|url(\.\./flags/|url(flags/|g' \
    "$NM/flag-icons/css/flag-icons.min.css" \
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
    svg="$NM/flag-icons/flags/4x3/$code.svg"
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

# Everything Vite does not bundle is minified in place; the sources stay readable.
# Keep Unicode escaped so CSS spacers (such as flag-icons' NBSP) cannot show as
# stray characters when a browser decodes a stylesheet with a legacy charset.
minify() {
    local f="$1"; shift
    "$NM/.bin/esbuild" "$f" --minify --log-level=warning --charset=ascii "$@" --outfile="$f.min" && mv "$f.min" "$f"
}

file_hash() {
    if [[ "$OSTYPE" == "darwin"* ]]; then md5 -q "$1"; else md5sum "$1" | cut -d' ' -f1; fi
}

# lib.css again: Vite minified it, this keeps non-ASCII escaped like the others
for f in lib.css flag-icons.css; do minify "$DIST/$f"; done

# Cache-bust viewer assets. Anchored on the opening quote: an unanchored
# "icons.css" would also match "flag-icons.css"
for f in script.js lib.css flag-icons.css; do
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

minify "$DIST/control/css/shared.css"
minify "$DIST/control/css/locations.css"
minify "$DIST/control/css/settings.css"

# Cache-bust hub assets (served with a 1-year cache header, like the viewer's)
for f in css/shared.css css/locations.css css/settings.css; do
    HASH=$(file_hash "$DIST/control/$f")
    # full path: an unanchored "icons.css" would also match "flag-icons.css"
    perform_sed "$DIST/control/index.html" "s|\"${f}?hash=[^\"]*|\"${f}?hash=${HASH}|g" ''
done

# The hub is one bundle, control-app.js; its sources under control/js are all in it.
HASH=$(file_hash "$DIST/control-app.js")
perform_sed "$DIST/control/index.html" "s|control-app.js?hash=[^\"]*|control-app.js?hash=${HASH}|g" ''
rm -rf "$DIST/control/js" "$DIST/places-editor.js"
echo "Built frontend/dist — baking into WebDB..."
./scripts/build-web-db.sh "$DIST"

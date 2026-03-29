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

# Build the npm/vite library bundle → frontend/dist/lib.js + lib.css
(cd "$SRC" && npm install --include=dev && npm run build)

# Populate dist from src (static assets + hand-written files)
mkdir -p "$DIST"
cp "$SRC/script.js" "$DIST/script.js"
cp "$SRC/style.css" "$DIST/style.css"
cp "$SRC/favicon.ico" "$DIST/favicon.ico"
cp "$SRC/icons.png" "$DIST/icons.png"
cp "$SRC/index.html" "$DIST/index.html"

# Generate flag-icons.css (fix relative paths: url(../flags/) → url(flags/))
sed 's|url(\.\./flags/|url(flags/|g' \
    "$SRC/node_modules/flag-icons/css/flag-icons.min.css" \
    > "$DIST/flag-icons.css"

# Copy 4x3 flag SVGs into dist for WebDB embedding
rm -rf "$DIST/flags"
mkdir "$DIST/flags"
cp -r "$SRC/node_modules/flag-icons/flags/4x3" "$DIST/flags/4x3"

# Generate hashes from dist file content
if [[ "$OSTYPE" == "darwin"* ]]; then
    CSS_HASH=$(md5 -q "$DIST/style.css")
    JS_HASH=$(md5 -q "$DIST/script.js")
    LIB_JS_HASH=$(md5 -q "$DIST/lib.js")
    LIB_CSS_HASH=$(md5 -q "$DIST/lib.css")
    FLAG_CSS_HASH=$(md5 -q "$DIST/flag-icons.css")
else
    CSS_HASH=$(md5sum "$DIST/style.css" | cut -d' ' -f1)
    JS_HASH=$(md5sum "$DIST/script.js" | cut -d' ' -f1)
    LIB_JS_HASH=$(md5sum "$DIST/lib.js" | cut -d' ' -f1)
    LIB_CSS_HASH=$(md5sum "$DIST/lib.css" | cut -d' ' -f1)
    FLAG_CSS_HASH=$(md5sum "$DIST/flag-icons.css" | cut -d' ' -f1)
fi

# Update hashes in dist/index.html
perform_sed "$DIST/index.html" "s|lib\.js?hash=[^\"]*|lib.js?hash=${LIB_JS_HASH}|g" ''
perform_sed "$DIST/index.html" "s|lib\.css?hash=[^\"]*|lib.css?hash=${LIB_CSS_HASH}|g" ''
perform_sed "$DIST/index.html" "s|flag-icons\.css?hash=[^\"]*|flag-icons.css?hash=${FLAG_CSS_HASH}|g" ''
perform_sed "$DIST/index.html" "s|style\.css?hash=[^\"]*|style.css?hash=${CSS_HASH}|g" ''
perform_sed "$DIST/index.html" "s|script\.js?hash=[^\"]*|script.js?hash=${JS_HASH}|g" ''


echo "Built frontend/dist — baking into WebDB..."
./scripts/build-web-db.sh "$DIST"

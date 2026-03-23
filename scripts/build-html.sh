#!/bin/bash

# Function to perform sed replacement based on OS
perform_sed() {
    local file=$1
    local pattern=$2
    local replacement=$3

    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS (BSD) version
        sed -i '' "$pattern$replacement" "$file"
    else
        # Linux (GNU) version
        sed -i "$pattern$replacement" "$file"
    fi
}

# Build the vendor bundle (CDN dependencies + style.css bundled locally)
npm --prefix frontend install --include=dev --silent
npm --prefix frontend run build

# Copy static assets from source
mkdir -p Source/HTML
cp frontend/src/index.html Source/HTML/index.html
cp frontend/src/favicon.ico Source/HTML/favicon.ico
cp frontend/src/icons.png Source/HTML/icons.png

# Copy flag-icons CSS + SVGs directly (Vite would inline all 540 SVGs, so we copy manually)
rm -rf Source/HTML/flags
mkdir -p Source/HTML/flags
cp frontend/node_modules/flag-icons/css/flag-icons.min.css Source/HTML/flags/
cp -r frontend/node_modules/flag-icons/flags/4x3 Source/HTML/flags/

# Generate hashes from actual file content
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    JS_HASH=$(md5 -q Source/HTML/script.js)
    CSS_HASH=$(md5 -q Source/HTML/style.css)
    LIBS_HASH=$(md5 -q Source/HTML/libs.js)
else
    # Linux
    JS_HASH=$(md5sum Source/HTML/script.js | cut -d' ' -f1)
    CSS_HASH=$(md5sum Source/HTML/style.css | cut -d' ' -f1)
    LIBS_HASH=$(md5sum Source/HTML/libs.js | cut -d' ' -f1)
fi

# Update the hashes in index.html
perform_sed "Source/HTML/index.html" "s|script\.js?hash=[^\"]*|script.js?hash=${JS_HASH}|g" ''
perform_sed "Source/HTML/index.html" "s|style\.css?hash=[^\"]*|style.css?hash=${CSS_HASH}|g" ''
perform_sed "Source/HTML/index.html" "s|libs\.js?hash=[^\"]*|libs.js?hash=${LIBS_HASH}|g" ''

echo "Updated index.html with versioned script and vendor references"
./scripts/build-web-db.sh Source/HTML

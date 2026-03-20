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

# Copy flag-icons CSS + SVGs directly (Vite would inline all 540 SVGs, so we copy manually)
mkdir -p Source/HTML/flags
cp frontend/node_modules/flag-icons/css/flag-icons.min.css Source/HTML/flags/
cp -r frontend/node_modules/flag-icons/flags/4x3 Source/HTML/flags/
cp -r frontend/node_modules/flag-icons/flags/1x1 Source/HTML/flags/

# Generate hashes from actual file content
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    JS_HASH=$(md5 -q Source/HTML/script.js)
    VENDOR_CSS_HASH=$(md5 -q Source/HTML/vendor.css)
    VENDOR_JS_HASH=$(md5 -q Source/HTML/vendor.js)
else
    # Linux
    JS_HASH=$(md5sum Source/HTML/script.js | cut -d' ' -f1)
    VENDOR_CSS_HASH=$(md5sum Source/HTML/vendor.css | cut -d' ' -f1)
    VENDOR_JS_HASH=$(md5sum Source/HTML/vendor.js | cut -d' ' -f1)
fi

# Update the hashes in index.html
perform_sed "Source/HTML/index.html" "s|script\.js?hash=[^\"]*|script.js?hash=${JS_HASH}|g" ''
perform_sed "Source/HTML/index.html" "s|vendor\.css?hash=[^\"]*|vendor.css?hash=${VENDOR_CSS_HASH}|g" ''
perform_sed "Source/HTML/index.html" "s|vendor\.js?hash=[^\"]*|vendor.js?hash=${VENDOR_JS_HASH}|g" ''

echo "Updated index.html with versioned script and vendor references"
./scripts/build-web-db.sh Source/HTML

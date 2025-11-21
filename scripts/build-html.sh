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

# Generate hashes from actual file content
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    CSS_HASH=$(md5 -q Source/HTML/style.css)
    JS_HASH=$(md5 -q Source/HTML/script.js)
else
    # Linux
    CSS_HASH=$(md5sum Source/HTML/style.css | cut -d' ' -f1)
    JS_HASH=$(md5sum Source/HTML/script.js | cut -d' ' -f1)
fi

# Update the hashes in index.html
perform_sed "Source/HTML/index.html" "s|style\.css?hash=[^\"]*|style.css?hash=${CSS_HASH}|g" ''
perform_sed "Source/HTML/index.html" "s|script\.js?hash=[^\"]*|script.js?hash=${JS_HASH}|g" ''

# Create a local version and perform the same replacement
if [[ "$OSTYPE" == "darwin"* ]]; then
    sed -e 's|https://cdn.jsdelivr.net/|cdn/|g' -e 's|https://unpkg.com/|cdn/|g' Source/HTML/index.html > Source/HTML/index_local.html
else
    sed -e 's|https://cdn.jsdelivr.net/|cdn/|g' -e 's|https://unpkg.com/|cdn/|g' Source/HTML/index.html > Source/HTML/index_local.html
fi

echo "Updated index.html with versioned script and style references"
./scripts/build-web-db.sh Source/HTML
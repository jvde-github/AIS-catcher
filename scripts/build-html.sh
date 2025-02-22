#!/bin/bash

# Ensure a tag is provided as an argument or use the current timestamp if not provided
if [ -z "$1" ]; then
  TAG=$(date +%s)
else
  TAG=$1
fi

# Create URL-safe version of the tag
URL_SAFE_TAG=$(echo "$TAG" | sed 's/[.]/_/g' | sed 's/[-]/_/g')

# Output the tag and the URL-safe tag
echo "Tag: $TAG"
echo "URL-safe Tag: $URL_SAFE_TAG"

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

# Update Application/version.h
perform_sed "Application/version.h" '/VERSION_DESCRIBE/d' ''
echo -e "#define VERSION_DESCRIBE\t\"$TAG\"" >> Application/version.h
perform_sed "Application/version.h" '/VERSION_URL_TAG/d' ''
echo -e "#define VERSION_URL_TAG\t\"$URL_SAFE_TAG\"" >> Application/version.h

# Generate hashes from actual file content
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    CSS_HASH=$(md5 -q HTML/style.css)
    JS_HASH=$(md5 -q HTML/script.js)
else
    # Linux
    CSS_HASH=$(md5sum HTML/style.css | cut -d' ' -f1)
    JS_HASH=$(md5sum HTML/script.js | cut -d' ' -f1)
fi

# Update the hashes in index.html
perform_sed "HTML/index.html" "s|style\.css?hash=[^\"]*|style.css?hash=${CSS_HASH}|g" ''
perform_sed "HTML/index.html" "s|script\.js?hash=[^\"]*|script.js?hash=${JS_HASH}|g" ''

# Create a local version and perform the same replacement
if [[ "$OSTYPE" == "darwin"* ]]; then
    sed -e 's|https://cdn.jsdelivr.net/|cdn/|g' -e 's|https://unpkg.com/|cdn/|g' HTML/index.html > HTML/index_local.html
else
    sed -e 's|https://cdn.jsdelivr.net/|cdn/|g' -e 's|https://unpkg.com/|cdn/|g' HTML/index.html > HTML/index_local.html
fi

echo "Updated Application/version.h with VERSION_DESCRIBE and VERSION_URL_TAG"
echo "Updated index.html with versioned script and style references"
./scripts/build-web-db.sh HTML
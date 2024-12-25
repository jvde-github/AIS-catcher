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

# Update Application/version.h
sed -i '/VERSION_DESCRIBE/d' Application/version.h
echo -e "#define VERSION_DESCRIBE\t\"$TAG\"" >> Application/version.h
sed -i '/VERSION_URL_TAG/d' Application/version.h
echo -e "#define VERSION_URL_TAG\t\"$URL_SAFE_TAG\"" >> Application/version.h

# Generate hashes from actual file content
CSS_HASH=$(md5sum HTML/style.css | cut -d' ' -f1)
JS_HASH=$(md5sum HTML/script.js | cut -d' ' -f1)

# Update the hashes in index.html
sed -i "s|style\.css?hash=[^\"]*|style.css?hash=${CSS_HASH}|g" HTML/index.html
sed -i "s|script\.js?hash=[^\"]*|script.js?hash=${JS_HASH}|g" HTML/index.html

# Create a local version and perform the same replacement
sed -e 's|https://cdn.jsdelivr.net/|cdn/|g' -e 's|https://unpkg.com/|cdn/|g' HTML/index.html > HTML/index_local.html

echo "Updated Application/version.h with VERSION_DESCRIBE and VERSION_URL_TAG"
echo "Updated index.html with versioned script and style references"


./scripts/build-web-db.sh HTML


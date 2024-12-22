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

# Replace script.js and style.css with their versioned counterparts in index.html
sed -i "s|script_[^\.]*\.js|script_${URL_SAFE_TAG}.js|g" HTML/index.html
sed -i "s|style_[^\.]*\.css|style_${URL_SAFE_TAG}.css|g" HTML/index.html

# Alternatively, if there are also unversioned occurrences, include those as well:
sed -i "s|script\.js|script_${URL_SAFE_TAG}.js|g" HTML/index.html
sed -i "s|style\.css|style_${URL_SAFE_TAG}.css|g" HTML/index.html

# Create a local version and perform the same replacement
sed -e 's|https://cdn.jsdelivr.net/|cdn/|g' -e 's|https://unpkg.com/|cdn/|g' HTML/index.html > HTML/index_local.html

echo "Updated Application/version.h with VERSION_DESCRIBE and VERSION_URL_TAG"
echo "Updated index.html with versioned script and style references"

# Minify and compress index.html and generate corresponding C++ files
cd HTML

rm index_html.cpp index_local_html.cpp style_css.cpp script_js.cpp
minify index.html | gzip > index_html_gz
xxd -i index_html_gz > index_html.cpp

minify index_local.html | gzip > index_local_html_gz
xxd -i index_local_html_gz > index_local_html.cpp

cat script.js | gzip > script_js_gz
xxd -i script_js_gz > script_js.cpp

minify style.css | gzip > style_css_gz
xxd -i style_css_gz > style_css.cpp

cd ..

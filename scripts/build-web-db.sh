#!/bin/bash

# Check if directory argument is provided, otherwise use current directory
BASE_DIR="${1:-.}"
OUTPUT_FILE="Source/Application/WebDB.cpp"

# Remove existing output file if it exists
if [ -f "$OUTPUT_FILE" ]; then
    rm "$OUTPUT_FILE"
fi

# Write header includes
cat > "$OUTPUT_FILE" << 'EOF'
#include "WebDB.h"

namespace WebDB
{

  std::unordered_map<std::string, FileData> files;

EOF

# Function to convert filename to valid C identifier
make_identifier() {
    echo "$1" | sed 's/[^a-zA-Z0-9]/_/g' | sed 's/^[0-9]/_&/'
}

# Function to get MIME type based on extension (portable: no ${ext,,})
get_mime_type() {
    local file="$1"
    local ext="${file##*.}"
    local ext_lower
    ext_lower=$(echo "$ext" | tr '[:upper:]' '[:lower:]')
    case "$ext_lower" in
        "html"|"htm") echo "text/html" ;;
        "css")        echo "text/css; charset=utf-8" ;;
        "js")         echo "application/javascript; charset=utf-8" ;;
        "json")       echo "application/json; charset=utf-8" ;;
        "txt")        echo "text/plain; charset=utf-8" ;;
        "md")         echo "text/markdown; charset=utf-8" ;;
        "xml")        echo "application/xml; charset=utf-8" ;;
        "svg")        echo "image/svg+xml; charset=utf-8" ;;
        "jpg"|"jpeg") echo "image/jpeg" ;;
        "png")        echo "image/png" ;;
        "gif")        echo "image/gif" ;;
        "ico")        echo "image/x-icon" ;;
        "webp")       echo "image/webp" ;;
        *)            echo "application/octet-stream" ;;
    esac
}

# Portable relative path: strip BASE_DIR prefix from absolute file path
relative_to_base() {
    echo "${1#$BASE_DIR/}"
}

# Create a temporary file for the compressed content
TEMP_FILE=$(mktemp)

# Process each file
process_file() {
    local file="$1"
    local relative_path
    relative_path=$(relative_to_base "$file")

    # Skip the output file itself and files that didn't strip (outside BASE_DIR)
    if [ "$relative_path" = "$file" ] || [ "$relative_path" = "$OUTPUT_FILE" ]; then
        return
    fi

    echo "Processing: $relative_path"

    # Create C-friendly identifier
    local identifier
    identifier=$(make_identifier "$relative_path")
    local mime_type
    mime_type=$(get_mime_type "$file")

    # Compress the file content
    gzip -9 -n < "$file" > "$TEMP_FILE"

    # Generate the C array data (sed '$d' removes last line, portable alternative to head -n -1)
    {
        echo "// File: $relative_path"
        echo "static const unsigned char data_${identifier}[] = {"
        xxd -i < "$TEMP_FILE" | grep -v "unsigned" | grep -v "^};"
        echo "};"
        echo "static const size_t size_${identifier} = sizeof(data_${identifier});"
        echo
    } >> "$OUTPUT_FILE"
}

# Export functions and variables
export -f process_file
export -f make_identifier
export -f get_mime_type
export -f relative_to_base
export OUTPUT_FILE
export BASE_DIR
export TEMP_FILE

# Resolve BASE_DIR to absolute path (portable: use pwd -P fallback if realpath unavailable)
BASE_DIR=$(cd "$BASE_DIR" && pwd -P)
echo "Using base directory: $BASE_DIR"

# Find all files recursively and process them
find "$BASE_DIR" -type f -exec bash -c 'process_file "$0"' {} \;

# Remove temporary file
rm -f "$TEMP_FILE"

# Write the initialization function
cat >> "$OUTPUT_FILE" << 'EOF'

void initialize() {
EOF

# Add each file to the map
find "$BASE_DIR" -type f | while read -r file; do
    relative_path=$(relative_to_base "$file")
    # Skip output file and files outside BASE_DIR
    if [ "$relative_path" = "$file" ] || [ "$relative_path" = "$OUTPUT_FILE" ]; then
        continue
    fi
    identifier=$(make_identifier "$relative_path")
    mime_type=$(get_mime_type "$file")
    echo "    files.emplace(\"${relative_path}\", FileData(data_${identifier}, size_${identifier}, \"${mime_type}\"));" >> "$OUTPUT_FILE"
done

# Close the initialization function and namespace
cat >> "$OUTPUT_FILE" << 'EOF'
}

struct Init {
    Init() {
        initialize();
    }
} init;

} // namespace WebDB
EOF

echo "Header file generated successfully at $OUTPUT_FILE"

# Print some statistics
echo "Statistics:"
echo "Total files processed: $(find "$BASE_DIR" -type f | wc -l)"
echo "Output file size: $(stat -f %z "$OUTPUT_FILE" 2>/dev/null || stat -c %s "$OUTPUT_FILE") bytes"

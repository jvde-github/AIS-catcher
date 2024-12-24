#!/bin/bash

# Check if directory argument is provided, otherwise use current directory
BASE_DIR="${1:-.}"
DB_FILE="web.db"

# Remove existing database if it exists
if [ -f "$DB_FILE" ]; then
    rm "$DB_FILE"
fi

# Create the SQLite database and table with optimizations
sqlite3 "$DB_FILE" <<EOF
PRAGMA page_size=4096;
PRAGMA encoding='UTF-8';
PRAGMA journal_mode=OFF;
PRAGMA synchronous=OFF;
PRAGMA locking_mode=EXCLUSIVE;
CREATE TABLE files (
    path TEXT PRIMARY KEY,
    content BLOB,
    mime_type TEXT,
    timestamp INTEGER,
    original_size INTEGER
);
EOF

# Function to get MIME type based on extension
get_mime_type() {
    local file="$1"
    local ext="${file##*.}"
    case "${ext,,}" in
        # Text files
        "html"|"htm") mime_type="text/html" ;;
        "css")        mime_type="text/css" ;;
        "js")         mime_type="application/javascript" ;;
        "json")       mime_type="application/json" ;;
        "txt")        mime_type="text/plain" ;;
        "md")         mime_type="text/markdown" ;;
        "xml")        mime_type="application/xml" ;;
        "svg")        mime_type="image/svg+xml" ;;
        # Images
        "jpg"|"jpeg") mime_type="image/jpeg" ;;
        "png")        mime_type="image/png" ;;
        "gif")        mime_type="image/gif" ;;
        "ico")        mime_type="image/x-icon" ;;
        "webp")       mime_type="image/webp" ;;
        # Default
        *)            mime_type="application/octet-stream" ;;
    esac
    echo "$mime_type"
}

# Check if a MIME type is text-based
is_text_mime() {
    local mime="$1"
    case "$mime" in
        text/*|application/javascript|application/json|application/xml|image/svg+xml)
            return 0 ;;
        *)
            return 1 ;;
    esac
}

# Create a temporary file for the compressed content
TEMP_FILE=$(mktemp)

# Process each file
process_file() {
    local file="$1"
    # Calculate relative path from BASE_DIR
    local relative_path=$(realpath --relative-to="$BASE_DIR" "$file")
    
    # Skip the database file itself and files outside BASE_DIR
    if [ "$relative_path" = "$DB_FILE" ] || [[ $relative_path == ../* ]]; then
        return
    fi

    echo "Processing: $relative_path"
    
    # Get file metadata
    local mime_type=$(get_mime_type "$file")
    local timestamp=$(stat -c %Y "$file")
    local original_size=$(stat -c %s "$file")

    # Compress the file content
    if is_text_mime "$mime_type"; then
        # For text files, normalize line endings and compress
        tr -d '\r' < "$file" | gzip -9 -n > "$TEMP_FILE"
    else
        # For binary files, just compress
        gzip -9 -n < "$file" > "$TEMP_FILE"
    fi

    # Insert into database using SQLite's readfile() for binary data
    sqlite3 "$DB_FILE" <<EOF
INSERT INTO files (path, content, mime_type, timestamp, original_size)
SELECT
    '$relative_path',
    readfile('$TEMP_FILE'),
    '$mime_type',
    $timestamp,
    $original_size;
EOF
}

# Export functions and variables
export -f process_file
export DB_FILE
export -f get_mime_type
export -f is_text_mime
export TEMP_FILE
export BASE_DIR

# Resolve BASE_DIR to absolute path
BASE_DIR=$(realpath "$BASE_DIR")
echo "Using base directory: $BASE_DIR"

# Find all files recursively and process them
find "$BASE_DIR" -type f -exec bash -c 'process_file "$0"' {} \;

# Remove temporary file
rm -f "$TEMP_FILE"

# Create index and optimize
sqlite3 "$DB_FILE" <<EOF
CREATE INDEX idx_path ON files(path);
VACUUM;
ANALYZE;
EOF

echo "Database created successfully at $DB_FILE"

# Print some statistics
echo "Statistics:"
sqlite3 "$DB_FILE" <<EOF
SELECT
    COUNT(*) as 'Total Files',
    ROUND(SUM(length(content))/1024.0/1024.0, 2) as 'Compressed Size (MB)',
    ROUND(SUM(original_size)/1024.0/1024.0, 2) as 'Original Size (MB)',
    ROUND((1 - SUM(length(content))*1.0/SUM(original_size))*100, 1) as 'Space Saved %'
FROM files;

SELECT mime_type, COUNT(*) as count,
    ROUND(SUM(length(content))/1024.0, 1) as 'KB Compressed',
    ROUND(SUM(original_size)/1024.0, 1) as 'KB Original'
FROM files
GROUP BY mime_type
ORDER BY count DESC;
EOF
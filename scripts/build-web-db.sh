#!/usr/bin/env bash
# Generates Source/Application/WebDB.cpp from files in Source/HTML/.
# Usage: bash scripts/build-web-db.sh [repo-root]
set -euo pipefail

REPO_ROOT="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
HTML_DIR="$REPO_ROOT/frontend/dist"
OUTPUT="$REPO_ROOT/Source/Application/WebDB.cpp"

if [ ! -d "$HTML_DIR" ]; then
    echo "Error: $HTML_DIR not found. Run 'cd frontend && npm run build' first." >&2
    exit 1
fi

TEMP=$(mktemp)
trap 'rm -f "$TEMP"' EXIT

# Convert filename to valid C identifier
make_id() { echo "$1" | sed 's/[^a-zA-Z0-9]/_/g' | sed 's/^[0-9]/_&/'; }

# MIME type from file extension (portable lowercase via tr)
mime_type() {
    local ext
    ext=$(echo "${1##*.}" | tr '[:upper:]' '[:lower:]')
    case "$ext" in
        html|htm) echo "text/html" ;;
        css)      echo "text/css; charset=utf-8" ;;
        js)       echo "application/javascript; charset=utf-8" ;;
        json)     echo "application/json; charset=utf-8" ;;
        svg)      echo "image/svg+xml; charset=utf-8" ;;
        png)      echo "image/png" ;;
        ico)      echo "image/x-icon" ;;
        *)        echo "application/octet-stream" ;;
    esac
}

cat > "$OUTPUT" <<'EOF'
#include "WebDB.h"

namespace WebDB
{

  std::unordered_map<std::string, FileData> files;

EOF

# Collect all files, sorted for deterministic output (portable: no mapfile)
FILES_LIST=$(find "$HTML_DIR" -type f | sort)

while IFS= read -r FILE; do
    REL="${FILE#$HTML_DIR/}"
    ID=$(make_id "$REL")
    MIME=$(mime_type "$FILE")

    gzip -9 -n < "$FILE" > "$TEMP"

    {
        echo "// File: $REL"
        echo "static const unsigned char data_${ID}[] = {"
        xxd -i < "$TEMP" | grep -v "^unsigned" | grep -v "^};"
        echo "};"
        echo "static const size_t size_${ID} = sizeof(data_${ID});"
        echo
    } >> "$OUTPUT"
done <<< "$FILES_LIST"

cat >> "$OUTPUT" <<'EOF'

void initialize() {
EOF

while IFS= read -r FILE; do
    REL="${FILE#$HTML_DIR/}"
    ID=$(make_id "$REL")
    MIME=$(mime_type "$FILE")
    echo "    files.emplace(\"${REL}\", FileData(data_${ID}, size_${ID}, \"${MIME}\"));" >> "$OUTPUT"
done <<< "$FILES_LIST"

cat >> "$OUTPUT" <<'EOF'
}

struct Init {
    Init() { initialize(); }
} init;

} // namespace WebDB
EOF

FILE_COUNT=$(echo "$FILES_LIST" | grep -c .)
echo "Generated $OUTPUT ($(wc -c < "$OUTPUT") bytes, $FILE_COUNT files)"

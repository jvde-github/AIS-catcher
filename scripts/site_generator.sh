#!/bin/bash

# Check for the correct number of input arguments
if [ "$#" -ne 3 ]; then
  echo "Usage: $0 <base_template.html> <content_file.txt> <output_directory>"
  exit 1
fi

# Input files and output directory
TEMPLATE="$1"
CONTENT_FILE="$2"
OUTPUT_DIR="$3"

# Ensure the output directory exists
mkdir -p "$OUTPUT_DIR"

# Extract filename without the extension for naming the output files
CONTENT_FILENAME=$(basename "$CONTENT_FILE" .txt)
OUTPUT_HTML_FILE="${CONTENT_FILENAME}.html"

# Function to extract section from content file
extract_section() {
  local section="$1"
  local content_file="$2"
  
  # Use sed to extract lines between section markers
  sed -n "/<!-- $section -->/,/<!--/{//!p}" "$content_file"
}

# Generate page function
generate_page() {
  local content_file="$1"
  local output_file="$2"

  # Check if content file exists
  if [ ! -f "$content_file" ]; then
    echo "Error: Content file $content_file does not exist."
    return
  fi

  # Extract the sections
  local css_content
  local html_content
  local scripts_content

  css_content=$(extract_section "CSS" "$content_file")
  html_content=$(extract_section "CONTENT" "$content_file")
  scripts_content=$(extract_section "SCRIPTS" "$content_file")

  # Escape forward slashes in content to prevent sed issues
  css_content_escaped=$(echo "$css_content" | sed 's/[&/\]/\\&/g')
  html_content_escaped=$(echo "$html_content" | sed 's/[&/\]/\\&/g')
  scripts_content_escaped=$(echo "$scripts_content" | sed 's/[&/\]/\\&/g')

  # Replace placeholders in the template
  sed -e "s|{{css}}|${css_content_escaped}|g" \
      -e "s|{{content}}|${html_content_escaped}|g" \
      -e "s|{{scripts}}|${scripts_content_escaped}|g" "$TEMPLATE" > "${output_file}"
}

# Generate the HTML page
generate_page "$CONTENT_FILE" "${OUTPUT_DIR}/${OUTPUT_HTML_FILE}"

# Check if the HTML file was successfully created
if [ ! -f "${OUTPUT_DIR}/${OUTPUT_HTML_FILE}" ]; then
  echo "Error: Failed to create ${OUTPUT_DIR}/${OUTPUT_HTML_FILE}."
  exit 1
fi

# Remove previous C++ files in the output directory
rm -f "${OUTPUT_DIR}/index_html.cpp" "${OUTPUT_DIR}/index_local_html.cpp" "${OUTPUT_DIR}/style_css.cpp" "${OUTPUT_DIR}/script_js.cpp"

# Minify, compress, and convert the HTML file to a C++ file
minify "${OUTPUT_DIR}/${OUTPUT_HTML_FILE}" | gzip > "${OUTPUT_DIR}/${CONTENT_FILENAME}_html_gz"
xxd -i "${OUTPUT_DIR}/${CONTENT_FILENAME}_html_gz" > "${OUTPUT_DIR}/${CONTENT_FILENAME}_html.cpp"

# Clean up the temporary gzip file
rm -f "${OUTPUT_DIR}/${CONTENT_FILENAME}_html_gz"

echo "Page generated and C++ file created successfully in the '$OUTPUT_DIR' directory."

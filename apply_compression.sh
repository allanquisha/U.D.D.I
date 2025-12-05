#!/bin/bash
# Script to replace uncompressed HTML with compressed version in main.cpp

SRC_FILE="src/main.cpp"
BACKUP_FILE="src/main.cpp.backup"

echo "Creating backup..."
cp "$SRC_FILE" "$BACKUP_FILE"

echo "Applying compression patch..."

# Use sed to replace the HTML section
# Find line with "static const char *html_page" and replace until line with just ";"
python3 << 'EOF'
import re

with open('src/main.cpp', 'r') as f:
    content = f.read()

# Read the compressed HTML bytes from the generated file
with open('html_compressed.h', 'r') as f:
    compressed = f.read()
    # Extract just the C code part (skip the stats output)
    compressed_code = '\n'.join([line for line in compressed.split('\n') if line.startswith('static') or line.startswith('    0x') or line.startswith('};')])

# Pattern to match the old HTML declaration
pattern = r'// HTML content for the web interface\nstatic const char \*html_page = .*?"</html>";'

# Replacement
replacement = f'''// Compressed HTML content (saves ~3.8KB flash!)
{compressed_code}'''

# Replace
new_content = re.sub(pattern, replacement, content, flags=re.DOTALL)

# Also update the root_handler to serve compressed content
old_handler = '''// HTTP GET handler for main page
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}'''

new_handler = '''// HTTP GET handler for main page - serves compressed HTML
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, (const char *)html_page_gz, html_page_gz_len);
    return ESP_OK;
}'''

new_content = new_content.replace(old_handler, new_handler)

with open('src/main.cpp', 'w') as f:
    f.write(new_content)

print("✓ Compression applied successfully!")
EOF

echo "Done! Backup saved to $BACKUP_FILE"
echo "Build and check memory usage with: platformio run"

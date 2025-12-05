#!/usr/bin/env python3
"""
HTML Minifier and GZIP Compressor for ESP32
Converts HTML to minified, gzipped C array
"""

import gzip
import sys

def minify_html(html):
    """Basic HTML minification"""
    # Remove extra whitespace
    lines = [line.strip() for line in html.split('\n')]
    html = ''.join(lines)
    
    # Remove spaces around tags
    html = html.replace('> <', '><')
    html = html.replace('; }', ';}')
    html = html.replace(': ', ':')
    html = html.replace(' {', '{')
    
    return html

def html_to_gzip_array(html):
    """Convert HTML to gzipped C byte array"""
    # Minify first
    minified = minify_html(html)
    
    # Compress with gzip
    compressed = gzip.compress(minified.encode('utf-8'), compresslevel=9)
    
    # Convert to C array format
    hex_bytes = ', '.join(f'0x{b:02x}' for b in compressed)
    
    # Create C code
    c_code = f'''// Auto-generated compressed HTML ({len(compressed)} bytes, {len(minified)} bytes uncompressed)
static const uint8_t html_page_gz[] = {{
    {hex_bytes}
}};
static const size_t html_page_gz_len = {len(compressed)};
'''
    
    print(f"Original size: {len(html)} bytes")
    print(f"Minified size: {len(minified)} bytes ({100*(len(minified)/len(html)):.1f}%)")
    print(f"Compressed size: {len(compressed)} bytes ({100*(len(compressed)/len(html)):.1f}%)")
    print(f"Savings: {len(html) - len(compressed)} bytes ({100*(1-len(compressed)/len(html)):.1f}%)")
    
    return c_code

if __name__ == '__main__':
    # Read HTML from stdin or file
    if len(sys.argv) > 1:
        with open(sys.argv[1], 'r') as f:
            html = f.read()
    else:
        html = sys.stdin.read()
    
    print(html_to_gzip_array(html))

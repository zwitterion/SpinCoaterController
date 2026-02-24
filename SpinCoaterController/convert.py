import argparse
import gzip
import os
import minify_html

def main():
    parser = argparse.ArgumentParser(description="Convert HTML to C++ Header")
    parser.add_argument("--mode", choices=["dev", "prod"], default="prod", help="dev=minify only, prod=gzip")
    parser.add_argument("input", nargs="?", default="index.html", help="Input HTML file")
    parser.add_argument("output", nargs="?", default="web_assets.h", help="Output .h file")
    
    args = parser.parse_args()
    
    with open(args.input, 'r', encoding='utf-8') as f:
        content = f.read()
        
    # Use minify_html for robust minification of HTML, CSS, and JS
    minified = minify_html.minify(content, minify_js=True, minify_css=True, remove_processing_instructions=True)
    
    final_bytes = b""
    is_gzipped = False
    
    if args.mode == "prod":
        final_bytes = gzip.compress(minified.encode('utf-8'))
        is_gzipped = True
    else:
        final_bytes = minified.encode('utf-8')
        
    # Generate C++ Header
    with open(args.output, 'w') as f:
        f.write("#ifndef WEB_ASSETS_H\n")
        f.write("#define WEB_ASSETS_H\n\n")
        f.write("#include <Arduino.h>\n\n")
        
        if is_gzipped:
            f.write("// GZIP Compressed HTML\n")
            f.write("const bool WEB_ASSETS_GZIPPED = true;\n")
        else:
            f.write("// Plain HTML\n")
            f.write("const bool WEB_ASSETS_GZIPPED = false;\n")
            
        f.write(f"const size_t INDEX_HTML_SIZE = {len(final_bytes)};\n")
        f.write("const uint8_t INDEX_HTML[] PROGMEM = {\n")
        
        # Hex dump
        hex_str = ", ".join(f"0x{b:02X}" for b in final_bytes)
        f.write(hex_str)
        
        f.write("\n};\n\n")
        f.write("#endif\n")

if __name__ == "__main__":
    main()
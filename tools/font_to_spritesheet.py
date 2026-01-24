#!/usr/bin/env python3
"""
Convert TinyPixie2.ttf to a pixel-perfect PNG spritesheet for GBA.
Handles variable-width fonts and generates character width metadata.
"""

from PIL import Image, ImageFont, ImageDraw
import sys
import json

def create_font_spritesheet(ttf_path, output_png, font_size=8, chars_per_row=16):
    """
    Create a spritesheet from a TTF font file.
    
    Args:
        ttf_path: Path to the TTF font file
        output_png: Path to output PNG file
        font_size: Font size in pixels
        chars_per_row: Number of characters per row in the spritesheet
    """
    
    # ASCII printable characters (32-126)
    start_char = 32  # Space
    end_char = 126   # Tilde
    char_range = range(start_char, end_char + 1)
    
    # Load font
    try:
        font = ImageFont.truetype(ttf_path, font_size)
    except Exception as e:
        print(f"Error loading font: {e}")
        sys.exit(1)
    
    # Measure all characters to find max dimensions
    max_width = 0
    max_height = 0
    char_metrics = {}
    
    for char_code in char_range:
        char = chr(char_code)
        
        # Create a temporary image to measure the character
        temp_img = Image.new('RGB', (100, 100), color='black')
        temp_draw = ImageDraw.Draw(temp_img)
        
        # Get bounding box
        bbox = temp_draw.textbbox((0, 0), char, font=font)
        width = bbox[2] - bbox[0]
        height = bbox[3] - bbox[1]
        
        char_metrics[char_code] = {
            'width': width,
            'height': height,
            'bbox': bbox
        }
        
        max_width = max(max_width, width)
        max_height = max(max_height, height)
    
    # Add padding
    cell_width = max_width + 2  # 1 pixel padding on each side
    cell_height = max_height + 2
    
    # Round up to 8x8 for GBA tiles
    cell_width = 8
    cell_height = 8
    
    print(f"Font metrics:")
    print(f"  Max character size: {max_width}x{max_height}")
    print(f"  Cell size (with padding): {cell_width}x{cell_height}")
    
    # Calculate spritesheet dimensions
    total_chars = len(char_range)
    rows = (total_chars + chars_per_row - 1) // chars_per_row
    sheet_width = cell_width * chars_per_row
    sheet_height = cell_height * rows
    
    print(f"  Spritesheet size: {sheet_width}x{sheet_height}")
    print(f"  Layout: {chars_per_row}x{rows} ({total_chars} chars)")
    
    # Create the spritesheet (magenta background for transparency)
    spritesheet = Image.new('RGB', (sheet_width, sheet_height), color='#FF00FF')
    draw = ImageDraw.Draw(spritesheet)
    
    # Draw each character
    char_widths = []
    for i, char_code in enumerate(char_range):
        char = chr(char_code)
        
        row = i // chars_per_row
        col = i % chars_per_row
        
        x = col * cell_width + 1  # 1 pixel padding
        y = row * cell_height + 1
        
        # Draw character in white
        draw.text((x, y), char, font=font, fill='white')
        
        # Store width for metadata
        char_widths.append(char_metrics[char_code]['width'])
    
    # Save the spritesheet
    spritesheet.save(output_png)
    print(f"\nSaved spritesheet to: {output_png}")
    
    # Generate metadata for C header
    metadata = {
        'start_char': start_char,
        'end_char': end_char,
        'cell_width': cell_width,
        'cell_height': cell_height,
        'chars_per_row': chars_per_row,
        'char_widths': char_widths
    }
    
    # Save metadata as JSON
    metadata_path = output_png.replace('.png', '_meta.json')
    with open(metadata_path, 'w') as f:
        json.dump(metadata, f, indent=2)
    print(f"Saved metadata to: {metadata_path}")
    
    # Generate C header snippet
    header_path = output_png.replace('.png', '_widths.h')
    with open(header_path, 'w') as f:
        f.write(f"// Font metadata for {ttf_path}\n")
        f.write(f"#define FONT_START_CHAR {start_char}\n")
        f.write(f"#define FONT_END_CHAR {end_char}\n")
        f.write(f"#define FONT_CELL_WIDTH {cell_width}\n")
        f.write(f"#define FONT_CELL_HEIGHT {cell_height}\n")
        f.write(f"#define FONT_CHARS_PER_ROW {chars_per_row}\n\n")
        f.write(f"// Character widths (for variable-width rendering)\n")
        f.write(f"const unsigned char font_char_widths[] = {{\n")
        for i in range(0, len(char_widths), 16):
            line = char_widths[i:i+16]
            f.write("    " + ", ".join(f"{w:2d}" for w in line) + ",\n")
        f.write("};\n")
    print(f"Saved C header snippet to: {header_path}")
    
    print(f"\nDone! Add {output_png} to your assets/ folder.")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python font_to_spritesheet.py <font.ttf> [output.png] [font_size]")
        print("Example: python font_to_spritesheet.py TinyPixie2.ttf tinypixie.png 8")
        sys.exit(1)
    
    ttf_path = sys.argv[1]
    output_png = sys.argv[2] if len(sys.argv) > 2 else 'font_spritesheet.png'
    font_size = int(sys.argv[3]) if len(sys.argv) > 3 else 8
    
    create_font_spritesheet(ttf_path, output_png, font_size)

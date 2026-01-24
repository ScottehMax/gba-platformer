#!/usr/bin/env python3
"""
Fix transparency in PNG files by replacing transparent pixels with magenta (#FF00FF).
This prepares images for GBA which uses palette index 0 for transparency.

Usage: uv run --with=Pillow fix_transparency.py <input.png> <output.png>
"""

import sys
from PIL import Image

def fix_transparency(input_path, output_path):
    """Replace transparent pixels with magenta."""
    # Open the image
    img = Image.open(input_path)
    
    # Convert to RGBA if not already
    if img.mode != 'RGBA':
        img = img.convert('RGBA')
    
    # Get pixel data
    pixels = img.load()
    width, height = img.size
    
    # Replace transparent pixels with magenta
    magenta = (255, 0, 255, 255)
    replaced_count = 0
    
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            # If pixel is transparent or semi-transparent
            if a < 255:
                pixels[x, y] = magenta
                replaced_count += 1
    
    # Convert to RGB (no alpha channel needed)
    img = img.convert('RGB')
    
    # Save the result
    img.save(output_path)
    print(f"Processed {input_path} -> {output_path}")
    print(f"Replaced {replaced_count} transparent pixels with magenta")

def main():
    if len(sys.argv) != 3:
        print("Usage: uv run --with=Pillow fix_transparency.py <input.png> <output.png>")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2]
    
    try:
        fix_transparency(input_path, output_path)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()

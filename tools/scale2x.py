import sys
from PIL import Image

def scale_image(input_path):
    img = Image.open(input_path)
    width, height = img.size
    scaled_img = img.resize((width * 2, height * 2), Image.NEAREST)
    output_path = input_path.replace('.png', '_2x.png')
    scaled_img.save(output_path)
    print(f"Saved scaled image to {output_path}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python scale2x.py <image.png>")
        sys.exit(1)
    scale_image(sys.argv[1])

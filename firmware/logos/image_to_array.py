#!/usr/bin/env python3
"""
Image to C++ Array Converter (batch)

Converts image files to C++ arrays of 24-bit hex RGB color values (0xRRGGBB).

Usage:
    python image_to_array.py <path>

Where <path> can be either:
    - a single image file (PNG/JPG/BMP/GIF/WEBP), or
    - a directory containing image files (non-recursive)

Output for each image:
    - Console output of the C++ array
    - Header file in the same folder with the same base name and 
      the ".h" extension (e.g., logo.png -> logo.h)
"""

import sys
import os
from pathlib import Path
from typing import List, Tuple

SUPPORTED_EXTS = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"}

try:
    from PIL import Image
except ImportError:
    print("Error: PIL (Pillow) is required. Install it with: pip install Pillow")
    sys.exit(1)


def sanitize_identifier(name: str) -> str:
    """Make a valid C identifier from a file stem."""
    out = []
    for i, ch in enumerate(name):
        if (ch.isalnum()) or ch == '_':
            out.append(ch)
        else:
            out.append('_')
    ident = ''.join(out)
    if ident and ident[0].isdigit():
        ident = '_' + ident
    return ident or 'image_data'


def image_to_rgb_array(image_path: Path) -> Tuple[int, int, List[List[int]]]:
    """
    Convert an image to a 2D array of 24-bit RGB hex values.

    Returns (width, height, 2D list of color values).
    """
    try:
        img = Image.open(image_path)
    except Exception as e:
        raise RuntimeError(f"Error opening '{image_path}': {e}")

    # For animated GIFs, use the first frame
    try:
        if getattr(img, "is_animated", False):
            img.seek(0)
    except Exception:
        pass

    if img.mode != 'RGB':
        img = img.convert('RGB')

    width, height = img.size
    pixels = img.load()

    color_array: List[List[int]] = []
    for y in range(height):
        row: List[int] = []
        for x in range(width):
            r, g, b = pixels[x, y]
            color_value = (r << 16) | (g << 8) | b  # 0xRRGGBB
            row.append(color_value)
        color_array.append(row)

    return width, height, color_array


def format_cpp_array(color_array: List[List[int]], array_name: str) -> str:
    """Format a 2D color array as C++ code with 0xRRGGBB values."""
    height = len(color_array)
    width = len(color_array[0]) if height > 0 else 0

    lines = [f"const uint32_t {array_name}[{height}][{width}] = {{"]

    for row_idx, row in enumerate(color_array):
        row_hex = [f"0x{color:06X}" for color in row]
        row_str = ", ".join(row_hex)
        if row_idx < len(color_array) - 1:
            lines.append(f"    {{{row_str}}},")
        else:
            lines.append(f"    {{{row_str}}}")

    lines.append("};")
    return "\n".join(lines)


def process_image_file(image_path: Path) -> None:
    width, height, color_array = image_to_rgb_array(image_path)
    array_name = sanitize_identifier(image_path.stem)
    cpp_code = format_cpp_array(color_array, array_name)

    print(f"\n=== {image_path.name} ({width}x{height}) ===")
    print(cpp_code)

    output_path = image_path.with_suffix('.h')
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("#pragma once\n\n")
        f.write(f"// Generated from {image_path.name}\n")
        f.write(f"// Image size: {width}x{height}\n\n")
        f.write(cpp_code)
        f.write("\n")

    print(f"Written: {output_path}")


def gather_images_in_dir(dir_path: Path) -> List[Path]:
    """List image files in a directory (non-recursive)."""
    images: List[Path] = []
    for entry in dir_path.iterdir():
        if entry.is_file() and entry.suffix.lower() in SUPPORTED_EXTS:
            images.append(entry)
    return sorted(images)


def main():
    if len(sys.argv) != 2:
        print("Usage: python image_to_array.py <path>")
        print("  <path>: image file or directory containing images")
        sys.exit(1)

    target = Path(sys.argv[1])

    if not target.exists():
        print(f"Error: '{target}' not found")
        sys.exit(1)

    if target.is_file():
        if target.suffix.lower() not in SUPPORTED_EXTS:
            print(f"Error: Unsupported file extension '{target.suffix}'. Supported: {', '.join(sorted(SUPPORTED_EXTS))}")
            sys.exit(1)
        process_image_file(target)
    elif target.is_dir():
        images = gather_images_in_dir(target)
        if not images:
            print(f"No image files found in '{target}'. Supported extensions: {', '.join(sorted(SUPPORTED_EXTS))}")
            sys.exit(1)
        print(f"Found {len(images)} image(s) in '{target}'. Processing...")
        for img in images:
            try:
                process_image_file(img)
            except Exception as e:
                print(f"Error processing {img.name}: {e}")
        print("\nDone.")
    else:
        print(f"Error: '{target}' is neither a file nor a directory")
        sys.exit(1)


if __name__ == "__main__":
    main()

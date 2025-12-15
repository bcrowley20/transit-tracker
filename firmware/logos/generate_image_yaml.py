#!/usr/bin/env python3
"""
Generate ESPHome YAML snippets for image resources and the logos[] array
based on the files in a directory.

Usage:
  python generate_image_yaml.py <dir> [--limit N]

Outputs two sections:
- image: entries (file, id, type)
- C++ const Image* logos[] = { id(...), ... } array

Paste these into firmware/image-display.yaml in the appropriate places.
"""

import sys
from pathlib import Path
from typing import List

SUPPORTED_EXTS = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"}


def sanitize_identifier(stem: str) -> str:
    s = ''.join(ch if (ch.isalnum() or ch == '_') else '_' for ch in stem)
    if s and s[0].isdigit():
        s = '_' + s
    return s or 'image_data'


def gather_images(dir_path: Path) -> List[Path]:
    return sorted([p for p in dir_path.iterdir() if p.is_file() and p.suffix.lower() in SUPPORTED_EXTS])


def main():
    if len(sys.argv) < 2:
        print("Usage: python generate_image_yaml.py <dir> [--limit N]")
        sys.exit(1)

    dir_path = Path(sys.argv[1])
    if not dir_path.is_dir():
        print(f"Error: '{dir_path}' is not a directory")
        sys.exit(1)

    limit = None
    if len(sys.argv) == 4 and sys.argv[2] == '--limit':
        try:
            limit = int(sys.argv[3])
        except ValueError:
            print("Error: --limit expects an integer")
            sys.exit(1)

    images = gather_images(dir_path)
    if limit:
        images = images[:limit]

    # YAML for image entries
    print("image:")
    for img in images:
        ident = sanitize_identifier(img.stem)
        # Use absolute Windows path for consistency in current config
        print(f"  - file: \"{img.resolve()}\"")
        print(f"    id: {ident}")
        print(f"    type: RGB565")
    print()

    # C++ logos array snippet
    print("// C++ logos[] array snippet:")
    print("const Image* logos[] = {")
    for idx, img in enumerate(images):
        ident = sanitize_identifier(img.stem)
        comma = ',' if idx < len(images) - 1 else ''
        print(f"  id({ident}){comma}")
    print("};")


if __name__ == '__main__':
    main()

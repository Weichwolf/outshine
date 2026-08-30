#!/usr/bin/env python3
"""Write every case's reference picture from the oracle's own floats.

The oracle is rendered ONCE, into `oracle.raw` beside the case in the prepared corpus. That file is
float32 RGBA in the scene's linear space; the reference this tree commits is the same picture under
Blender's stated colour management -- displayDevice sRGB, view transform Standard, gamma 1, exposure
0 -- which is one sRGB transfer and nothing else. Verified: the encode reproduces the reference this
tree already held for ABeautifulGame byte for byte, 921 600 of 921 600 pixels.

The floats stay OUT of the tree. They are 9.3 GB across the corpus and they exist to LICENSE a
reference, not to be one.
"""
import os
import pathlib
import struct
import sys

import numpy as np
from PIL import Image

TREE = pathlib.Path(__file__).resolve().parents[2]
kMagic = b"OSRAWF32"


def floats(path):
    held = path.read_bytes()
    if held[:8] != kMagic:
        return None
    width, height, channels = struct.unpack_from("<III", held, 16)
    at = struct.unpack_from("<I", held, 28)[0]
    if at + width * height * channels * 4 != len(held):
        return None
    return np.frombuffer(held, dtype="<f4", count=width * height * channels,
                         offset=at).reshape(height, width, channels)


def encoded(linear):
    held = np.clip(linear[..., :3], 0.0, 1.0)
    shown = np.where(held <= 0.0031308, held * 12.92,
                     1.055 * np.power(held, 1.0 / 2.4) - 0.055)
    return np.round(shown * 255.0).astype(np.uint8)


def prepared_root():
    return pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-prepared"


def main():
    wrote, unlicensed, refused = 0, 0, 0
    for manifest in sorted((TREE / "test" / "khronos").glob("*/*/manifest.json")):
        where = manifest.parent
        if b'"renders"' not in manifest.read_bytes():
            continue
        raw = prepared_root() / str(where.relative_to(TREE)).replace("/", "-") / "oracle.raw"
        if not raw.exists():
            unlicensed += 1
            continue
        linear = floats(raw)
        if linear is None:
            print(f"REFUSED {where.name}: {raw.name} is not this tree's float format")
            refused += 1
            continue
        Image.fromarray(encoded(linear)).save(where / "reference.png")
        wrote += 1
    print(f"{wrote} reference(s) written, {unlicensed} case(s) with no oracle rendered yet, "
          f"{refused} refused")
    return 1 if refused else 0


if __name__ == "__main__":
    sys.exit(main())

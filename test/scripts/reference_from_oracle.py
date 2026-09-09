#!/usr/bin/env python3
"""Explicitly pin reference images in JSON and keep their bytes outside the repository."""
import argparse
import json
import os
import pathlib
import struct
import sys

import numpy as np
from PIL import Image
from reference_store import pin

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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", nargs="*")
    parser.add_argument("--migrate-existing", action="store_true")
    args = parser.parse_args()
    written, missing = 0, 0
    known = set()
    for manifest in sorted((TREE / "test" / "khronos").glob("*/*/manifest.json")):
        known.add(manifest.parent.name)
        if args.case and manifest.parent.name not in args.case:
            continue
        declared = json.loads(manifest.read_text())
        if not declared.get("renders"):
            continue
        where = manifest.parent
        prepared = prepared_root() / str(where.relative_to(TREE)).replace("/", "-")
        fps = declared.get("scene", {}).get("animation", {}).get("fps", {}).get("value", 1)
        images = sorted(where.glob("reference*.png")) if args.migrate_existing else []
        if not args.migrate_existing:
            sources = [prepared / "oracle.raw"] if (prepared / "oracle.raw").is_file() else sorted(prepared.glob("oracle.f[0-9][0-9][0-9][0-9].raw"))
            for source in sources:
                linear = floats(source)
                if linear is None:
                    raise ValueError(f"invalid oracle float image: {source}")
                image = prepared / source.name.replace("oracle", "reference").replace(".raw", ".png")
                Image.fromarray(encoded(linear)).save(image)
                images.append(image)
        if not images:
            missing += 1
            continue
        records = []
        for image in images:
            frame = int(image.stem.split(".f")[1]) if ".f" in image.stem else 0
            records.append(pin(image, frame, frame / fps))
        declared["referenceImages"] = sorted(records, key=lambda record: record["frame"])
        manifest.write_text(json.dumps(declared, indent=2) + "\n")
        if args.migrate_existing:
            for image in images:
                image.unlink()
        written += len(records)
    unknown = set(args.case) - known
    if unknown:
        parser.error("unknown cases: " + ", ".join(sorted(unknown)))
    print(f"{written} image hashes pinned; {missing} cases without available reference images")
    return 0 if written else 1


if __name__ == "__main__":
    sys.exit(main())

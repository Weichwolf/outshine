#!/usr/bin/env python3
"""Compare immutable sharing with world snapshot copies using a real Place image."""
import argparse
import json
import time
import tracemalloc
from pathlib import Path

from PIL import Image


def measure(pixels, copies, clone):
    tracemalloc.start()
    began = time.perf_counter_ns()
    worlds = [bytearray(pixels) if clone else pixels for _ in range(copies)]
    elapsed = (time.perf_counter_ns() - began) / 1e6
    live, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    assert all(world == pixels for world in worlds)
    return {"ms": elapsed, "live_bytes": live, "peak_bytes": peak}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--copies", type=int, default=8)
    args = parser.parse_args()
    if args.copies < 1:
        parser.error("copies must be positive")
    with Image.open(args.image) as image:
        pixels = image.convert("RGBA").tobytes()
    print(json.dumps({"image": str(args.image), "input_bytes": len(pixels),
                      "copies": args.copies, "clone": measure(pixels, args.copies, True),
                      "share": measure(pixels, args.copies, False),
                      "scope": "CPU image ownership model, not native GPU performance"}))


if __name__ == "__main__":
    main()

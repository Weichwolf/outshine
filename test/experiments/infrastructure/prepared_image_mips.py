#!/usr/bin/env python3
"""Measure prepared lower-mip storage for a real cached impostor prototype.

Counts uncompressed RGBA8 payload only; compression, uploads and frame time need
native measurement. Uses the existing capture, without regenerating an atlas.
"""
import argparse
import json
from pathlib import Path

from impostor_ready_payload import measure


def lower_bytes(width, height):
    total = 0
    while width > 1 or height > 1:
        width, height = max(1, width // 2), max(1, height // 2)
        total += width * height * 4
    return total


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cache', type=Path)
    parser.add_argument('--zstd-library')
    args = parser.parse_args()
    source = measure(args.cache, args.zstd_library)
    pixels, views = source['pixels'], source['views']
    base = source['ready_rgba8_bytes']
    lower = lower_bytes(pixels, pixels) * views * 3
    assert lower_bytes(5, 3) == 12 and lower_bytes(1, 7) == 16
    print(json.dumps(dict(asset=source['asset'], pixels=pixels, views=views,
                          base_rgba8_bytes=base, lower_rgba8_bytes=lower,
                          prepared_total_rgba8_bytes=base + lower,
                          extra_fraction=lower / base,
                          duplicated_base_bytes=0,
                          scope='real prototype dimensions; no compression or GPU timing proof'),
                     indent=2))


if __name__ == '__main__':
    main()

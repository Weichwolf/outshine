import argparse
import json
from pathlib import Path
from time import perf_counter

import numpy as np


def unique_rows(words):
    keys = np.ascontiguousarray(words).view("V16").reshape(-1)
    table, indices = np.unique(keys, return_inverse=True)
    return table.view("<u4").reshape(-1, 4), indices


def encode(colours, width):
    words = np.asarray(colours, dtype="<u4")
    if words.ndim != 2 or words.shape[1] != 4 or width not in (None, 16, 32):
        raise ValueError("expected four UInt32 colour words and 16/32-bit indices")
    table, indices = unique_rows(words)
    if width is None:
        width = 16 if len(table) <= 65536 else 32
    if len(table) > 1 << width or len(words) > 0xFFFFFFFF:
        raise ValueError("palette or colour count exceeds the selected encoding")
    packed = indices.astype("<u2" if width == 16 else "<u4").tobytes()
    padded = packed + bytes((-len(packed)) % 16)
    first = 4 + len(padded) // 4
    if first > 0xFFFFFFFF:
        raise ValueError("palette offset exceeds the buffer encoding")
    header = np.array([first, len(words), len(table), width], dtype="<u4")
    return header.tobytes() + padded + table.tobytes()


def view(payload):
    """Validate unchanged palette bytes, then borrow table/index views only."""
    if len(payload) < 16 or len(payload) % 16:
        raise ValueError("truncated palette buffer")
    first, count, unique, width = map(int, np.frombuffer(payload, "<u4", count=4))
    if width not in (16, 32) or unique > 1 << width:
        raise ValueError("invalid index encoding")
    index_bytes = count * (width // 8)
    expected_first = 4 + (index_bytes + 15) // 16 * 4
    if first != expected_first or (first + unique * 4) * 4 != len(payload):
        raise ValueError("palette spans differ from the buffer length")
    indices = np.frombuffer(payload, "<u2" if width == 16 else "<u4", count, 16)
    if count and (unique == 0 or int(indices.max()) >= unique):
        raise ValueError("colour index exceeds the palette")
    table = np.frombuffer(payload, "<u4", unique * 4, first * 4).reshape(-1, 4)
    return table, indices


def decode(payload):
    table, indices = view(payload)
    return table[indices]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.input.stat().st_size % 16:
        parser.error("input must contain complete RGBA32F entries")
    colours = np.fromfile(args.input, dtype="<u4").reshape(-1, 4)
    args.output.mkdir(parents=True, exist_ok=True)
    report = {
        "scope": "Bit-exact CPU encoding; no GPU traffic, frame-time or product-memory proof",
        "source_bytes": colours.nbytes,
        "colour_slots": len(colours),
        "unique_rgba_words": len(np.unique(colours, axis=0)),
        "variants": [],
    }
    for width in (16, 32):
        started = perf_counter()
        try:
            payload = encode(colours, width)
        except ValueError as error:
            report["variants"].append({"index_bits": width, "unavailable": str(error)})
            continue
        elapsed = (perf_counter() - started) * 1000
        restored = decode(payload)
        if not np.array_equal(colours, restored):
            raise ValueError("colour words changed during roundtrip")
        (args.output / f"palette-{width}.bin").write_bytes(payload)
        report["variants"].append(
            {
                "index_bits": width,
                "encoded_bytes": len(payload),
                "encode_ms": elapsed,
                "source_roundtrip_exact": True,
            }
        )
    (args.output / "inventory.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()

"""Compare existing conservative coarse patches with original indexed-mesh D32."""

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def floats(path, count):
    data = path.read_bytes()
    if len(data) != count * 4:
        raise ValueError(f"incorrect dump size: {path}")
    result = struct.unpack(f"<{count}f", data)
    if not all(math.isfinite(v) and 0 <= v <= 1 for v in result):
        raise ValueError(f"invalid depth: {path}")
    return result


def compare(coarse, fine, width, height):
    tested, missing, unsafe = 0, 0, 0
    examples = []
    for y in range(height):
        cy = (2 * y + 1) * 64 // (2 * height)
        for x in range(width):
            cx = (2 * x + 1) * 64 // (2 * width)
            bound = coarse[cy * 64 + cx]
            if bound <= 0:
                continue
            actual = fine[y * width + x]
            tested += 1
            missing += actual == 0
            unsafe += actual < bound
            if actual < bound and len(examples) < 16:
                examples.append({"pixel": [x, y], "bound": bound, "actual": actual})
    return {
        "tested_samples": tested,
        "missing": missing,
        "unsafe": unsafe,
        "examples": examples,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--patch-run", type=Path, required=True)
    parser.add_argument("--depth-run", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    patch, depth = args.patch_run.resolve(), args.depth_run.resolve()
    receipts = [json.loads((p / "receipt.json").read_text()) for p in (patch, depth)]
    for key in (
        "native_commit",
        "input_sha256",
        "archive_sha256",
        "compile_database_sha256",
    ):
        if receipts[0][key] != receipts[1][key]:
            raise ValueError(f"mismatched input/native lineage: {key}")
    if not all(r["success"] and not r["source_worktree_dirty"] for r in receipts):
        raise ValueError("nonterminal or dirty upstream evidence")
    description = json.loads((depth / "result.json").read_text())
    if description["mode_count"] != 6 or description["qualified_for_page_suppression"]:
        raise ValueError("expected six-mode isolated indexed-mesh comparison")
    coarse_path = patch / "gpu/native-uncertain-integer.colour.f32"
    coarse = floats(coarse_path, 64 * 64)
    width, height = 1280, 720
    results = []
    for mode, name in ((4, "precise-native-lit"), (5, "precise-native-flat")):
        fine_path = depth / f"depth-{mode}.d32"
        result = compare(coarse, floats(fine_path, width * height), width, height)
        result.update({"mode": name, "depth_sha256": digest(fine_path)})
        results.append(result)
    result = {
        "patch_receipt_sha256": digest(patch / "receipt.json"),
        "depth_receipt_sha256": digest(depth / "receipt.json"),
        "input_sha256": receipts[0]["input_sha256"],
        "coarse_sha256": digest(coarse_path),
        "fine_profile": [width, height],
        "coarse_profile": [64, 64],
        "positive_coarse_cells": sum(v > 0 for v in coarse),
        "meshes": description["meshes"],
        "results": results,
        "success": all(r["tested_samples"] > 0 and r["unsafe"] == 0 for r in results),
        "qualified_for_page_suppression": False,
        "scope": "Finite same-input indexed-mesh sample comparison; half-open coarse-cell assignment by exact fine-sample centre; no full-pixel raster proof or world admission",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    if not result["success"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

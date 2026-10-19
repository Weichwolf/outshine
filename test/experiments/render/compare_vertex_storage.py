"""Exact original-vertex storage controls for the geometry-only prism comparison."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from full_view_reference import load

FIELDS = ("positions", "uv", "normal", "colour", "metres")


def controls(source):
    count = len(source["positions"])
    records = np.concatenate(
        [
            np.ascontiguousarray(source[k]).view(np.uint8).reshape(count, -1)
            for k in FIELDS
        ],
        axis=1,
    )
    if records.shape[1] != 48 or not np.isfinite(source["positions"]).all():
        raise ValueError("known finite 48-byte diagnostic vertex model required")
    arrays, report = {}, {}
    for name, values in (
        (
            "position",
            np.ascontiguousarray(source["positions"]).view(np.uint8).reshape(count, 12),
        ),
        ("full_vertex", records),
    ):
        unique, inverse = np.unique(values, axis=0, return_inverse=True)
        remapped = inverse[source["indices"]]
        # Every triangle corner keeps every represented input bit, including signed zeros.
        np.testing.assert_array_equal(unique[remapped], values[source["indices"]])
        arrays[name + "_records"] = unique
        for dtype in (np.uint32, np.uint16):
            if len(unique) > np.iinfo(dtype).max + 1:
                continue
            indices = remapped.astype(dtype)
            np.testing.assert_array_equal(indices, remapped)
            key = name + "_" + np.dtype(dtype).name
            arrays[key + "_indices"] = indices
            report[key] = {
                "vertices": len(unique),
                "record_bytes": unique.nbytes,
                "index_bytes": indices.nbytes,
                "total_bytes": unique.nbytes + indices.nbytes,
                "bit_equal_triangle_corners": int(remapped.size),
            }
    return arrays, report


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source, _, _, receipt = load(args.input)
    arrays, result = controls(source)
    args.output.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(args.output / "vertex-controls.npz", **arrays)
    report = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "input_receipt": receipt,
        "scope": "exact original triangle order/winding/attributes; position-only controls exclude all attributes; no native GPU layout or speed claim",
        "controls": result,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()

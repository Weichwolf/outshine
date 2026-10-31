"""Compare reconstructed mixed sources through the existing native input adapter.

This is an offline diagnostic, not a new native API, render or World admission.
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from source_patch_inputs import read_bound_sources
from source_patch_product import write_product

ROOT = Path(__file__).resolve().parents[3]


def run(args):
    original, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    producer = json.loads((args.producer / "report.json").read_text())
    if any(
        producer["input_provenance"][key] != provenance[key]
        for key in ("source_binding_sha256", "source_archive_sha256")
    ):
        raise ValueError("Bound original cohort/frame required")
    with np.load(args.input, allow_pickle=False) as saved:
        corners, roles, ids, fallback = [
            saved[k].copy()
            for k in ("corners", "material", "source", "fallback_binding")
        ]
    selected = set(map(int, context["source_ordinals"]))
    if (
        corners.dtype != np.float32
        or corners.shape != (len(ids), 3, 14)
        or roles.shape != ids.shape
        or set(map(int, ids)) != selected
    ):
        raise ValueError("Whole reconstructed cohort required")
    original_bindings = context["triangle_binding"]
    if not np.array_equal(
        fallback, original_bindings[np.isin(original_bindings[:, 1], fallback[:, 1])]
    ):
        raise ValueError("Whole original fallback bindings required")
    chunks, order = {}, {}
    for identity in selected:
        group = ids == identity
        chunks[identity] = corners[group], roles[group]
        order[identity] = int(
            original_bindings[original_bindings[:, 1] == identity, 0].min()
        )
    result = write_product(chunks, order, selected, original["parts"], args.output)
    expected = producer["diagnostic_native_candidate"]
    equal = {
        key: result[key] == expected[key]
        for key in (
            "parts",
            "native_file_sha256",
            "native_description_sha256",
            "prepared_source_face_binding_sha256",
        )
    }
    report = {
        "reconstructed_input_sha256": hashlib.sha256(
            args.input.read_bytes()
        ).hexdigest(),
        "source_binding_sha256": provenance["source_binding_sha256"],
        "matching_native_bindings": equal,
        "native_arrays": len(result["native_file_sha256"]),
        "scope": "Whole current Linux diagnostic candidate only. Hash equality preserves its exact native input words/order; actual native image execution, other platforms, runtime World/revision and devices remain separate.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--producer", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

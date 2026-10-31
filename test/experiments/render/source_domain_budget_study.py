"""Compare bounded cached-domain restoration with whole recorded originals.

A domain-control limit bounds input occurrences, not triangulation or GEOS time.
No native World, image, material, shadow or device admission follows.
"""

import argparse
import hashlib
import io
import json
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from prepared_surface_recipes import decode
from source_domain_buffer import DomainBudget, restore_source
from source_patch_inputs import read_bound_sources


def checked(path, expected):
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError("Changed bound input")
    return raw


def run(args):
    files = [Path(__file__), Path(__file__).with_name("source_domain_buffer.py")]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)], text=True
    ):
        raise ValueError("Freeze measured sources first")
    report = json.loads((args.storage / "report.json").read_text())
    raw = checked(
        args.storage / "scalar.npz", report["packing_alternatives"]["scalar"]["sha256"]
    )
    with np.load(io.BytesIO(raw), allow_pickle=False) as z:
        arrays = {key: z[key] for key in z.files}
    recipes = {}
    for row in json.loads((args.storage / "recipes/index.json").read_text()):
        root = args.storage / "recipes"
        surfaces, _ = decode(checked(root / row["file"], row["sha256"]))
        with io.BytesIO(checked(root / row["source_file"], row["source_sha256"])) as f:
            identities = np.load(f, allow_pickle=False)
        recipes.update(zip(map(int, identities), surfaces, strict=True))
    original, context, provenance = read_bound_sources(args.original)
    if (
        report["input_provenance"]["source_binding_sha256"]
        != provenance["source_binding_sha256"]
    ):
        raise ValueError("Same recorded original cohort required")
    sources = set(map(int, arrays["source_ids"]))
    whole = set(map(int, context["source_ordinals"]))
    fallback = whole - sources
    accepted, refused, controls, saved = [], [], 0, 0
    before, after = 0.0, 0.0
    for identity in sorted(sources):
        began = time.process_time()
        baseline = restore_source(arrays, identity, recipes[identity])
        before += time.process_time() - began
        count = sum(
            len(ring.coords) - 1
            for p in baseline
            for polygon in shapely.get_parts(p.geometry.domain)
            for ring in (polygon.exterior, *polygon.interiors)
        )
        controls += count
        began = time.process_time()
        try:
            bounded = restore_source(
                arrays, identity, recipes[identity], max_controls=args.max_controls
            )
        except DomainBudget as error:
            elapsed = time.process_time() - began
            assert error.controls == count
            refused.append(identity)
            saved += count
        else:
            elapsed = time.process_time() - began
            assert [p.geometry.domain.wkb for p in baseline] == [
                p.geometry.domain.wkb for p in bounded
            ]
            accepted.append(identity)
        after += elapsed
    fallback.update(refused)
    labels = context["triangle_binding"]
    chosen = np.isin(labels[:, 1], sorted(fallback))
    assert set(map(int, labels[chosen, 1])) == fallback
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "scalar_sha256": hashlib.sha256(raw).hexdigest(),
        "max_controls": args.max_controls,
        "accepted_sources": len(accepted),
        "budget_refused_sources": refused,
        "whole_original_sources": len(fallback),
        "whole_original_triangles": int(chosen.sum()),
        "total_controls": controls,
        "controls_avoided": saved,
        "single_pass_baseline_restore_cpu_s": before,
        "single_pass_bounded_restore_cpu_s": after,
        "original_held_array_bytes": sum(
            value.nbytes for value in original.values() if isinstance(value, np.ndarray)
        ),
        "scope": __doc__,
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--storage", type=Path, required=True)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--max-controls", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

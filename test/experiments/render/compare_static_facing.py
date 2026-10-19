"""Conditional geometry comparison on unchanged actual building/transport meshes."""

import argparse
import collections
import hashlib
import itertools
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from facing_inputs import buildings, transport
from static_facing import bake, exact_facing, query

ENABLED = {
    "static": True,
    "one_sided": True,
    "trusted_winding": True,
    "model": np.eye(4),
}


def bits(mask, count):
    return np.array([bool(mask & (1 << i)) for i in range(count)])


def cameras(sphere, scene):
    centre, radius = sphere[:3], max(float(sphere[3]), 0.01)
    if scene is not None:
        yield "saved-eye", np.array(scene["eye_ecef"]) - scene["anchor_ecef"]
    yield "inside", centre
    # All six dominant directions, both near and far; exact face seams included.
    for scale in (1.000001, 1e6):
        for axis in range(3):
            for sign in (-1, 1):
                yield f"axis-{scale}", centre + scale * radius * np.eye(3)[axis] * sign
    for direction in itertools.product((-1, 1), repeat=3):
        yield "seam", centre + 10 * radius * np.array(direction)


def compare(model):
    started = time.perf_counter()
    vertices, indices = model["vertices"], model["indices"]
    result = {
        "key": model["key"],
        "group": model["group"],
        "provenance": model["provenance"],
        "triangles": len(indices),
        "vertices": len(np.unique(indices)),
        "clusters": len(model["ranges"]),
        "batches_before": 1,
        "batches_after": 1,
        "reasons": {},
        "cases": {},
        "cone_metadata_bytes": 8 * len(model["ranges"]),
        "mask_metadata_bytes": sum(
            6 * 8 * ((size + 63) // 64) for _, size in model["ranges"]
        ),
        "input_bytes": model["input_bytes"],
        "actual_runtime_state": "unknown: KEEP every index",
    }
    records, reasons = [], collections.Counter()
    for (first, size), sphere in zip(model["ranges"], model["spheres"], strict=True):
        triangles = vertices[indices[first : first + size]]
        record = bake(triangles, sphere)
        records.append(record)
        reasons[record["reason"]] += 1
        for name, camera in cameras(sphere, model["scene"]):
            signs = exact_facing(triangles, camera)
            count = result["cases"].setdefault(name, collections.Counter())
            cone, mask = query(record, camera, counters=count, **ENABLED)
            kept = bits(mask, size)
            if (signs[~kept] >= 0).any() or (not cone and (signs >= 0).any()):
                raise ValueError(
                    f"visible/tangent triangle discarded: {model['key']}/{first}/{name}"
                )
            if query(record, camera) != (True, (1 << size) - 1):
                raise ValueError("unknown runtime state must keep all triangles")
            count.update(
                queries=1,
                input_index_references=3 * size,
                cone_index_references=3 * size * int(cone),
                mask_index_references=3 * int(kept.sum()),
                exact_back_index_references=3 * int((signs < 0).sum()),
                input_unique_referenced_vertices=len(
                    np.unique(indices[first : first + size])
                ),
                cone_unique_referenced_vertices=int(cone)
                * len(np.unique(indices[first : first + size])),
                mask_unique_referenced_vertices=len(
                    np.unique(indices[first : first + size][kept])
                ),
                cone_compacted_index_write_bytes=12 * size * int(cone),
                mask_compacted_index_write_bytes=12 * int(kept.sum()),
                visible_discarded=0,
            )
    result["reasons"] = dict(reasons)
    result["python_seconds"] = time.perf_counter() - started
    return result, records


def run(args):
    from facing_images import image_comparison

    results, image_cases, groups = [], [], set()
    code = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in Path(__file__).parent.glob("*facing*.py")
    }
    for model in itertools.chain(buildings(args.buildings), transport(args.transport)):
        result, records = compare(model)
        results.append(result)
        if model["group"] not in groups and (
            model["scene"] is None or model["indices"].size > 3000
        ):
            image_cases.extend(image_comparison(model, records, args.output))
            groups.add(model["group"])
        print(
            json.dumps(
                {
                    "key": result["key"],
                    "group": result["group"],
                    "triangles": result["triangles"],
                    "reasons": result["reasons"],
                }
            ),
            flush=True,
        )
    report = {
        "experiment_source_sha256": code,
        "models": results,
        "images": image_cases,
        "inputs": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                args.buildings / "manifest.json",
                args.transport / "resident.json",
            )
        },
        "peak_process_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Conditional static one-sided trusted-winding original-object-space reference. No runtime eligibility, native camera/model arithmetic, alpha/shading/shadow, LOD, device time or hardware VS invocation proof. Unique vertices are summed within clusters, not a post-transform cache simulation. Batch records retained even when empty. Images use a diagnostic camera, not a saved native orientation.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--buildings", type=Path, required=True)
    parser.add_argument("--transport", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = args.output.resolve()
    if (
        root == Path(tempfile.gettempdir())
        or not root.is_relative_to(Path(tempfile.gettempdir()).resolve())
        or root.exists()
    ):
        parser.error("fresh explicit system-temp output directory required")
    root.mkdir(parents=True)
    run(args)

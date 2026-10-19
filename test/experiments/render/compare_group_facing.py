"""Compare six/eight normal groups inside original clusters, without reordering."""

import argparse
import collections
import hashlib
import itertools
import json
import platform
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_static_facing import ENABLED, bits, cameras
from facing_images import image_comparison
from facing_inputs import buildings, transport
from group_facing import bake_groups, select_groups
from static_facing import exact_facing


def compare(model):
    started = time.perf_counter()
    vertices, indices = model["vertices"], model["indices"]
    result = {
        "key": model["key"],
        "group": model["group"],
        "provenance": model["provenance"],
        "triangles": len(indices),
        "clusters": len(model["ranges"]),
        "batch_groups_before": 1,
        "batch_groups_after": 1,
        "variants": {},
    }
    partitions = ("faces6", "octants8")
    records = {name: [] for name in partitions}
    for name in partitions:
        result["variants"][name] = {
            "groups": 0,
            "packed_payload_bytes": 0,
            "aligned_payload_bytes": 0,
            "cases": {},
        }
    for (first, count), sphere in zip(model["ranges"], model["spheres"], strict=True):
        triangle_indices = indices[first : first + count]
        triangles = vertices[triangle_indices]
        for name, variant in records.items():
            record = bake_groups(triangles, sphere, name)
            variant.append(record)
            entry = result["variants"][name]
            entry["groups"] += len(record["groups"])
            entry["packed_payload_bytes"] += 8 + 24 * len(record["groups"])
            entry["aligned_payload_bytes"] += 8 + 32 * len(record["groups"])
        for case, camera in cameras(sphere, model["scene"]):
            oracle = exact_facing(triangles, camera)
            for name, variant in records.items():
                entry = result["variants"][name]["cases"].setdefault(
                    case, collections.Counter()
                )
                record = variant[-1]
                keep = select_groups(record, camera, counters=entry, **ENABLED)
                selected = bits(keep, count)
                if (oracle[~selected] >= 0).any():
                    raise ValueError(
                        f"visible/tangent removal: {model['key']}/{first}/{name}/{case}"
                    )
                if select_groups(record, camera) != (1 << count) - 1:
                    raise ValueError("unknown state must keep every source triangle")
                entry.update(
                    queries=1,
                    input_index_references=count * 3,
                    kept_index_references=keep.bit_count() * 3,
                    exact_back_index_references=3 * int((oracle < 0).sum()),
                    exact_whole_cluster_back_index_references=3
                    * count
                    * int((oracle < 0).all()),
                    input_unique_referenced_vertices=len(np.unique(triangle_indices)),
                    kept_unique_referenced_vertices=len(
                        np.unique(triangle_indices[selected])
                    ),
                    removed_visible_or_tangent=0,
                )
    result["python_seconds"] = time.perf_counter() - started
    return result, records


def diagnostic_images(model, records, output):
    model = dict(model)
    if model["scene"] is not None:
        points = model["vertices"][np.unique(model["indices"])]
        target, span = points.mean(0), np.ptp(points, axis=0).max()
        anchor = np.array(model["scene"]["anchor_ecef"])
        up = anchor / np.linalg.norm(anchor)
        east = np.cross([0, 0, 1], up)
        east /= np.linalg.norm(east)
        model["scene"] = dict(
            model["scene"],
            eye_ecef=(anchor + target + span * (up * 0.6 - east * 0.8)).tolist(),
        )

    def select(camera):
        selections = {}
        for name, variant in records.items():
            keep = np.ones(len(model["indices"]), bool)
            for (first, count), record in zip(model["ranges"], variant, strict=True):
                keep[first : first + count] = bits(
                    select_groups(record, camera, **ENABLED), count
                )
            selections[name] = keep
        return selections

    cases = image_comparison(model, None, output, selector=select)
    for case in cases:
        case["camera_scope"] = (
            "tight diagnostic pose, quantitative saved-eye results remain separate"
        )
        case["eye_ecef"] = (
            model["scene"]["eye_ecef"] if model["scene"] is not None else None
        )
    return cases


def run(args):
    root = Path(__file__).parent
    code = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in root.glob("*facing*.py")
    }
    models, images, imaged = [], [], set()
    for model in itertools.chain(buildings(args.buildings), transport(args.transport)):
        result, records = compare(model)
        models.append(result)
        if model["group"] not in imaged and (
            model["scene"] is None or model["indices"].size > 3000
        ):
            images.extend(diagnostic_images(model, records, args.output))
            imaged.add(model["group"])
        print(
            json.dumps({"key": model["key"], "triangles": result["triangles"]}),
            flush=True,
        )
    report = {
        "models": models,
        "images": images,
        "source_sha256": code,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "peak_process_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "input_manifest_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                args.buildings / "manifest.json",
                args.transport / "resident.json",
            )
        },
        "scope": "Conditional original-object-space static one-sided trusted-winding geometry. Unknown runtime state KEEP. No batch/job/triangle reorder, native layout, GPU time, HW VS invocations, native cache/allocations, alpha/shadows/lighting/full-world or device claim. Costs describe a possible group bitmap/compaction recipe, not native instructions. Python also bakes/queries unused static masks through the shared reference. Unique vertices summed per original cluster; pixel-centre diagnostic depth only.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--buildings", type=Path, required=True)
    parser.add_argument("--transport", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if target == temporary or not target.is_relative_to(temporary) or target.exists():
        parser.error("fresh explicit system-temp output required")
    target.mkdir(parents=True)
    run(args)

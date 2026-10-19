"""Offline geometry-only comparison of existing cluster parents, with full fallback."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from collections import Counter
from pathlib import Path

import fast_simplification
import numpy as np
from check_convex_surface_bound import check_bound
from check_surface_parent import check_parent
from clustered_surface_parent import cluster_parent
from convex_surface_bound import directed_bound
from facing_inputs import buildings
from far_product_cut import projection_error


def candidates(xyz, indices):
    started = time.perf_counter()
    referenced = np.unique(indices)
    xyz = xyz[referenced]
    indices = np.searchsorted(referenced, indices)
    parent = cluster_parent(xyz, indices, cell_m=0.5)
    check_parent(xyz, indices, parent)
    yield (
        "grid",
        parent.positions[parent.triangles].astype(float),
        time.perf_counter() - started,
        parent.error_m,
    )
    started = time.perf_counter()
    points, remap = np.unique(xyz[indices].reshape(-1, 3), axis=0, return_inverse=True)
    p, t = fast_simplification.simplify(
        points.astype(float), remap.reshape(-1, 3), target_reduction=0.5
    )
    yield (
        "qem",
        p.astype(np.float32)[t].astype(float),
        time.perf_counter() - started,
        None,
    )


def compare(source, target, observer, focal, *, max_tests, affine_bound):
    if not len(target) or len(target) >= len(source):
        return {"accepted": False, "status": "no_smaller_nonempty_parent"}
    all_points = np.concatenate((source.reshape(-1, 3), target.reshape(-1, 3)))
    box = np.stack((all_points.min(0), all_points.max(0)))
    if affine_bound is not None:
        affine_pixel = projection_error(
            box,
            observer,
            displacement_m=affine_bound,
            focal_px=focal,
            width=1280,
            height=720,
        )
        if affine_pixel <= 1:
            return {
                "accepted": True,
                "status": "affine",
                "error_upper_m": affine_bound,
                "pixel_error_upper": float(affine_pixel),
                "affine_accepted": True,
                "directions": [],
            }
    gap = np.maximum(np.maximum(box[0] - observer[1], observer[0] - box[1]), 0)
    distance = np.linalg.norm(gap)
    c = np.sqrt(1 + (640 / focal) ** 2 + (360 / focal) ** 2)
    # Tolerance proposal only; the final outward projection check is authoritative.
    tolerance = float(0.95 * distance / (c * (focal * c + 1)))
    result = {
        "tolerance_proposal_m": tolerance,
        "directions": [],
        "accepted": False,
        "affine_accepted": False,
    }
    for a, b in ((source, target), (target, source)):
        proof = directed_bound(
            a, b, tolerance_m=tolerance, max_tests=max_tests, max_depth=14
        )
        direction = {
            "complete": proof.complete,
            "upper_m": proof.upper_m,
            "status": proof.status,
            "work": proof.work,
        }
        if proof.complete:
            direction["audit"] = check_bound(a, b, proof)
        result["directions"].append(direction)
        if not proof.complete:
            result["status"] = proof.status
            return result
    bound = max(d["upper_m"] for d in result["directions"])
    pixel = projection_error(
        box, observer, displacement_m=bound, focal_px=focal, width=1280, height=720
    )
    result.update(
        error_upper_m=bound,
        pixel_error_upper=float(pixel) if np.isfinite(pixel) else None,
        accepted=bool(pixel <= 1),
        status="complete",
    )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-clusters", type=int)
    parser.add_argument("--max-tests", type=int, default=15000)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    out.mkdir(parents=True, exist_ok=False)
    totals = Counter()
    rows = []
    number = 0
    start = time.perf_counter()
    models = []
    for model in buildings(args.inputs):
        scene = model["scene"]
        eye = np.array(scene["eye_ecef"]) - scene["anchor_ecef"]
        observer = np.stack((eye - 64, eye + 64))
        fragments = []
        vertex_count = 0
        fallback_clusters = 0
        adopted = Counter()
        for cluster, (first, count) in enumerate(model["ranges"]):
            if args.max_clusters is not None and number >= args.max_clusters:
                break
            number += 1
            xyz = model["vertices"]
            ids = model["indices"][first : first + count]
            original = xyz[ids].astype(float)
            best = original
            winner = "original"
            for name, target, bake_seconds, affine_bound in candidates(xyz, ids):
                t = time.perf_counter()
                result = compare(
                    original,
                    target,
                    observer,
                    scene["focal_px"],
                    max_tests=args.max_tests,
                    affine_bound=affine_bound,
                )
                row = {
                    "model": model["key"],
                    "cohort": model["group"],
                    "cluster": cluster,
                    "original_first": first,
                    "original_count": count,
                    "recipe": name,
                    "candidate_triangles": len(target),
                    "python_seconds": time.perf_counter() - t,
                    "candidate_bake_seconds": bake_seconds,
                    **result,
                }
                rows.append(row)
                with (out / "rows.jsonl").open("a") as f:
                    f.write(json.dumps(row, allow_nan=False) + "\n")
                totals[name + ":original_triangles"] += count
                totals[name + ":candidate_triangles"] += len(target)
                totals[name + ":selected_triangles"] += (
                    len(target) if result["accepted"] else count
                )
                totals[name + ":accepted_clusters"] += int(result["accepted"])
                if name == "grid":
                    totals["grid:affine_selected_triangles"] += (
                        len(target) if result.get("affine_accepted", False) else count
                    )
                if result["accepted"] and len(target) < len(best):
                    best = target
                    winner = name
            adopted[winner] += 1
            fallback_clusters += winner == "original"
            totals["combined:original_triangles"] += count
            totals["combined:selected_triangles"] += len(best)
            fragments.append(best.astype(np.float32))
            vertex_count += 3 * len(best)
            if number % 50 == 0:
                print(
                    json.dumps(
                        {
                            "clusters": number,
                            "seconds": time.perf_counter() - start,
                            "totals": totals,
                        }
                    ),
                    flush=True,
                )
        if fragments:
            file = hashlib.sha256(model["key"].encode()).hexdigest() + ".npz"
            np.savez(out / file, triangles=np.concatenate(fragments))
            models.append(
                {
                    "key": model["key"],
                    "cohort": model["group"],
                    "provenance": model["provenance"],
                    "scene": model["scene"],
                    "file": file,
                    "original_triangles": sum(
                        n for _, n in model["ranges"][: len(fragments)]
                    ),
                    "selected_triangles": vertex_count // 3,
                    "adopted": adopted,
                    "fallback_clusters": fallback_clusters,
                }
            )
        if args.max_clusters is not None and number >= args.max_clusters:
            break
    report = {
        "scope": "Geometry-only cached primitive partition, no source-owned native parent or appearance/opacity inference. Missing source records remain original catalog references. Offline selection after decode, not a native pre-emission result.",
        "inputs": str(args.inputs.resolve()),
        "input_manifest_sha256": hashlib.sha256(
            (args.inputs / "manifest.json").read_bytes()
        ).hexdigest(),
        "models": models,
        "totals": totals,
        "clusters": number,
        "seconds": time.perf_counter() - start,
        "peak_process_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "fast_simplification": fast_simplification.__version__,
        "code_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__),
                Path(__file__).with_name("convex_surface_bound.py"),
                Path(__file__).with_name("check_convex_surface_bound.py"),
            )
        },
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(totals), flush=True)


if __name__ == "__main__":
    main()

"""Separate absent-at-capture surfaces from reconstruction misses using geometry.

This offline oracle shoots back to the capture eye through each current Fine
hit. The cache renderer never uses these source queries. Opaque triangle
visibility is tested; native material opacity/PBR is not evaluated.
"""

import argparse
import hashlib
import json
import tempfile
import time
from pathlib import Path

import numpy as np
from depth_cluster import RECORD, prepare
from raster_depth_cluster import raster
from source_billboard import image_rays
from source_far_assets import load_source
from source_far_reprojection import intersect_source, prepare_source


def classify(source, index, cache, view, lo, width, height):
    origins, directions = image_rays(
        view, np.array([lo, lo + [width, height]]), width, height
    )
    depth, _ = intersect_source(source, origins, directions, prepared=index)
    cached, _, _ = raster(cache, prepare(cache), view, lo, width, height)
    ids = np.flatnonzero(np.isfinite(depth))
    points = origins[ids] + depth[ids, None] * directions[ids]
    cap_eye, cap_basis, focal, screen_width, screen_height = cache["view"]
    local = (points - cap_eye) @ cap_basis.T
    xy = np.divide(
        local[:, :2],
        local[:, 2:3],
        out=np.zeros((len(ids), 2)),
        where=local[:, 2:3] > 0.1,
    )
    xy = [screen_width / 2, screen_height / 2] + focal * xy * [1, -1]
    inside = (
        (local[:, 2] > 0.1)
        & (xy >= cache["lo"]).all(1)
        & (xy < cache["lo"] + [cache["width"], cache["height"]]).all(1)
    )
    # Capture-camera-Z ray parameter, not a unit segment with an incorrect near.
    cap_rays = (points[inside] - cap_eye) / local[inside, 2:3]
    cap_origins = np.broadcast_to(cap_eye, cap_rays.shape)
    old_depth, _ = intersect_source(source, cap_origins, cap_rays, prepared=index)
    visible = np.zeros(len(ids), bool)
    visible[inside] = abs(old_depth - local[inside, 2]) <= 1e-5
    missing = ~np.isfinite(cached[ids])
    return {
        "fine_subpixels": len(ids),
        "missing_subpixels": int(missing.sum()),
        "missing_previously_visible": int((missing & visible).sum()),
        "missing_previously_occluded_or_backfacing": int(
            (missing & inside & ~visible).sum()
        ),
        "missing_outside_capture": int((missing & ~inside).sum()),
        "fine_points_previously_occluded_or_backfacing": int((inside & ~visible).sum()),
        "cache_hits_over_previously_invisible_points": int(
            (~missing & inside & ~visible).sum()
        ),
        "capture_truth_rays": len(cap_rays),
    }


def read_cache(root, capture):
    directory = root / ("capture-" + capture["tag"])
    d = json.loads((directory / "descriptor.json").read_text())
    data = np.load(directory / "runtime.npy", allow_pickle=False)
    if data.dtype != RECORD:
        raise ValueError("different diagnostic cache layout")
    return {
        "data": data,
        "view": (
            np.array(d["eye"]),
            np.array(d["basis"]),
            d["focal_px"],
            *d["image_width_height"],
        ),
        "lo": np.array(d["lo"]),
        "width": d["capture_width_height"][0],
        "height": d["capture_width_height"][1],
        "supersample": d["supersample"],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--comparison", type=Path, required=True)
    parser.add_argument("--case", nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    report = json.loads(args.comparison.read_text())
    roots = [
        Path(p).parent for p in report["input_sha256"] if p.endswith("/input.json")
    ]
    fine = next(
        p
        for p in roots
        if json.loads((p / "input.json").read_text())["generation_error_px"] == 0
    )
    for name in ("input.json", "mesh/receipt.json", "subset.index", "subset.forms"):
        p = fine / name
        if hashlib.sha256(p.read_bytes()).hexdigest() != report["input_sha256"][str(p)]:
            raise ValueError("reference input changed")
    source = load_source(fine)
    index = prepare_source(source)
    out.mkdir(parents=True, exist_ok=False)
    rows = []
    for tag in args.case:
        case = next(c for c in report["cases"] if c["tag"] == tag)
        capture = next(c for c in report["captures"] if tag.startswith(c["tag"] + "-"))
        cache = read_cache(args.comparison.parent, capture)
        view = (
            np.array(case["eye"]),
            np.array(case["basis"]),
            report["focal_px"],
            1280,
            720,
        )
        lo, hi = np.array(case["roi"])
        start = time.perf_counter()
        result = classify(source, index, cache, view, lo, *(hi - lo))
        result.update(tag=tag, seconds_including_oracles=time.perf_counter() - start)
        if result["missing_subpixels"] != case["raw_missing_subpixels"]:
            raise ValueError("cached renderer output changed")
        if (
            sum(
                result[k]
                for k in (
                    "missing_previously_visible",
                    "missing_previously_occluded_or_backfacing",
                    "missing_outside_capture",
                )
            )
            != result["missing_subpixels"]
        ):
            raise AssertionError("missing classifications do not partition raw loss")
        rows.append(result)
        print(json.dumps(result), flush=True)
    (out / "report.json").write_text(
        json.dumps(
            {
                "scope": "Offline opaque-geometry truth, 1e-5m depth equality threshold; old visibility distinct from sample presence. Not runtime cache detection, material opacity or PBR acceptance. Source queries never enter candidate rendering.",
                "comparison_sha256": hashlib.sha256(
                    args.comparison.read_bytes()
                ).hexdigest(),
                "code_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                "cases": rows,
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()

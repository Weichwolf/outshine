"""Compare full site enumeration with lazy camera demand and cached products."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from demand import Camera
from stand_plan import Products, StandPlan


def enumerate_needed(plan, camera, distance):
    pending, needed, generated = [plan.root], {}, 0
    while pending:
        cell = pending.pop()
        children = plan.children(cell)
        if children:
            pending.extend(children)
            continue
        sites = plan.sites(cell)
        generated += len(sites["positions"])
        for index in np.flatnonzero(plan.needed_members(sites, camera, distance)):
            needed[sites["ids"][index]] = sites["positions"][index]
    return needed, generated


def equal_sites(left, right):
    assert left.keys() == right.keys()
    for identity in left:
        np.testing.assert_array_equal(left[identity], right[identity])


def check_coverage(cells):
    for left in cells:
        for right in cells:
            if right.level <= left.level:
                continue
            shift = right.level - left.level
            assert (right.x >> shift, right.y >> shift) != (left.x, left.y)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    names = ("oak", "beech", "birch")
    sources = [Path("src/assets/world/species") / f"{name}.json" for name in names]
    profiles = [json.loads(path.read_text()) for path in sources]
    heights = [profile["height_m"] for profile in profiles]
    spreads = [profile["spread_m"] for profile in profiles]
    plan = StandPlan((2460, 1024, 612), heights, spreads)
    children = plan.children(plan.root)
    np.testing.assert_array_equal(
        np.sum([cell.counts for cell in children], axis=0), plan.root.counts
    )
    products = Products(plan)
    root = products.field(plan.root)
    child_fields = [products.field(cell) for cell in children]
    for name in ("crown_area_m2", "height_sum_m"):
        np.testing.assert_allclose(
            sum(child[name] for child in child_fields), root[name], rtol=1e-12
        )
    camera = Camera((0, -80, 2), (0, 100, 8))
    began = time.perf_counter()
    reference, enumerated = enumerate_needed(plan, camera, 40)
    full_ms = (time.perf_counter() - began) * 1000
    products = Products(plan)
    began = time.perf_counter()
    first = products.select(camera)
    lazy_ms = (time.perf_counter() - began) * 1000
    equal_sites(reference, first["sites"])
    check_coverage(first["cells"])
    generated = products.generated.copy()
    second = products.select(camera)
    assert products.generated == generated
    equal_sites(first["sites"], second["sites"])
    moved_camera = Camera((8, -80, 2), (8, 100, 8))
    moved = products.select(moved_camera)
    moved_reference, _ = enumerate_needed(plan, moved_camera, 40)
    equal_sites(moved_reference, moved["sites"])
    common = first["sites"].keys() & moved["sites"].keys()
    assert common
    for identity in common:
        np.testing.assert_array_equal(
            first["sites"][identity], moved["sites"][identity]
        )
    limited = Products(plan).select(camera, node_budget=1)
    assert limited["visited"] == 1 and limited["deferred"] == 1
    assert limited["cells"] == (plan.root,) and not limited["sites"]
    small = Products(plan).select(camera, node_budget=32)
    assert small["visited"] <= 32
    check_coverage(small["cells"])
    constrained = Products(plan)
    partial = constrained.select(camera, site_budget=40)
    assert 0 < constrained.generated["members"] <= 40
    assert partial["deferred"] > 0
    check_coverage(partial["cells"])
    resumed = constrained.select(camera)
    equal_sites(reference, resumed["sites"])
    assert constrained.generated == generated
    warmed = constrained.generated.copy()
    equal_sites(reference, constrained.select(camera, site_budget=1)["sites"])
    assert constrained.generated == warmed
    front = Camera((0, 0, 0), (0, 1, 0))
    assert front.visible(np.array([-1, 10, -1]), np.array([1, 12, 1]))
    assert not front.visible(np.array([-1, -12, -1]), np.array([1, -10, 1]))
    assert front.visible(np.array([-1, -1, -1]), np.array([1, 1, 1]))
    far = Camera((0, -15000, 3000), (0, 0, 10))
    far_cases = []
    for total in (4096, 1000000, 4000000):
        counts = (total // 2, total // 4, total - total // 2 - total // 4)
        far_products = Products(StandPlan(counts, heights, spreads, levels=8))
        began = time.perf_counter()
        selected = far_products.select(far)
        elapsed = (time.perf_counter() - began) * 1000
        assert len(selected["cells"]) == 1 and sum(selected["cells"][0].counts) == total
        assert (
            far_products.generated["sites"] == 0
            and far_products.generated["members"] == 0
        )
        far_cases.append(
            {
                "logical_members": total,
                "generated": far_products.generated,
                "visited": selected["visited"],
                "elapsed_ms": elapsed,
            }
        )
    behind = Products(plan)
    assert not behind.select(Camera((0, -15000, 3000), (0, -16000, 3000)))["cells"]
    assert not behind.records
    report = {
        "full_enumeration_ms": full_ms,
        "lazy_selection_ms": lazy_ms,
        "full_generated_members": enumerated,
        "lazy_generated": generated,
        "required_members": len(first["sites"]),
        "selected_cells": len(first["cells"]),
        "stable_after_move": len(common),
        "cache_hit_generated_products": 0,
        "exact_same_required_sites": True,
        "conserved_species_counts_and_area": True,
        "bounded_coverage": True,
        "bounded_site_generation_with_coarse_fallback": True,
        "offscreen_generated_products": 0,
        "far_cases": far_cases,
        "image_lod_quality_proven": False,
        "persistent_cache_integrated": False,
        "shadow_and_occlusion_integrated": False,
        "numpy_version": np.__version__,
        "implementation_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in Path(__file__).parent.glob("*.py")
        },
        "profile_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in sources
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
        ),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
    }
    (args.output / "demand.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, separators=(",", ":")), flush=True)


if __name__ == "__main__":
    main()

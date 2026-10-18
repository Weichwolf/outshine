"""Reject two real-terrain candidates without admitting their source network."""

import argparse
import importlib.metadata
import json
import resource
import subprocess
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_outer_inputs import FIXTURE, load
from stair_outer_verify import verify as verify_network
from stair_terrain_audit import inspect
from stair_terrain_display import scenes
from stair_terrain_layout import gentle_candidate, partition, selected_surfaces
from stair_terrain_solid import protected_excavation, retaining_body, split_roots

PALETTE = FIXTURE.with_name("research-palette.json")


def frozen():
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", "test/experiments/transport"],
        text=True,
    ).strip():
        raise ValueError("frozen clean research required")
    return subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()


def generate(args):
    commit = frozen()
    if args.output.exists():
        raise ValueError("fresh explicit temporary output required")
    verify_network(
        SimpleNamespace(
            output=args.network_products, fixture=FIXTURE, palette_scene=PALETTE
        )
    )
    parent = json.loads((args.network_products / "comparison.json").read_text())
    with np.load(args.network_products / "after/body.npz") as data:
        data = dict(data)
    recipe = json.loads(FIXTURE.read_text())
    dem = FIXTURE.with_name(recipe["elevation"]["window_file"])
    with np.load(dem) as raster:
        raster = dict(raster)
    _case, rows, _nodes, _stairs, context, _recipe = load()
    ground, upper = selected_surfaces(data, rows)
    began = time.perf_counter()
    layout, layout_note = partition(ground, context, raster)
    gentle = gentle_candidate(layout)
    split = split_roots(layout)
    retaining, components = retaining_body(split)
    retained_s = time.perf_counter() - began
    began_cut = time.perf_counter()
    excavated, actual, cut_note = protected_excavation(retaining, ground, upper)
    cut_s = time.perf_counter() - began_cut
    road = Mesh(data["body_vertices"], data["body_faces"])
    proof = inspect(retaining, actual, ground, upper, road)
    args.output.mkdir(parents=True)
    np.savez_compressed(args.output / "terrain-layout.npz", **split)
    np.savez_compressed(
        args.output / "retaining.npz",
        vertices=retaining.vertices,
        faces=retaining.triangles,
        **components,
    )
    np.savez_compressed(
        args.output / "excavated-double.npz",
        vertices=excavated.vertices,
        faces=excavated.triangles,
    )
    np.savez_compressed(
        args.output / "excavated-float32.npz",
        vertices=actual.vertices,
        faces=actual.triangles,
    )
    result = {
        "commit": commit,
        "network_products": str(args.network_products.resolve()),
        "network_report_sha256": digest(args.network_products / "comparison.json"),
        "source_body_sha256": parent["after"]["body_sha256"],
        "fixture_sha256": digest(FIXTURE),
        "dem_window_sha256": digest(dem),
        "source_catalog_sha256": parent["source_catalog_sha256"],
        "candidate_source_ids": parent["candidate_source_ids"],
        "admitted_source_ids": [],
        "layout": layout_note,
        "gentle": gentle,
        "excavation": cut_note,
        "proof": proof,
        "retaining_pipeline_seconds": retained_s,
        "excavation_seconds": cut_s,
        "generation_and_audit_seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "packages": {
            k: importlib.metadata.version(k)
            for k in ("numpy", "scipy", "shapely", "clarabel", "manifold3d")
        },
        "display": scenes(
            args.output, args.network_products, retaining, actual, PALETTE
        ),
        "artifacts": {
            name: digest(args.output / name)
            for name in (
                "terrain-layout.npz",
                "retaining.npz",
                "excavated-double.npz",
                "excavated-float32.npz",
            )
        },
        "approved_terrain": False,
        "visual_approval": "pending_actual_image_inspection; never implied by solver/kernel success",
        "scope": "Rejected local fixed-road terrain study; no native/runtime/whole-world approval.",
        "cost_limits": "120CPU-s/3GiB; solve8s; per-clearance20s/1M candidate pairs. Native allocation counts/internal copied bytes unmeasured.",
    }
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in (
                    "commit",
                    "approved_terrain",
                    "generation_and_audit_seconds",
                    "peak_rss_kib",
                )
            }
        )
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--network-products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    generate(arguments())

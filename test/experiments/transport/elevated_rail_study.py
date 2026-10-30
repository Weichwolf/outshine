"""Compare bounded flat/affine elevated rail slabs against held whole transport."""

import argparse
import hashlib
import json
import os
import platform
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from elevated_rail_audit import inspect
from elevated_rail_inputs import lower_world, sources
from elevated_rail_plane import fit_plane
from outshine_ref.contracts import Mesh
from process_memory import peak_rss_bytes
from stair_local_replay import load_chain
from stair_module_inputs import FIXTURE
from stair_terrain_experiment import frozen
from surface_solid_methods import close_surface


def old_arrays(assets):
    return {
        f"{a.identity}/{key}": hashlib.sha256(memoryview(value).cast("B")).hexdigest()
        for a in assets
        for key, value in a.data.items()
    }


def compare(recipe, rows, top, domain, points, groups):
    axes = np.vstack([np.diff(r["xy"], axis=0) for r in rows])
    result, arrays = {}, {}
    for method in ("flat", "tilted"):
        cpu, wall = time.process_time(), time.perf_counter()
        origin, coefficients, fit = fit_plane(
            points,
            domain,
            axes,
            along=recipe["along_grade"],
            cross=recipe["cross_grade"],
            method=method,
        )
        vertices = top.vertices.copy()
        vertices[:, 2] = (vertices[:, :2] - origin) @ coefficients[:2] + coefficients[2]
        body = close_surface(Mesh(vertices, top.triangles), recipe["deck_thickness_m"])
        cpu, wall = time.process_time() - cpu, time.perf_counter() - wall
        proof = inspect(
            body,
            top.triangles,
            rows,
            groups,
            recipe["deck_thickness_m"],
            recipe["along_grade"],
            recipe["cross_grade"],
        )
        result[method] = {
            "fit": fit,
            "build_cpu_s": cpu,
            "build_wall_s": wall,
            "proof": proof,
        }
        arrays[method + "_vertices"] = body.vertices
        arrays[method + "_triangles"] = body.triangles
    return result, arrays


def run(args):
    commit = frozen()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp output required")
    cpu, wall = time.process_time(), time.perf_counter()
    view, assets, piers, _contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    recipe, context, available, rows, top, domain, layout = sources(
        args.fixture, manifest["source_ids"]
    )
    if context["origin_lat_lon"] != manifest["origin_lat_lon"]:
        raise ValueError("Original sources and held world frames differ")
    before = old_arrays(assets)
    decoded = (time.process_time() - cpu, time.perf_counter() - wall)
    points, groups, lower_work = lower_world(
        view,
        assets,
        piers,
        available,
        domain,
        recipe["deck_thickness_m"] + recipe["construction_reserve_m"],
    )
    result, arrays = compare(recipe, rows, top, domain, points, groups)
    if old_arrays(assets) != before:
        raise ValueError("Held original transport arrays changed")
    report = {
        "commit": commit,
        "fixture_sha256": digest(args.fixture),
        "reference_fixture_sha256": recipe["reference_fixture_sha256"],
        "resident": str(args.resident.resolve()),
        "resident_sha256": digest(args.resident / "resident.json"),
        "prior_modules": [
            {"path": str(p.resolve()), "report_sha256": digest(p / "comparison.json")}
            for p in args.prior_module
        ],
        "origin_lat_lon": manifest["origin_lat_lon"],
        "old_source_ids": manifest["source_ids"],
        "old_road_arrays": before,
        "source_ids": recipe["source_ids"],
        "original_properties": [r["original_properties"] for r in rows],
        "whole_layout": layout,
        "lower_work": lower_work,
        "methods": result,
        "cost": {
            "decode_cpu_s": decoded[0],
            "decode_wall_s": decoded[1],
            "analysis_cpu_s": time.process_time() - cpu - decoded[0],
            "analysis_wall_s": time.perf_counter() - wall - decoded[1],
            "process_peak_rss_bytes": peak_rss_bytes(),
            "held_output_array_bytes": sum(v.nbytes for v in arrays.values()),
            "constraint_array_bytes": points.nbytes,
            "loadavg": os.getloadavg(),
            "nice": os.getpriority(os.PRIO_PROCESS, 0),
            "platform": platform.platform(),
        },
        "scope": "Separate whole raised rail slab candidates against unchanged held transport/terrain, no Ground replay, new terrain/source/routing/native/World/structural/curvature/C1/consumer/device authority. New piers, hardware, outer continuation and full Buildings/Plants integration remain open.",
    }
    args.output.mkdir(parents=True)
    np.savez_compressed(args.output / "candidates.npz", **arrays)
    report["candidates_sha256"] = digest(args.output / "candidates.npz")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": commit,
                "old_sources": len(manifest["source_ids"]),
                "whole_new_sources": len(rows),
                "output": str(args.output),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resident", type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument(
        "--fixture",
        type=Path,
        default=FIXTURE.with_name("stair-elevated-rail-vienna.json"),
    )
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

"""Continuous source-bed contacts and bounded constructed zones in the real Vienna case."""

import argparse
import contextlib
import io
import json
import resource
import time
from functools import partial
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from camera_height_inputs import digest
from stair_blend_audit import inspect
from stair_blend_display import scenes
from stair_blend_metrics import earthwork
from stair_blend_plan import prepare
from stair_blend_solve import candidate, solve, validate_candidate
from stair_network_inputs import sample_xy
from stair_outer_inputs import FIXTURE, load
from stair_outer_verify import verify as verify_network
from stair_terrain_experiment import PALETTE, frozen
from stair_terrain_solid import retaining_body, split_roots


def inputs(products):
    with contextlib.redirect_stdout(io.StringIO()):
        verify_network(
            SimpleNamespace(output=products, fixture=FIXTURE, palette_scene=PALETTE)
        )
    source = json.loads((products / "comparison.json").read_text())
    data = dict(np.load(products / "after/body.npz"))
    recipe = json.loads(FIXTURE.read_text())
    dem = FIXTURE.with_name(recipe["elevation"]["window_file"])
    raster = dict(np.load(dem))
    _case, rows, _nodes, _stairs, context, _recipe = load()
    return data, rows, context, raster, source, dem


def stage(output, name, layout, ground, upper, footprint, smoothing):
    heights, model, note = candidate(layout, upper, footprint, smoothing=smoothing)
    built = validate_candidate(layout, heights, model, note)
    note["candidate_exported"] = heights is not None
    if heights is None:
        return None, None, {"solve": note}
    if not note["accepted_field"]:
        np.savez_compressed(output / (name + "-field.npz"), heights=heights)
        return None, None, {"solve": note}
    body, parts = retaining_body(dict(layout, targets=heights))
    roles = np.zeros(len(layout["faces"]), bool)
    roles[~layout["ground"]] = built
    proof = inspect(body, parts, ground, upper)
    np.savez_compressed(
        output / (name + "-field.npz"), heights=heights, constructed=roles
    )
    np.savez_compressed(
        output / (name + "-body.npz"),
        vertices=body.vertices,
        faces=body.triangles,
        **parts,
    )
    return (
        body,
        roles,
        {"solve": note, "physical": proof, "earthwork": earthwork(layout, heights)},
    )


def generate(args):
    commit = frozen()
    if args.output.exists():
        raise ValueError("fresh explicit temporary output required")
    data, rows, context, raster, source, dem = inputs(args.network_products)
    began = time.perf_counter()
    initial, layout, ground, upper, footprint, plan = prepare(
        data, rows, context, raster, partial(sample_xy, context=context, raster=raster)
    )
    _, _, control = solve(layout, upper, footprint, structure_limit=2.0)
    args.output.mkdir(parents=True)
    before, _parts = retaining_body(split_roots(initial))
    np.savez_compressed(
        args.output / "before-body.npz",
        vertices=before.vertices,
        faces=before.triangles,
    )
    np.savez_compressed(args.output / "plan.npz", **layout)
    data_fit, data_roles, first = stage(
        args.output, "data", layout, ground, upper, footprint, 0
    )
    after, roles, second = stage(
        args.output, "membrane", layout, ground, upper, footprint, 16
    )
    if after is None:
        raise ValueError("bounded membrane candidate failed: " + str(second))
    bodies = [("before", before, None), ("after", after, roles)]
    if data_fit is not None:
        bodies.append(("data", data_fit, data_roles))
    result = {
        "commit": commit,
        "network_products": str(args.network_products.resolve()),
        "network_report_sha256": digest(args.network_products / "comparison.json"),
        "source_body_sha256": source["after"]["body_sha256"],
        "fixture_sha256": digest(FIXTURE),
        "dem_window_sha256": digest(dem),
        "source_catalog_sha256": source["source_catalog_sha256"],
        "candidate_source_ids": source["candidate_source_ids"],
        "admitted_source_ids": [],
        "plan": plan,
        "shallower_structure_control": control,
        "data": first,
        "membrane": second,
        "pipeline_before_display_seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "display": scenes(args.output, args.network_products, bodies),
        "artifacts": {
            name: digest(args.output / name)
            for name in (
                "plan.npz",
                "before-body.npz",
                "data-field.npz",
                "membrane-field.npz",
                "membrane-body.npz",
            )
            if (args.output / name).exists()
        },
        "scope": "Local continuous terrain with a derived Float32 contact envelope; other sources, outer ports, swept users, statics and native integration remain unadmitted.",
    }
    if data_fit is not None:
        result["artifacts"]["data-body.npz"] = digest(args.output / "data-body.npz")
    (args.output / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in ("commit", "pipeline_before_display_seconds", "peak_rss_kib")
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
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    generate(arguments())

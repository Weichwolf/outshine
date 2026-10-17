"""Prepare independently held verified road bodies beside existing resident terrain."""

import argparse
import json
import resource
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from stair_module_cache import parent_report
from stair_module_experiment import write_json
from stair_module_inputs import FIXTURE, cached, read_arrays
from stair_patch_inputs import frame_for, fresh, from_field, load_prepared
from stair_terrain_experiment import frozen


def save_road(output, index, keys, rows, road, owners, ground):
    name = "road-" + str(index) + ".npz"
    np.savez_compressed(output / name, **road, owners=owners, ground=ground)
    return {
        "file": name,
        "sha256": digest(output / name),
        "source_ids": keys,
        "rows": rows,
    }


def root_road(args, state):
    keys = state["report"]["candidate_source_ids"]
    body = state["road"]
    old_xyz = np.c_[state["plan"]["xy"], state["heights"]]
    beds = {
        p.tobytes() for p in old_xyz[state["plan"]["faces"][state["plan"]["ground"]]]
    }
    ground = np.array(
        [p.tobytes() in beds for p in body["body_vertices"][body["bottom_faces"]]]
    )
    if ground.sum() != state["plan"]["ground"].sum():
        raise ValueError("original source-ground ownership changed")
    owners = (
        body["face_sources"].astype(np.uint64)[:, None]
        & (np.uint64(1) << np.arange(len(keys), dtype=np.uint64))
    ) != 0
    fields = ("body_vertices", "body_faces", "top_faces", "bottom_faces")
    rows = root_rows(args.reference, state)
    return save_road(
        args.output, 0, keys, rows, {k: body[k] for k in fields}, owners, ground
    )


def root_rows(reference, state):
    from stair_cycle_inputs import load

    if digest(reference) != state["report"]["fixture_sha256"]:
        raise ValueError("verified root recipe required for profile provenance")
    _case, rows, _nodes, _stairs, _context, _sample = load(reference)
    if [r["source_id"] for r in rows] != state["report"]["candidate_source_ids"]:
        raise ValueError("original ordered root source profiles required")
    provenance = {
        "fixture_sha256": digest(reference),
        "body_sha256": state["report"]["after"]["body_sha256"],
    }
    return [
        {k: row[k] for k in ("source_id", "xy", "width_m")}
        | {"resident_profile_provenance": provenance}
        for row in rows
    ]


def prepare(args):
    commit = frozen()
    fresh(args.output)
    view, terrain = load_prepared(args.terrain)
    ref = {
        "reference_fixture": args.reference.name,
        "reference_fixture_sha256": digest(args.reference),
    }
    state = cached(args.base, args.supports, ref)
    origin = state["support_report"]["origin_lat_lon"]
    root = from_field(
        state["plan"], read_arrays(args.base / "membrane-field.npz"), frame_for(origin)
    )
    if (
        frame_for(origin) != view.patches[0].frame
        or not 1 <= len(args.cached_module) <= 32
        or not np.array_equal(root.mesh.vertices, view.patches[0].mesh.vertices)
        or not np.array_equal(root.mesh.triangles, view.patches[0].mesh.triangles)
        or any(
            not np.array_equal(v, view.patches[0].face_data[k])
            for k, v in root.face_data.items()
        )
    ):
        raise ValueError(
            "same original terrain root/frame and bounded road parents required"
        )
    state["before_display_path"] = args.supports / "after-display.npz"
    args.output.mkdir()
    roads = [root_road(args, state)]
    history = []
    for index, (path, fixture) in enumerate(args.cached_module, 1):
        report, recipe = parent_report(path, fixture, state, history)
        data = read_arrays(path / "module.npz")
        count = len(data["surface_vertices"])
        road = {
            "body_vertices": data["body_vertices"],
            "body_faces": data["body_faces"],
            "top_faces": data["surface_faces"],
            "bottom_faces": data["surface_faces"] + count,
        }
        if "surface_materials" in data:
            road["surface_materials"] = data["surface_materials"]
        rows = json.loads((path / "planning.json").read_text())["rows"]
        roads.append(
            save_road(
                args.output,
                index,
                recipe["source_ids"],
                rows,
                road,
                data["owners"],
                np.ones(len(data["surface_faces"]), bool),
            )
        )
        history.append(
            {
                "report_sha256": digest(path / "comparison.json"),
                "fixture_sha256": digest(fixture),
            }
        )
        state.update(
            report={"candidate_source_ids": report["candidate_source_ids"]},
            baseline_report_sha256=digest(path / "comparison.json"),
            before_display_path=path / "after-display.npz",
        )
    if (
        history != terrain["source_reports"]
        or state["report"]["candidate_source_ids"] != terrain["source_ids"]
    ):
        raise ValueError("road and terrain histories differ")
    support = args.supports / (state["support_report"]["selected"] + ".npz")
    last = args.cached_module[-1][0]
    manifest = {
        "version": 1,
        "commit": commit,
        "origin_lat_lon": origin,
        "frame": view.patches[0].frame.identifier,
        "roads": roads,
        "source_ids": terrain["source_ids"],
        "terrain": str(args.terrain.resolve()),
        "terrain_sha256": digest(args.terrain / "patches.json"),
        "support_mesh": str(support.resolve()),
        "support_sha256": digest(support),
        "catalog": str((last / "source-catalog.json").resolve()),
        "catalog_sha256": digest(last / "source-catalog.json"),
        "height_floor_m": float(read_arrays(last / "world-field.npz")["heights"].min()),
        "base_display": str((args.base / "after/display-body.npz").resolve()),
        "base_display_sha256": digest(args.base / "after/display-body.npz"),
        "before_display": str((last / "after-display.npz").resolve()),
        "before_display_sha256": digest(last / "after-display.npz"),
        "support_scene": str((args.supports / "after-overview-scene.json").resolve()),
        "support_scene_sha256": digest(args.supports / "after-overview-scene.json"),
        "scope": "Prepared finite resident research assets, not a runtime storage/cache ABI.",
    }
    write_json(args.output / "resident.json", manifest)
    return manifest


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--supports", type=Path, required=True)
    parser.add_argument("--terrain", type=Path, required=True)
    parser.add_argument(
        "--reference",
        type=Path,
        default=FIXTURE.with_name("stair-north-bridges-vienna.json"),
    )
    parser.add_argument(
        "--cached-module", type=Path, nargs=2, action="append", required=True
    )
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    prepare(arguments())

"""Read saved eastern cycle bodies, whole caps and quantized terrain; never fit."""

import json
import resource

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_blend_audit import inspect
from stair_blend_metrics import earthwork
from stair_blend_solve import check_heights
from stair_blend_verify import field_contract
from stair_context_verify import stage
from stair_cycle_contacts import contacts
from stair_cycle_experiment import arguments, base
from stair_cycle_inputs import FIXTURE, load
from stair_network_geometry import interactions, vertex_links
from stair_outer_context import catalog
from stair_outer_geometry import outer_ends
from stair_terrain_experiment import frozen
from stair_terrain_layout import selected_surfaces


def displayed(output, report, body, roles, palette_scene):
    for name in ("before", "after"):
        if digest(output / (name + "-display.npz")) != report[name + "_display_sha256"]:
            raise ValueError("actual cycle display binding changed")
    original = dict(np.load(output / "after/display-body.npz"))
    shown = dict(np.load(output / "after-display.npz"))
    for key, value in original.items():
        if not np.array_equal(shown[key][: len(value)], value):
            raise ValueError("actual cycle road display prefix changed")
    first, offset = len(original["triangles"]), len(original["vertices"])
    xyz = body.vertices.astype(np.float32)
    if not np.array_equal(shown["vertices"][offset:], xyz) or not np.array_equal(
        shown["triangles"][first:] - offset, body.triangles
    ):
        raise ValueError("shown terrain differs from actual verified export")
    p = xyz.astype(float)[body.triangles]
    normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    slots = np.where(abs(normals[:, 2]) < np.linalg.norm(normals, axis=1) * 1e-6, 6, 1)
    slots[np.flatnonzero(roles)] = 6
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    expected = np.repeat(normals, 3, axis=0).astype(shown["normals"].dtype)
    palette = np.array(json.loads(palette_scene.read_text())["colours"])
    if not np.array_equal(
        shown["face_base_colour_linear_rgba"][first:], palette[slots]
    ):
        raise ValueError("constructed terrain colour ownership changed")
    if not np.array_equal(
        shown["normals"][3 * first :], expected
    ) or not np.array_equal(shown["materials"][first:], slots):
        raise ValueError("actual terrain normals/material roles changed")


def verify(args):
    commit = frozen()
    report = json.loads((args.output / "comparison.json").read_text())
    if report["fixture_sha256"] != digest(FIXTURE) or report["baseline"] != base(args):
        raise ValueError("original cycle inputs/baseline changed")
    for name, expected in report["artifacts"].items():
        if digest(args.output / name) != expected:
            raise ValueError("saved cycle artifact changed")
    case, rows, nodes, stairs, context, sample = load()
    if (
        report["candidate_source_ids"] != [r["source_id"] for r in rows]
        or report["admitted_source_ids"]
    ):
        raise ValueError("source ownership or explicit non-admission changed")
    inventory = catalog(case, rows, context)
    catalog_path = args.output / "source-catalog.json"
    if digest(catalog_path) != report[
        "source_catalog_sha256"
    ] or inventory != json.loads(catalog_path.read_text()):
        raise ValueError("whole original payload inventory changed")
    road = stage(
        args,
        "after",
        rows,
        nodes,
        report["after"],
        outer_ends(rows, nodes),
        [owner for owner, _low, _high in stairs],
    )
    data = dict(np.load(args.output / "after/body.npz"))
    pairs, whole = interactions(rows, nodes)
    whole.update(vertex_links(data["body_vertices"], data["body_faces"]))
    joins = contacts(data, rows)
    if (
        whole != report["whole_interactions"]
        or pairs != report["after"]["crossings"]
        or joins != report["whole_profile_contacts"]
    ):
        raise ValueError("saved whole interaction/profile evidence changed")
    layout = dict(np.load(args.output / "terrain-plan.npz"))
    if not np.array_equal(layout["targets"], sample(layout["xy"])):
        raise ValueError("saved cycle terrain has different actual DEM targets")
    ground, upper = selected_surfaces(data, rows)
    footprint = shapely.union_all(
        shapely.polygons(ground.vertices[ground.triangles, :2])
    )
    field = dict(np.load(args.output / "membrane-field.npz"))
    model = field_contract(layout, upper, footprint)
    roles, grades = check_heights(layout, field["heights"], model)
    global_roles = np.zeros(len(layout["faces"]), bool)
    global_roles[~layout["ground"]] = roles
    saved = dict(np.load(args.output / "membrane-body.npz"))
    terrain = Mesh(saved["vertices"], saved["faces"])
    if not np.array_equal(field["constructed"], global_roles) or not np.array_equal(
        terrain.vertices[saved["top_faces"]],
        np.c_[layout["xy"], field["heights"]][layout["faces"]],
    ):
        raise ValueError("saved terrain cap or construction roles changed")
    physical = inspect(terrain, saved, ground, upper)
    displayed(args.output, report["display"], terrain, global_roles, args.palette_scene)
    result = {
        "generation_commit": report["commit"],
        "reader_commit": commit,
        "products_report_sha256": digest(args.output / "comparison.json"),
        "road": road,
        "terrain": physical,
        "grades": grades,
        "earthwork": earthwork(layout, field["heights"]),
        "valid_local_candidate": True,
        "admitted_source_ids": [],
    }
    (args.output / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps({k: result[k] for k in ("reader_commit", "valid_local_candidate")})
    )


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    verify(arguments())

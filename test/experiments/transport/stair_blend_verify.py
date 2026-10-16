"""Replay saved whole contacts and actual class bounds without fitting or remeshing."""

import json
import resource

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_blend_audit import inspect
from stair_blend_constraints import upper_rows
from stair_blend_experiment import arguments
from stair_blend_metrics import earthwork
from stair_blend_solve import REACH_M, check_heights, pins_for
from stair_network_inputs import sample_xy
from stair_outer_inputs import FIXTURE, load
from stair_terrain_experiment import frozen
from stair_terrain_layout import selected_surfaces


def field_contract(layout, upper, footprint):
    faces = layout["faces"][~layout["ground"]]
    polygons = shapely.polygons(layout["xy"][faces])
    pins, values = pins_for(layout)
    bridge, bounds, _note = upper_rows(layout["xy"], faces, upper, footprint)
    return {
        "soil_faces": faces,
        "eligible": shapely.covered_by(polygons, footprint.buffer(REACH_M)),
        "pins": pins,
        "pin_values": values[pins],
        "areas": shapely.area(polygons),
        "structure_limit": 10,
        "bridge": bridge,
        "b": bounds,
        "datum": 0.0,
    }


def source_state(args, report):
    source = json.loads((args.network_products / "comparison.json").read_text())
    recipe = json.loads(FIXTURE.read_text())
    dem = FIXTURE.with_name(recipe["elevation"]["window_file"])
    if (
        report["fixture_sha256"] != digest(FIXTURE)
        or report["dem_window_sha256"] != digest(dem)
        or report["network_report_sha256"]
        != digest(args.network_products / "comparison.json")
        or report["source_body_sha256"]
        != digest(args.network_products / "after/body.npz")
        or report["source_catalog_sha256"]
        != digest(args.network_products / "source-catalog.json")
        or report["candidate_source_ids"] != source["candidate_source_ids"]
        or report["admitted_source_ids"]
    ):
        raise ValueError("saved terrain source/DEM/profile/non-admission changed")
    for name, expected in report["artifacts"].items():
        if digest(args.output / name) != expected:
            raise ValueError("saved blend artifact changed: " + name)
    data = dict(np.load(args.network_products / "after/body.npz"))
    _case, rows, _nodes, _stairs, context, _recipe = load()
    ground, upper = selected_surfaces(data, rows)
    footprint = shapely.union_all(
        shapely.polygons(ground.vertices[ground.triangles, :2])
    )
    return ground, upper, footprint, context, dict(np.load(dem))


def display_proof(args, report, body, roles):
    with np.load(args.network_products / "after/display-body.npz") as original:
        for name in ("before", "after"):
            target = args.output / (name + "-display.npz")
            if digest(target) != report["display"][name + "_display_sha256"]:
                raise ValueError("saved terrain display binding changed")
            with np.load(target) as displayed:
                for key in original:
                    if not np.array_equal(
                        displayed[key][: len(original[key])], original[key]
                    ):
                        raise ValueError("source road display prefix changed")
                if name != "after":
                    continue
                offset, first = len(original["vertices"]), len(original["triangles"])
                if not np.array_equal(
                    displayed["vertices"][offset:], body.vertices.astype(np.float32)
                ) or not np.array_equal(
                    displayed["triangles"][first:] - offset, body.triangles
                ):
                    raise ValueError(
                        "displayed terrain differs from the actual checked body"
                    )
                p = body.vertices.astype(np.float32).astype(float)[body.triangles]
                normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
                normals /= np.linalg.norm(normals, axis=1)[:, None]
                expected = np.repeat(normals, 3, axis=0).astype(
                    displayed["normals"].dtype
                )
                if not np.array_equal(displayed["normals"][3 * first :], expected):
                    raise ValueError("displayed actual terrain normals changed")
                if not np.all(
                    displayed["materials"][first : first + len(roles)][roles] == 6
                ):
                    raise ValueError(
                        "constructed terrain was displayed as ordinary soil"
                    )


def verify(args):
    commit = frozen()
    report = json.loads((args.output / "comparison.json").read_text())
    ground, upper, footprint, context, raster = source_state(args, report)
    layout = dict(np.load(args.output / "plan.npz"))
    if not np.array_equal(layout["targets"], sample_xy(layout["xy"], context, raster)):
        raise ValueError("actual retained DEM targets changed")
    model = field_contract(layout, upper, footprint)
    result = {}
    for name in ("data", "membrane"):
        if not (args.output / (name + "-field.npz")).exists():
            if report[name]["solve"]["accepted_field"] or report[name]["solve"].get(
                "candidate_exported"
            ):
                raise ValueError("expected candidate export is missing")
            result[name] = {"accepted_field": False, "candidate_available": False}
            continue
        field = dict(np.load(args.output / (name + "-field.npz")))
        try:
            roles, grades = check_heights(layout, field["heights"], model)
        except ValueError as error:
            if report[name]["solve"]["accepted_field"]:
                raise ValueError("formerly admitted field is now invalid") from error
            result[name] = {"accepted_field": False, "rejection": str(error)}
            continue
        if not report[name]["solve"]["accepted_field"]:
            raise ValueError(
                "previously rejected field now passes; review its recorded evidence"
            )
        data = dict(np.load(args.output / (name + "-body.npz")))
        body = Mesh(data["vertices"], data["faces"])
        expected = np.c_[layout["xy"], field["heights"]][layout["faces"]]
        if not np.array_equal(body.vertices[data["top_faces"]], expected):
            raise ValueError("actual terrain cap changed the stored affine field")
        global_roles = np.zeros(len(layout["faces"]), bool)
        global_roles[~layout["ground"]] = roles
        if not np.array_equal(global_roles, field["constructed"]):
            raise ValueError("stored constructed-face classification changed")
        result[name] = {
            "accepted_field": True,
            "grades": grades,
            "physical": inspect(body, data, ground, upper),
            "earthwork": earthwork(layout, field["heights"]),
        }
        if name == "membrane":
            display_proof(args, report, body, global_roles)
    if not result["membrane"]["accepted_field"]:
        raise ValueError("no verified membrane terrain product")
    result.update(
        generation_commit=report["commit"],
        reader_commit=commit,
        products_report_sha256=digest(args.output / "comparison.json"),
        valid_local_terrain=True,
        admitted_source_ids=[],
    )
    (args.output / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in ("reader_commit", "valid_local_terrain", "admitted_source_ids")
            }
        )
    )


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    verify(arguments())

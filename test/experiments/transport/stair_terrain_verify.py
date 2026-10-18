"""Read saved terrain counterexamples without remeshing, solving or excavation."""

import json
import resource

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_outer_inputs import FIXTURE, load
from stair_terrain_audit import clearance, inspect
from stair_terrain_experiment import arguments, frozen
from stair_terrain_layout import selected_surfaces


def read_mesh(path):
    with np.load(path) as arrays:
        return Mesh(arrays["vertices"], arrays["faces"])


def verify(args):
    commit = frozen()
    report = json.loads((args.output / "comparison.json").read_text())
    source = json.loads((args.network_products / "comparison.json").read_text())
    recipe = json.loads(FIXTURE.read_text())
    if (
        report["fixture_sha256"] != digest(FIXTURE)
        or report["dem_window_sha256"]
        != digest(FIXTURE.with_name(recipe["elevation"]["window_file"]))
        or report["network_report_sha256"]
        != digest(args.network_products / "comparison.json")
        or report["source_body_sha256"]
        != digest(args.network_products / "after/body.npz")
        or report["source_catalog_sha256"] != source["source_catalog_sha256"]
        or report["candidate_source_ids"] != source["candidate_source_ids"]
        or report["admitted_source_ids"]
        or report["approved_terrain"]
    ):
        raise ValueError("saved source/DEM binding or terrain non-admission changed")
    for name, expected in report["artifacts"].items():
        if digest(args.output / name) != expected:
            raise ValueError("saved terrain artifact changed: " + name)
    with np.load(args.network_products / "after/body.npz") as data:
        data = dict(data)
    _case, rows, _nodes, _stairs, _context, _recipe = load()
    ground, upper = selected_surfaces(data, rows)
    retaining = read_mesh(args.output / "retaining.npz")
    actual = read_mesh(args.output / "excavated-float32.npz")
    road = Mesh(data["body_vertices"], data["body_faces"])
    result = inspect(retaining, actual, ground, upper, road)
    with np.load(args.output / "retaining.npz") as fields:
        contact = Mesh(retaining.vertices, fields["top_faces"][fields["ground"]])
    a = shapely.union_all(shapely.polygons(contact.vertices[contact.triangles, :2]))
    b = shapely.union_all(shapely.polygons(ground.vertices[ground.triangles, :2]))
    if a.symmetric_difference(b).area > 1e-7:
        raise ValueError("retaining candidate lost complete Double contact footprint")
    result["double_bed_contact"] = [
        clearance(contact, ground),
        clearance(ground, contact),
    ]
    if any(abs(r["minimum_clearance_m"]) > 1e-7 for r in result["double_bed_contact"]):
        raise ValueError("retaining candidate changed actual Double underside contact")
    with np.load(args.network_products / "after/display-body.npz") as original:
        for name in ("before", "after"):
            path = args.output / (name + "-display.npz")
            if digest(path) != report["display"][name + "_display_sha256"]:
                raise ValueError("counterexample display binding changed")
            with np.load(path) as displayed:
                for key in original:
                    if not np.array_equal(
                        displayed[key][: len(original[key])], original[key]
                    ):
                        raise ValueError(
                            "terrain study changed the source road display"
                        )
    result.update(
        generation_commit=report["commit"],
        reader_commit=commit,
        saved_counterexamples_verified=True,
        approved_terrain=False,
    )
    (args.output / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in (
                    "reader_commit",
                    "saved_counterexamples_verified",
                    "approved_terrain",
                )
            }
        )
    )


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    verify(arguments())

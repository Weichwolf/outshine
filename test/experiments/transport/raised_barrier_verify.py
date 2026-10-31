"""Read saved whole side bodies and independently qualify their physical caps."""

import argparse
import json
import zipfile
from dataclasses import asdict
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from elevated_rail_hardware_verify import arrays
from experiment_output import create_output
from ground_area_bounds import body_caps
from outshine_ref.contracts import Mesh
from raised_barrier_study import bound_inputs, physical_guards
from raised_edge_barriers import DEFAULT_RECIPE, strip_caps
from stair_terrain_experiment import frozen
from surface_solid_checks import audit_closed


def payload(path):
    if path.stat().st_size > 128 * 1024:
        raise ValueError("Bounded side-body file required before hash/decode")
    with zipfile.ZipFile(path) as archive:
        if sum(x.file_size for x in archive.infolist()) > 128 * 1024:
            raise ValueError("Bounded side-body payload required before decode")
    return arrays(
        path, ("body_vertices", "body_triangles", "lower_vertices", "cap_triangles")
    )


def verify(directory, recipe=DEFAULT_RECIPE):
    path = directory / "report.json"
    if path.stat().st_size > 128 * 1024:
        raise ValueError("Bounded complete side-body report required")
    report = json.loads(path.read_text())
    if report["recipe"] != asdict(recipe):
        raise ValueError("Actual requested side-body recipe changed")
    hardware = Path(report["hardware"])
    if digest(hardware / "report.json") != report["hardware_report_sha256"]:
        raise ValueError("Whole original hardware report changed")
    parent, _bed, datum, clearance, cases, terrain = bound_inputs(
        hardware, Path(report["hardware_receipt"])
    )
    if (
        report["frame"] != parent["frame"]
        or report["object_origin_m"] != [0, 0, datum]
        or report["clearance_m"] != clearance
    ):
        raise ValueError("Actual source frame/datum/reserved volume changed")
    products = report["products"]
    if len(products) != 2 * len(cases):
        raise ValueError("Two complete side bodies per whole chain required")
    rows = []
    for index, row in enumerate(products):
        component, side = divmod(index, 2)
        case = cases[component]
        if (
            row["component"] != component
            or row["side"] != side
            or row["source_ids"] != case["keys"]
            or row["file"] != "side-" + str(index) + ".npz"
        ):
            raise ValueError("Exact whole-source/component/side binding required")
        source = directory / row["file"]
        data = payload(source)
        if digest(source) != row["sha256"]:
            raise ValueError("Saved whole side-body payload changed")
        expected, _top, lower = strip_caps(case["axis"], case["width"], recipe)[side]
        if not np.array_equal(
            data["lower_vertices"], lower.vertices
        ) or not np.array_equal(data["cap_triangles"], lower.triangles):
            raise ValueError("Actual complete source side profile changed")
        body = Mesh(data["body_vertices"], data["body_triangles"])
        upper = Mesh(
            np.c_[lower.vertices[:, :2], np.full(len(lower.vertices), recipe.height_m)],
            lower.triangles,
        )
        for cast in (np.float64, np.float32):
            b = Mesh(body.vertices.astype(cast).astype(float), body.triangles)
            u = Mesh(upper.vertices.astype(cast).astype(float), upper.triangles)
            l = Mesh(lower.vertices.astype(cast).astype(float), lower.triangles)
            body_caps(b, u, l)
            if audit_closed(b) <= 0:
                raise ValueError("Saved body is not positively closed and oriented")
        rows.append(
            {
                "body": body,
                "lower": lower,
                "upper": upper,
                "domain": expected,
                "component": component,
                "side": side,
                "source_ids": case["keys"],
            }
        )
    guards = physical_guards(rows, cases, terrain, recipe)
    return rows, {
        "whole_side_bodies_verified": True,
        "report_sha256": digest(path),
        "source_count": len({k for row in rows for k in row["source_ids"]}),
        "whole_physical_guards": guards,
        "scope": "Diagnostic saved-product qualification, including bound original input reconstruction; not the native cache-hit path or World/device authority.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    commit = frozen()
    create_output(args.output)
    _rows, note = verify(args.products)
    note["reader_commit"] = commit
    (args.output / "verification.json").write_text(json.dumps(note, indent=2) + "\n")
    print(json.dumps({"verified": True, "sources": note["source_count"]}))

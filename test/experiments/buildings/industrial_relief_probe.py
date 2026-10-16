"""Frozen actual-provider industrial relief inputs at gate and whole-source scales."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from demand import View
from evidence import revision
from industrial_relief_products import ArticulatedDetails
from prepared_hall_tile import RetainedDetails, prepare_halls
from prepared_hall_tile_probe import source_tile
from recipe_graph_data import plain
from stepped_source_probe import overview
from tile_buildings import construct


def camera(view):
    return {
        "projection": "perspective",
        "eye": np.asarray(view.eye).tolist(),
        "target": np.asarray(view.target).tolist(),
        "vertical_fov_deg": view.fov_deg,
        "clip_near_m": 0.1,
        "clip_far_m": 10000,
        "profile": [view.width, view.height],
    }


def export(output, name, owner, rows, receipt, view, proof):
    mesh, source = construct(
        "Grohnde", prepared=(rows, receipt), ordinary_builder=owner
    )
    path = output / (name + "-mesh.npz")
    np.savez_compressed(path, **mesh)
    record = proof | {
        "city": name,
        "ground_mode": "flat-no-dem",
        "camera": camera(view),
        "mesh": str(path.resolve()),
        "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "triangles": len(mesh["faces"]),
        "source": source,
        "facade_materials": owner.ordinary.facades.bindings,
        "procedures": owner.report(),
        "scope": "Actual original provider inputs and unchanged source height/outline/window/access programmes. Procedural industrial relief around existing openings; independent complete physical products. Geometry-only draw detail bound, no shadow/night/native/device admission.",
    }
    (output / (name + "-geometry.json")).write_text(
        json.dumps(record, default=plain, indent=2, allow_nan=False) + "\n"
    )
    return {
        "name": name,
        "triangles": len(mesh["faces"]),
        "generated": source["generated"],
    }


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze industrial relief before evidence")
    rows, receipt, _context, tile_view = source_tile()
    prepared = prepare_halls(rows)
    detail = ArticulatedDetails(prepared, tile_view)
    before = RetainedDetails(prepared, tile_view)
    output.mkdir(parents=True, exist_ok=True)
    reports = [
        export(
            output,
            "GrohndeReliefBefore",
            before,
            prepared.rows,
            receipt,
            tile_view,
            proof,
        )
    ]
    reports.append(
        export(
            output,
            "GrohndeReliefAfter",
            detail,
            prepared.rows,
            receipt,
            tile_view,
            proof,
        )
    )
    targets = {}
    for identifier, parts in detail.reliefs.items():
        if parts:
            kind = parts[0].plan.kind
            previous = targets.get(kind)
            if (
                previous is None
                or detail.source_rows[identifier]["polygon"].area
                > detail.source_rows[previous]["polygon"].area
            ):
                targets[kind] = identifier
    for index, (kind, identifier) in enumerate(sorted(targets.items())):
        plan = prepared.assets[identifier].plan
        gate = plan["gate"]
        t, centre = np.asarray(gate["tangent"]), np.asarray(gate["centre"])
        normal = np.array([t[1], -t[0]])
        view = View(
            np.r_[centre + normal * 18 + t * 8, 2.6],
            np.r_[centre + t * 2, 2.1],
            width=1920,
            height=1080,
            fov_deg=48,
        )
        for owner, suffix in ((before, "Before"), (detail, "After")):
            owner.view, owner.opened, owner.staff_angle = view, 1.0, 90.0
            name = f"HallRelief{index}{suffix}"
            reports.append(
                export(
                    output,
                    name,
                    owner,
                    (owner.source_rows[identifier],),
                    receipt,
                    view,
                    proof,
                )
                | {"kind": kind, "source_id": identifier}
            )
        if index == 0:
            opposite = overview(
                prepared.assets[identifier].body["vertices"], [-1, 1.2, 0.35]
            )
            for owner, suffix in ((before, "Before"), (detail, "After")):
                owner.view = opposite
                reports.append(
                    export(
                        output,
                        "HallReliefOpposite" + suffix,
                        owner,
                        (owner.source_rows[identifier],),
                        receipt,
                        opposite,
                        proof,
                    )
                )
    (output / "models.json").write_text(
        json.dumps(proof | {"models": reports}, indent=2) + "\n"
    )
    return reports


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(models(args.output)))

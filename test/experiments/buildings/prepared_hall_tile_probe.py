"""Offline whole-source tile inputs for retained hall comparisons."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from demand import View
from evidence import revision
from prepared_hall_tile import RetainedDetails, prepare_halls
from recipe_graph_data import plain
from tile_buildings import construct, read_tile
from tile_facades import contexts
from tile_industrial_halls import HallDetails
from tile_stepped import SteppedDetails

ROOT = Path(__file__).with_name("data") / "provider" / "grohnde"


def source_tile():
    rows, receipt = read_tile("Grohnde", source_root=ROOT)
    context = contexts(rows, receipt, source_root=ROOT)
    camera = json.loads((ROOT / "view.json").read_text())["camera"]
    view = View(
        camera["eye"],
        camera["target"],
        width=camera["profile"][0],
        height=camera["profile"][1],
        fov_deg=camera["vertical_fov_deg"],
    )
    return rows, receipt, context, view


def models(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze retained hall source before evidence")
    rows, receipt, context, view = source_tile()
    prepared = prepare_halls(rows)
    baseline = SteppedDetails(prepared.rows, view)
    baseline.facades.halls = HallDetails(baseline.facades.entrances, version="2")
    camera = json.loads((ROOT / "view.json").read_text())["camera"]
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for name, owner in (
        ("GrohndePreviousV2", baseline),
        (
            "GrohndeRetainedFull",
            RetainedDetails(prepared, view, reference_full_fields=True),
        ),
        ("GrohndeRetainedAdaptive", RetainedDetails(prepared, view)),
    ):
        mesh, source = construct(
            "Grohnde",
            prepared=(prepared.rows, receipt),
            ordinary_builder=owner,
        )
        facade = owner.facades if owner is baseline else owner.ordinary.facades
        path = output / (name + "-mesh.npz")
        np.savez_compressed(path, **mesh)
        record = proof | {
            "city": name,
            "camera": camera,
            "ground_mode": "flat-no-dem",
            "source": source,
            "context_report": context,
            "mesh": str(path.resolve()),
            "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "triangles": len(mesh["faces"]),
            "facade_materials": facade.bindings,
            "procedures": owner.report(),
            "scope": "All original source rows. Retained complete physical hall assets with independent adaptive draw parts; diagnostic whole-tile export still copies full source bodies. No complete body LOD, native cache, shadow/night or device admission.",
        }
        (output / (name + "-geometry.json")).write_text(
            json.dumps(record, default=plain, indent=2, allow_nan=False) + "\n",
        )
        results.append(
            {
                "name": name,
                "triangles": len(mesh["faces"]),
                "generated": source["generated"],
            }
        )
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(models(args.output)))

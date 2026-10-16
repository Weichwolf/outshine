"""Entire original source tile through the ordinary version-2 hall builder."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from demand import View
from evidence import revision
from tile_buildings import construct, read_tile
from tile_facades import contexts
from tile_industrial_halls import HallDetails
from tile_stepped import SteppedDetails


def build(reference, output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze normal v2 tile builder before evidence")
    old = json.loads(reference.read_text())
    camera = old["camera"]
    view = View(
        np.asarray(camera["eye"]),
        np.asarray(camera["target"]),
        width=1920,
        height=1080,
        fov_deg=camera["vertical_fov_deg"],
    )
    rows, receipt = read_tile("Grohnde")
    contexts(rows, receipt)
    owner = SteppedDetails(rows, view)
    owner.facades.halls = HallDetails(owner.facades.entrances, version="2")
    mesh, source = construct(
        "Grohnde", prepared=(rows, receipt), ordinary_builder=owner
    )
    for key in ("generated", "omissions", "counts", "material_definitions"):
        if source[key] != old["source"][key]:
            raise ValueError("Normal v2 tile changed source coverage/palette")
    members = json.loads(json.dumps(source["members"]))
    for first, last in zip(old["source"]["members"], members, strict=True):
        if {k: v for k, v in first.items() if k != "prototype"} != {
            k: v for k, v in last.items() if k != "prototype"
        }:
            raise ValueError(
                "Normal v2 tile changed source identities/attributes/geometry"
            )
    halls = owner.facades.halls.report()
    if len(halls["planned"]) != 8 or len(halls["completed"]) != 6:
        raise ValueError("Normal v2 tile lost existing logical/constructed halls")
    if len(mesh["faces"]) != old["triangles"]:
        raise ValueError("Normal direct-closed v2 changed admitted triangle count")
    output.mkdir(parents=True, exist_ok=True)
    path = output / "GrohndeHallV2-mesh.npz"
    np.savez_compressed(path, **mesh)
    record = (
        old
        | proof
        | {
            "city": "GrohndeHallV2",
            "mesh": str(path.resolve()),
            "mesh_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "triangles": len(mesh["faces"]),
            "source": source,
            "prior_reference": str(reference.resolve()),
            "procedures": owner.facades.report(),
            "facade_materials": owner.facades.bindings,
            "scope": "Entire original provider tile through normal explicit hall v2, no prototype patch. Deferred hall logical glazing retained; their compact roof, far selection, night and native integration remain unproved.",
            "normal_v2": {
                "planned": len(halls["planned"]),
                "completed": len(halls["completed"]),
                "planned_fields": sum(len(h["rooflights"]) for h in halls["planned"]),
                "rejected": halls["rejected"],
                "source_members": len(source["members"]),
                "source_attributes_ids_footprints_heights_counts_palette": "exact",
            },
        }
    )
    (output / "GrohndeHallV2-geometry.json").write_text(
        json.dumps(record, indent=2) + "\n"
    )
    print(json.dumps(record["normal_v2"]))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.reference, args.output)

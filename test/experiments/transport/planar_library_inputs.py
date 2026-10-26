"""Fixed real selected cases and original admitted rail archives."""

import json

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from tilted_rail_inputs import GROUND_PIN
from tilted_rail_verify import GroundContact
from upper_rail_contacts import UpperContact
from upper_rail_inputs import inputs as upper_inputs

FIXTURE = "6a17241e93231f34afafe7a610c497ca352bbef0f0e82389b98971cf619154d1"


def cases(args):
    if digest(args.inputs) != FIXTURE:
        raise ValueError("unchanged fixed actual selected planar rail inputs required")
    fixture = json.loads(args.inputs.read_text())
    if len(fixture["cases"]) != 133:
        raise ValueError("all actual admitted ground/deck cases required")
    for name, root in [("lower", args.lower), ("upper", args.upper)]:
        if digest(root / "comparison.json") != fixture[name + "_report_sha256"]:
            raise ValueError("actual original admitted rail report changed")
    return fixture


def contacts(args, fixture):
    if digest(args.network / "ground.npz") != GROUND_PIN:
        raise ValueError("original whole actual ground contact required")
    with np.load(args.network / "ground.npz") as d:
        ground = Mesh(d["vertices"], d["faces"])
        road = d["road"]
    deck, _pieces, _raw, _ledger, _work, binding = upper_inputs(args.reference)
    if binding != fixture["upper_binding"]:
        raise ValueError("actual selected upper deck changed")
    with np.load(args.lower / "after-body.npz") as d:
        before = Mesh(d["vertices"], d["triangles"])
    return {
        "ground": GroundContact(ground, road),
        "deck": UpperContact(deck, before),
    }


def original(args, row):
    root = args.lower if row["bed_kind"] == "ground" else args.upper
    path = root / "pieces" / f"piece-{row['product']}.npz"
    if digest(path) != row["original_product_sha256"]:
        raise ValueError("original actual piece changed")
    return path

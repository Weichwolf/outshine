"""Actual original border bodies and fixed surface/source frames for local ramp edits."""

import json

import numpy as np
import shapely
from camera_height_inputs import digest
from curb_inputs import protected_domain
from curb_verify import actual
from retained_surface import surface_domain
from sidewalk_inputs import inputs as old_inputs

WALK_REPORT = "2b575752914fcadde3e1dd0a5b127021edc47b501318c873ad318f0ce3274c23"
WALK_BODY = "5806b20c23d9d54b088ad57593a02e67ba36207b699e324a0f13120e346bb830"
LATEST = "3e272f12fe5ca9f9e6bd8026761e7320280c678e6838a0f4dde83cb372d15b31"


def inputs(args):
    ground, field, _base, curbs, _shapes, owners, traffic, world, _excluded, binding = (
        old_inputs(args)
    )
    for name, pin in [
        ("comparison.json", WALK_REPORT),
        ("sidewalks.npz", WALK_BODY),
        ("after-body.npz", LATEST),
    ]:
        if digest(args.sidewalks / name) != pin:
            raise ValueError("actual original sidewalk input changed: " + name)
    report = json.loads((args.sidewalks / "comparison.json").read_text())
    walks, _data = actual(args.sidewalks / "sidewalks.npz")
    rows = sorted(
        [r for r in report["candidates"] if r["valid"]], key=lambda r: r["product"]
    )
    if report["dirty"] or len(walks) != 204 or len(rows) != len(walks):
        raise ValueError("all original clean sidewalk bodies and owners required")
    products = curbs + walks
    old = [
        {"kind": "curb", "original": i, "sources": row["sources"]}
        for i, row in enumerate(owners)
    ]
    old.extend(
        {"kind": "sidewalk", "original": i, "sources": row["sources"]}
        for i, row in enumerate(rows)
    )
    shapes = [surface_domain(p.lower_contact) for p in products]
    obstacles = protected_domain(args.supports, args.reference_root)
    with np.load(args.baseline / "after-body.npz") as d:
        base = {k: d[k] for k in d.files}
    with np.load(args.sidewalks / "after-body.npz") as d:
        before = {k: d[k] for k in d.files}
    binding.update(
        original_sidewalk_report_sha256=WALK_REPORT,
        original_sidewalk_body_sha256=WALK_BODY,
        latest_scene_sha256=LATEST,
    )
    return {
        "ground": ground,
        "field": field,
        "traffic": traffic,
        "world": world,
        "obstacles": obstacles,
        "products": products,
        "owners": old,
        "shapes": shapes,
        "raised": shapely.union_all(shapes),
        "tree": shapely.STRtree(shapes),
        "base": base,
        "before": before,
        "binding": binding,
    }

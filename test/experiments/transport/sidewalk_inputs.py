"""Pinned actual curb products; surface/source contracts remain caller selected."""

import json

import numpy as np
import shapely
from camera_height_inputs import digest
from curb_inputs import inputs as curb_inputs
from curb_verify import actual
from retained_surface import surface_domain

CURB_REPORT = "b7b26c17052273a6ace843880d4654261b46c1dacd237527b37869b10100f810"
CURB_BODY = "4db10d8ede0e9ee0dcd7df19212841c94fd570de5ef3fea8b91f80fd32788431"
BASE_BODY = "3cbb4f6ac7e0f555e09204d1e9c76fbff35ba8497519e3c59e012e215c8a21c9"


def inputs(args):
    ground, field, _before, _candidates, traffic, world, excluded, binding, _ = (
        curb_inputs(args.network, args.baseline, args.supports, args.reference_root)
    )
    for name, pin in [
        ("comparison.json", CURB_REPORT),
        ("curbs.npz", CURB_BODY),
        ("after-body.npz", BASE_BODY),
    ]:
        if digest(args.curbs / name) != pin:
            raise ValueError("actual accepted curb input changed: " + name)
    report = json.loads((args.curbs / "comparison.json").read_text())
    if report["dirty"]:
        raise ValueError("clean original curb result required")
    products, _ = actual(args.curbs / "curbs.npz")
    rows = sorted(
        [r for r in report["candidates"] if r["valid"]], key=lambda r: r["product"]
    )
    if len(products) != len(rows) or len(products) != 256:
        raise ValueError("actual curb ownership count changed")
    shapes = [surface_domain(p.lower_contact) for p in products]
    for index, (shape, row) in enumerate(zip(shapes, rows, strict=True)):
        if (
            row["product"] != index
            or shape.symmetric_difference(
                shapely.from_wkb(bytes.fromhex(row["footprint_wkb_hex"]))
            ).area
            > 1e-8
        ):
            raise ValueError("actual curb footprint/source ownership changed")
    with np.load(args.curbs / "after-body.npz") as d:
        before = {k: d[k] for k in d.files}
    binding.update(
        curb_report_sha256=CURB_REPORT,
        actual_curbs_sha256=CURB_BODY,
        before_body_sha256=BASE_BODY,
    )
    return (
        ground,
        field,
        before,
        products,
        shapes,
        rows,
        traffic,
        world,
        excluded,
        binding,
    )

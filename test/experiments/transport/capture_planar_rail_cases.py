"""Explicitly capture already admitted real inputs; normal tests never update these."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from tilted_rail_inputs import FIELD_PIN
from upper_rail_inputs import inputs as upper_inputs


def capture(args):
    if args.output.exists() or digest(args.network / "field.npz") != FIELD_PIN:
        raise ValueError(
            "fresh fixed-case output and original pinned height field required"
        )
    with np.load(args.network / "field.npz") as d:
        lower_vertices, lower_faces = d["vertices"], d["faces"]
    deck, _pieces, _raw, _ledger, _work, upper_binding = upper_inputs(args.reference)
    cases, reports = [], {}
    for name, root, vertices, faces, kind in [
        ("lower", args.lower, lower_vertices, lower_faces, "ground"),
        ("upper", args.upper, deck.vertices, deck.triangles, "deck"),
    ]:
        report = json.loads((root / "comparison.json").read_text())
        if report["dirty"]:
            raise ValueError("clean old admitted rail products required")
        reports[name] = digest(root / "comparison.json")
        for row in report["pieces"]:
            if not row["valid"]:
                continue
            path = root / "pieces" / f"piece-{row['product']}.npz"
            if digest(path) != row["mesh_sha256"]:
                raise ValueError("actual original admitted rail export changed")
            with np.load(path) as d:
                counts = {
                    "vertices": len(d["vertices"]),
                    "faces": len(d["triangles"]),
                    "sleepers": len(d["sleeper_transforms"]),
                }
            cases.append(
                {
                    "id": f"{name}-{row['product']}",
                    "product": row["product"],
                    "sources": row["sources"],
                    "bed_kind": kind,
                    "plane_face": row["plane_face"],
                    "triangle": vertices[faces[row["plane_face"]]].tolist(),
                    "xy": row["xy"],
                    "gauge_m": row["gauge_m"],
                    "original_product_sha256": row["mesh_sha256"],
                    "original_counts": counts,
                }
            )
    if len(cases) != 133:
        raise ValueError("all 77 lower and 56 upper admitted pieces required")
    result = {
        "schema": "outshine-planar-rail-cases-1",
        "frame": "local east/north/up metres; source origin lat/lon48.21/16.37",
        "lower_report_sha256": reports["lower"],
        "upper_report_sha256": reports["upper"],
        "height_field_sha256": FIELD_PIN,
        "upper_binding": upper_binding,
        "scope": "Only previously admitted explicit affine ground/deck pieces; full sources and omitted candidates remain in original reports. No new source use/contact/admission or finite bed selection.",
        "cases": cases,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"cases": len(cases), "sha256": digest(args.output)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["network", "reference", "lower", "upper", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    capture(parser.parse_args())

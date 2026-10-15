"""Independently decode saved upper hardware, verify metric whole contacts/base prefix."""

import argparse
import json
import resource
import subprocess
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from rail_bed_patches import contact_patches, cut_to_patches
from rail_place_experiment import packed_mesh
from retained_surface import surface_domain
from tilted_rail_replay import load_product, verify_display
from tilted_rail_verify import admit, metric_audit
from upper_rail_contacts import UpperContact
from upper_rail_experiment import read_base
from upper_rail_inputs import inputs


def replay(args):
    report = json.loads((args.products / "comparison.json").read_text())
    if report["dirty"] or report["baseline_report_sha256"] != digest(
        args.baseline / "comparison.json"
    ):
        raise ValueError("clean pinned baseline required")
    before = read_base(args.baseline)
    deck, candidates, _raw, chains, _work, binding = inputs(args.reference)
    patches, _note = contact_patches(deck, method=report["method"])
    pieces, cuts, _work = cut_to_patches(candidates, patches)
    if (
        report["bindings"] != binding
        or report["chains"] != chains
        or report["affine_cuts"] != cuts
        or len(report["pieces"]) != len(pieces)
        or report["patches"]
        != [{"plane_face": p["plane_face"], "faces": p["faces"]} for p in patches]
    ):
        raise ValueError("source/deck selection ledger changed")
    contact = UpperContact(deck, before[0])
    bed = surface_domain(deck)
    products, rows, proofs = [], [], []
    for candidate, row in zip(pieces, report["pieces"], strict=True):
        if (
            row["sources"] != candidate["sources"]
            or row["chain"] != candidate["chain"]
            or row["plane_face"] != candidate["plane_face"]
            or not np.array_equal(row["xy"], candidate["xy"])
        ):
            raise ValueError("source/affine piece ownership changed")
        if not row["valid"]:
            if not row["reason"]:
                raise ValueError("explicit optional omission reason required")
            continue
        path = args.products / "pieces" / f"piece-{row['product']}.npz"
        if row["product"] != len(products) or digest(path) != row["mesh_sha256"]:
            raise ValueError("saved actual piece changed")
        triangle = deck.vertices[deck.triangles[row["plane_face"]]]
        product = load_product(path, triangle)
        shape = shapely.union_all(product.footprints)
        if shape.difference(bed).area > 1e-7:
            raise ValueError("saved hardware outside actual finite upper deck")
        proof = contact.verify(shape, triangle)
        proof.update(metric_audit(product, row["gauge_m"]))
        rings = product.rails.vertices.reshape(2, -1, 12, 3)
        axis = (rings[:, :, 0] + rings[:, :, 1]).mean(axis=0) / 2 - product.plane_frame[
            :, 2
        ] * 0.18
        if (
            shapely.LineString(axis[:, :2]).hausdorff_distance(
                shapely.LineString(row["xy"])
            )
            > 1e-7
        ):
            raise ValueError("actual rails leave chosen original axis piece")
        products.append(product)
        rows.append(dict(row))
        proofs.append(proof)
    kept, mutual = admit(products, rows)
    if len(kept) != len(products):
        raise ValueError("saved hardware pieces overlap")
    packed = packed_mesh(products)
    if digest(args.products / "hardware.npz") != report["hardware_sha256"]:
        raise ValueError("saved packed hardware changed")
    with np.load(args.products / "hardware.npz") as d:
        if set(d.files) != set(packed) or any(
            not np.array_equal(d[k], packed[k]) for k in packed
        ):
            raise ValueError("packed/rendered hardware differs from saved pieces")
    return {
        "verifier_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.products / "comparison.json"),
        "pieces": len(products),
        "sleepers": len(packed["sleeper_transforms"]),
        "maximum_errors": {k: max(p[k] for p in proofs) for k in proofs[0]},
        "mutual": mutual,
        "display": verify_display(args, report, packed),
        "preserved_base": True,
        "base_triangle_tests": contact.base_tests,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["products", "reference", "baseline", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh independent proof required")
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    result = replay(args)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)

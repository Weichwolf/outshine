"""Independent replay of actual saved sidewalk bodies and their whole curb joints."""

import json
import resource
import subprocess

import shapely
from bridge_support_contacts import ContactSurface
from camera_height_inputs import digest
from curb_verify import actual, verify_display
from sidewalk_experiment import arguments
from sidewalk_geometry import audit
from sidewalk_inputs import inputs
from sidewalk_plan import select


def verify(args):
    path = args.output / "verification.json"
    if path.exists():
        raise ValueError("fresh independent sidewalk receipt required")
    report = json.loads((args.output / "comparison.json").read_text())
    if (
        report["dirty"]
        or digest(args.output / "sidewalks.npz") != report["sidewalks_sha256"]
    ):
        raise ValueError("actual clean sidewalk export required")
    ground, _field, before, curbs, shapes, owners, traffic, world, excluded, binding = (
        inputs(args)
    )
    if binding != report["bindings"]:
        raise ValueError("sidewalk source/height/curb bindings differ")
    expected, blocked, outside, _ = select(shapes, owners, traffic, world, excluded)
    if len(expected) != len(report["candidates"]):
        raise ValueError("sidewalk omitted source ledger incomplete")
    products, data = actual(args.output / "sidewalks.npz")
    context = ContactSurface(ground), traffic, world, blocked, outside
    proofs = []
    domains = []
    for row, choice in zip(report["candidates"], expected, strict=True):
        for key in ["candidate", "curb_products", "sources", "joint_wkb_hex"]:
            if row[key] != choice[key]:
                raise ValueError("sidewalk actual source or joint choice differs")
        footprint = shapely.from_wkb(bytes.fromhex(row["footprint_wkb_hex"]))
        if footprint.symmetric_difference(choice["shape"]).area > 1e-8:
            raise ValueError("sidewalk actual candidate footprint differs")
        if not row["valid"]:
            if not row["reason"] or "product" in row:
                raise ValueError("missing sidewalk omission reason")
            continue
        if row["product"] != len(proofs) or choice["reason"]:
            raise ValueError("sidewalk actual body ownership differs")
        proofs.append(audit(products[row["product"]], choice, curbs, shapes, context))
        domains.append(footprint)
    if len(proofs) != len(products) or len(proofs) != report["generation"]["accepted"]:
        raise ValueError("sidewalk export body ownership incomplete")
    tree = shapely.STRtree(domains)
    pairs = tree.query(domains, predicate="intersects")
    for left, right in pairs.T:
        if left < right and domains[left].intersection(domains[right]).area > 1e-8:
            raise ValueError("actual sidewalks overlap each other")
    receipt = {
        "verifier_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.output / "comparison.json"),
        "bodies": len(products),
        "area_m2": sum(p["area_m2"] for p in proofs),
        "joint_length_m": sum(p["joint_length_m"] for p in proofs),
        "maximum_ground_contact_error_m": max(
            p["whole_ground_contact_error_m"] for p in proofs
        ),
        "maximum_joint_height_error_m": max(
            p["maximum_joint_height_error_m"] for p in proofs
        ),
        "affine_joint_endpoint_queries": sum(
            p["affine_joint_endpoint_queries"] for p in proofs
        ),
        "maximum_height_error_m": max(p["height_error_m"] for p in proofs),
        "maximum_volume_error_m3": max(p["volume_error_m3"] for p in proofs),
        "mutual_pair_candidates": pairs.shape[1],
        "display": verify_display(args, report, before, data),
    }
    path.write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt), flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (240, 240))
    verify(arguments())

"""Independent saved ramp/border replay with original sources and whole old-state checks."""

import json
import resource
import subprocess

import numpy as np
import shapely
from camera_height_inputs import digest
from curb_verify import actual, verify_display
from foot_ramp_audit import audit
from foot_ramp_cut import audit as audit_border
from foot_ramp_experiment import arguments, context
from foot_ramp_geometry import Ramp
from foot_ramp_inputs import inputs
from foot_ramp_joints import joint_error
from foot_ramp_plan import plan
from foot_ramp_sources import source_edges
from retained_surface import surface_domain
from wien_support_experiment import packed


def borders(args, data, report, rows, ground, upper):
    products, _pack = actual(args.output / "borders.npz")
    if (
        len(products) != len(report["border_owners"])
        or report["border_sources"] != data["owners"]
    ):
        raise ValueError("complete original border/source ownership required")
    groups = {i: [] for i in range(len(data["products"]))}
    for p, owner in zip(products, report["border_owners"], strict=True):
        index = owner["owner"]
        if index not in groups or owner["component"] != len(groups[index]):
            raise ValueError("border component ownership differs")
        groups[index].append(p)
    cuts = shapely.union_all([r["shape"] for r in rows])
    interfaces = []
    for index, old in enumerate(data["products"]):
        before = data["shapes"][index]
        expected = before.difference(cuts)
        members = groups[index]
        after = shapely.union_all([surface_domain(p.lower_contact) for p in members])
        if expected.symmetric_difference(after).area > 1e-8:
            raise ValueError(
                "saved border subtraction differs from the actual accepted ramps"
            )
        if before.intersection(cuts).area <= 1e-12:
            if (
                len(members) != 1
                or not np.array_equal(members[0].body.vertices, old.body.vertices)
                or not np.array_equal(members[0].body.triangles, old.body.triangles)
            ):
                raise ValueError("unaffected original body changed")
        else:
            for p in members:
                audit_border(p, surface_domain(p.lower_contact), ground, upper)
        interfaces.append((expected, members))
    return products, interfaces


def verify(args):
    receipt = args.output / "verification.json"
    if receipt.exists():
        raise ValueError("fresh independent ramp receipt required")
    report = json.loads((args.output / "comparison.json").read_text())
    if (
        report["dirty"]
        or digest(args.output / "borders.npz") != report["borders_sha256"]
        or digest(args.output / "ramps.npz") != report["ramps_sha256"]
    ):
        raise ValueError("actual clean ramp/border exports required")
    data = inputs(args)
    if data["binding"] != report["bindings"]:
        raise ValueError("actual ramp source/ground/original-body bindings differ")
    feet, roads, ledger, work = source_edges(args.network)
    choices, omissions, planning = plan(
        feet,
        roads,
        data["raised"],
        data["traffic"],
        data["world"],
        data["obstacles"],
        data["field"],
    )
    if (
        ledger != report["source_ledger"]
        or omissions != report["plan_ledger"]
        or planning != report["plan_work"]
        or work != report["source_work"]
    ):
        raise ValueError("actual source/access/omission planning changed")
    if len(choices) != len(report["candidates"]):
        raise ValueError("ramp candidate ledger incomplete")
    accepted = []
    for choice, row in zip(choices, report["candidates"], strict=True):
        for key, value in choice.items():
            if key != "shape" and row[key] != value:
                raise ValueError("actual selected ramp source/frame/profile differs")
        shape = shapely.from_wkb(bytes.fromhex(row["footprint_wkb_hex"]))
        if shape.symmetric_difference(choice["shape"]).area > 1e-8:
            raise ValueError("actual ramp footprint differs")
        if row["valid"]:
            if row["product"] != len(accepted):
                raise ValueError("ramp body ownership differs")
            accepted.append(choice)
        elif not row["reason"] or "product" in row:
            raise ValueError("ramp local omission is not explicit")
    _source, ground, upper, _limits = context(data)
    products, _pack = actual(args.output / "ramps.npz")
    if len(products) != len(accepted) or len(products) != report["accepted"]:
        raise ValueError("all saved actual ramps must have one selected source")
    profiles, interfaces = borders(args, data, report, accepted, ground, upper)
    proofs, seam_errors = [], []
    for product, row in zip(products, accepted, strict=True):
        ramp = Ramp(
            product.body, product.lower_contact, product.upper_contact, row["shape"]
        )
        proofs.append(
            audit(ramp, row, ground, data["traffic"], data["world"], data["obstacles"])
        )
        seam_length = 0.0
        for remaining, members in interfaces:
            seam = remaining.boundary.intersection(row["shape"].boundary)
            if seam.length <= 1e-7:
                continue
            covered = []
            for member in members:
                line = seam.intersection(
                    surface_domain(member.upper_contact).boundary.buffer(1e-8)
                )
                if line.length > 1e-7:
                    error, _queries = joint_error(
                        ramp.upper, member.upper_contact, line
                    )
                    seam_errors.append(error)
                    seam_length += line.length
                    covered.append(line)
            if seam.difference(shapely.union_all(covered).buffer(1e-8)).length > 1e-10:
                raise ValueError("saved ramp lacks a whole actual border seam")
        if seam_length < 3.6:
            raise ValueError("saved ramp lacks both finite side interfaces")
    all_products = profiles + products
    domains = [surface_domain(p.upper_contact) for p in all_products]
    tree = shapely.STRtree(domains)
    pairs = tree.query(domains, predicate="intersects")
    for left, right in pairs.T:
        if left < right and domains[left].intersection(domains[right]).area > 1e-8:
            raise ValueError("actual retained borders or ramps overlap")
    all_pack = packed(
        [p.body for p in all_products],
        [(p.lower_contact, p.upper_contact) for p in all_products],
    )
    display = verify_display(args, report, data["base"], all_pack)
    result = {
        "verifier_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.output / "comparison.json"),
        "ramps": len(products),
        "original_border_owners": len(data["products"]),
        "retained_border_bodies": len(profiles),
        "maximum_ground_contact_error_m": max(
            p["whole_ground_contact_error_m"] for p in proofs
        ),
        "maximum_field_error_m": max(p["whole_field_error_m"] for p in proofs),
        "maximum_mouth_step_m": max(p["maximum_mouth_step_m"] for p in proofs),
        "maximum_border_joint_error_m": max(seam_errors),
        "mutual_pair_candidates": pairs.shape[1],
        "display": display,
    }
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (240, 240))
    verify(arguments())

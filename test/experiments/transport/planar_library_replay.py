"""Read actual new rail exports independently of the construction factory."""

import argparse
import json
import resource
import subprocess
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from planar_library_build import LATEST_BODY
from planar_library_inputs import cases, contacts
from rail_place_experiment import packed_mesh
from tilted_rail_replay import load_product
from tilted_rail_verify import admit, metric_audit


def replay(args):
    receipt = args.output / "verification.json"
    if receipt.exists():
        raise ValueError("fresh independent planar rail receipt required")
    fixture = cases(args)
    report = json.loads((args.output / "comparison.json").read_text())
    if report["dirty"] or report["fixture_sha256"] != digest(args.inputs):
        raise ValueError("clean bound fixed real input construction required")
    if len(report["pieces"]) != len(fixture["cases"]):
        raise ValueError("actual source/body ledger incomplete")
    reference = contacts(args, fixture)
    groups = {"ground": [], "deck": []}
    rows = {"ground": [], "deck": []}
    proofs = []
    for source, record in zip(fixture["cases"], report["pieces"], strict=True):
        if source["id"] != record["id"] or source["sources"] != record["sources"]:
            raise ValueError("actual case/source identity changed")
        path = args.output / "pieces" / (source["id"] + ".npz")
        if digest(path) != record["sha256"]:
            raise ValueError("actual newly saved rail piece changed")
        triangle = np.asarray(source["triangle"], float)
        product = load_product(path, triangle)
        shape = shapely.union_all(product.footprints)
        proof = reference[source["bed_kind"]].verify(shape, triangle)
        proof.update(metric_audit(product, source["gauge_m"]))
        proofs.append(proof)
        groups[source["bed_kind"]].append(product)
        rows[source["bed_kind"]].append({"valid": True})
    for kind, products in groups.items():
        chosen, _note = admit(products, rows[kind])
        if len(chosen) != len(products):
            raise ValueError("new actual pieces fail original mutual conflict check")
        packed = packed_mesh(products)
        for path in [
            args.output / (kind + "-hardware.npz"),
            (args.lower if kind == "ground" else args.upper) / "hardware.npz",
        ]:
            with np.load(path) as actual:
                if set(actual.files) != set(packed) or any(
                    not np.array_equal(actual[k], packed[k]) for k in actual.files
                ):
                    raise ValueError(
                        "actual reconstructed rails/sleepers differ from original or new pack"
                    )
    if digest(args.latest / "after-body.npz") != LATEST_BODY:
        raise ValueError("unchanged full latest scene required")
    with (
        np.load(args.latest / "after-body.npz") as old,
        np.load(args.output / "after-body.npz") as actual,
    ):
        if set(old.files) != set(actual.files) or any(
            not np.array_equal(actual[k], old[k]) for k in old.files
        ):
            raise ValueError(
                "actual library scene differs from complete old presentation"
            )
    result = {
        "verifier_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.output / "comparison.json"),
        "bodies": {k: len(v) for k, v in groups.items()},
        "maximum_gauge_error_m": max(p["gauge_error_m"] for p in proofs),
        "maximum_dimension_error_m": max(p["dimension_error_m"] for p in proofs),
        "maximum_section_width_error_m": max(
            p["section_width_error_m"] for p in proofs
        ),
        "maximum_contact_error_m": max(p["contact_error_m"] for p in proofs),
        "maximum_rail_volume_error_m3": max(p["rail_volume_error_m3"] for p in proofs),
        "maximum_sleeper_volume_error_m3": max(
            p["sleeper_volume_error_m3"] for p in proofs
        ),
        "original_and_new_packed_arrays_exact": True,
        "whole_scene_arrays_exact": True,
    }
    receipt.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in [
        "inputs",
        "lower",
        "upper",
        "network",
        "reference",
        "latest",
        "output",
    ]:
        parser.add_argument("--" + name, type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (240, 240))
    replay(parser.parse_args())

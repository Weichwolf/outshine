"""Replay actual stored family areas, global coverage and unchanged rendered body."""

import argparse
import json
import resource
import subprocess
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from transport_surface_experiment import PALETTE
from transport_surface_families import FAMILIES, MATERIALS, family_domains
from transport_surface_inputs import inputs


def verify_areas(args, report, polygons, selected, domains):
    expected = shapely.area(polygons)
    datasets = {}
    reasons = None
    for method in ["cells", "whole", "centroid"]:
        path = args.products / ("areas-" + method + ".npz")
        if digest(path) != report["methods"][method]["sha256"]:
            raise ValueError("saved actual family areas changed")
        with np.load(path) as d:
            a = d["areas_xy_m2"]
            if not np.array_equal(d["body_face_ids"], selected):
                raise ValueError("family field bound to wrong body faces")
            if method == "cells":
                reasons = d["reasons"]
        if (
            a.shape != (len(polygons), len(FAMILIES))
            or not np.isfinite(a).all()
            or np.any(a < 0)
            or np.max(abs(a.sum(axis=1) - expected)) > 1e-7
        ):
            raise ValueError("whole positive family area/unknown partition failed")
        datasets[method] = a
    maximum = float(np.max(abs(datasets["cells"] - datasets["whole"])))
    if maximum > 1e-7 or reasons.shape != (len(polygons),):
        raise ValueError("saved equivalent family fields differ")
    traffic = shapely.coverage_union_all(polygons)
    unresolved = float(expected[reasons != 0].sum())
    global_errors = []
    for i, domain in enumerate(domains):
        if i:
            exact = traffic.intersection(domain).area
            global_errors.append(abs(exact - datasets["cells"][:, i].sum()))
    if max(global_errors) > unresolved + 1e-6:
        raise ValueError(
            "actual family areas fail independent global coverage intersection"
        )
    return datasets["cells"], {
        "maximum_method_area_error_m2": maximum,
        "maximum_global_family_area_error_m2": max(global_errors),
        "explicit_unresolved_xy_m2": unresolved,
        "whole_traffic_area_xy_m2": float(expected.sum()),
        "centroid_reclassified_xy_m2": float(
            abs(datasets["centroid"] - datasets["cells"]).sum() / 2
        ),
    }


def verify_body(args, report, arrays, selected, areas):
    path = Path(report["render_inputs"][1]["mesh"])
    if digest(path) != report["render_inputs"][1]["mesh_sha256"]:
        raise ValueError("actual rendered colour body changed")
    if digest(PALETTE) != report["material"]["sha256"]:
        raise ValueError("material owner input differs from frozen linear values")
    catalogue = {
        r["name"]: r["albedo"] for r in json.loads(PALETTE.read_text())["classes"]
    }
    scene = report["render_inputs"][1]
    rgb = np.array(
        [
            scene["colours"][0][:3] if name is None else catalogue[name]
            for name in MATERIALS
        ]
    )
    expected = np.array(scene["colours"], np.float32)[arrays["materials"]]
    expected[selected, :3] = np.einsum(
        "ij,jk->ik", areas / areas.sum(axis=1)[:, None], rgb
    ).astype(np.float32)
    with np.load(path) as d:
        if sum(d[k].nbytes for k in d.files) != scene["source_array_payload_bytes"]:
            raise ValueError(
                "scene payload bytes do not describe the actual exported arrays"
            )
        if set(d.files) != set(arrays) | {"face_base_colour_linear_rgba"}:
            raise ValueError("unexpected body arrays or missing colour field")
        for key in arrays:
            if not np.array_equal(d[key], arrays[key]):
                raise ValueError(
                    "positions, topology, normals or previous material slots changed"
                )
        actual = d["face_base_colour_linear_rgba"]
    error = float(np.max(abs(actual - expected)))
    if error > np.finfo(np.float32).eps or not np.array_equal(
        actual[:, 3], expected[:, 3]
    ):
        raise ValueError(
            "actual shader colour field differs from whole-area linear means"
        )
    return {
        "old_arrays_exact": True,
        "maximum_linear_colour_error": error,
        "face_colour_bytes": actual.nbytes,
        "faces": len(actual),
    }


def replay(args):
    report = json.loads((args.products / "comparison.json").read_text())
    ground, road, selected, records, arrays, binding = inputs(
        args.network, args.baseline
    )
    if report["dirty"] or report["bindings"] != binding:
        raise ValueError("clean pinned source and lower field required")
    for name in ["sources", "families"]:
        if digest(args.products / (name + ".json")) != report[name + "_sha256"]:
            raise ValueError("saved source/family ownership changed")
    metadata = [{k: v for k, v in r.items() if k != "shape"} for r in records]
    if metadata != json.loads((args.products / "sources.json").read_text()):
        raise ValueError("actual source/used-owner/level binding changed")
    saved = json.loads((args.products / "families.json").read_text())
    domains = [shapely.from_wkb(bytes.fromhex(s)) for s in saved["domains_wkb_hex"]]
    rebuilt, _ = family_domains(records)
    if saved["names"] != list(FAMILIES) or any(
        a.symmetric_difference(b).area > 1e-7
        for a, b in zip(domains, rebuilt, strict=True)
    ):
        raise ValueError("source-bound polygon families changed")
    for i, domain in enumerate(domains):
        if not domain.is_valid or any(
            domain.intersection(b).area > 1e-7 for b in domains[:i]
        ):
            raise ValueError("actual material family domains overlap or are invalid")
    polygons = shapely.polygons(ground.vertices[ground.triangles[road], :2])
    areas, proof = verify_areas(args, report, polygons, selected, domains)
    return {
        "verifier_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.products / "comparison.json"),
        "areas": proof,
        "body": verify_body(args, report, arrays, selected, areas),
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["products", "network", "baseline", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh independent output required")
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    result = replay(args)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)

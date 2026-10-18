"""Audit original native roof domains without generating a full mesh first."""

import argparse
import hashlib
import json
import tempfile
from collections import Counter
from pathlib import Path

import numpy as np
import shapely
from building_roof_domains import checked_union, footprint, planar_roof_domains


def controls(shape, pieces):
    counts, maximum_error, maximum_distance = Counter(), 0.0, 0.0
    for piece in pieces:
        gradient = piece["plane_enu"][:2]
        expected = np.r_[-gradient, 1.0]
        expected /= np.linalg.norm(expected)
        if not np.allclose(expected, piece["normal_enu"], rtol=0, atol=1e-12):
            raise ValueError("native normal disagrees with reconstructed roof plane")
        counts["plane_normals"] += 1
    for control in shape["height_controls"]:
        if not control["inside"]:
            counts["native_outside_controls"] += 1
            continue
        point = shapely.Point(control["point"])
        covering = [piece for piece in pieces if piece["domain"].covers(point)]
        if not covering:
            counts["inside_controls_without_exact_domain_membership"] += 1
            maximum_distance = max(
                maximum_distance,
                min(piece["domain"].distance(point) for piece in pieces),
            )
            continue
        for piece in covering:
            plane = piece["plane_enu"]
            products = np.asarray(control["point"]) * plane[:2]
            actual = float(products.sum() + plane[2])
            error = abs(actual - control["wall_top_m"])
            allowance = (
                32
                * np.finfo(float).eps
                * (np.abs(products).sum() + abs(plane[2]) + abs(control["wall_top_m"]))
            )
            if error > max(1e-12, allowance):
                raise ValueError("native height disagrees with covering roof plane")
            maximum_error = max(maximum_error, error)
            counts["covered_plane_height_comparisons"] += 1
    return counts, maximum_error, maximum_distance


def inspect_shape(shape):
    pieces = planar_roof_domains(shape)
    if not pieces:
        return {"state": "no_planar_domains", "roof_kind": shape["roof_kind"]}
    counts, height_error, distance = controls(shape, pieces)
    record = {
        "state": "planar_domains",
        "roof_kind": shape["roof_kind"],
        "domains": len(pieces),
        "counts": dict(counts),
        "maximum_height_error_m": height_error,
        "maximum_uncovered_control_distance_m": distance,
        "sum_domain_area_m2": sum(piece["domain"].area for piece in pieces),
    }
    try:
        union = checked_union(pieces)
    except ValueError as error:
        record["union_error"] = str(error)
        record["symmetric_difference_area_m2"] = None
    else:
        record["symmetric_difference_area_m2"] = (
            footprint(shape).symmetric_difference(union).area
        )
    return record


def audit(path):
    raw = path.read_bytes()
    native = json.loads(raw)
    rows, counts = [], Counter()
    for source in native["sources"]:
        counts["sources"] += 1
        surface = source["surface"]
        if surface is None:
            counts["unknown_source_surfaces"] += 1
            continue
        for index, shape in enumerate(surface["shapes"]):
            identity = {"source_index": source["index"], "shape": index}
            identity.update({key: source[key] for key in ("id", "kind", "cell")})
            polygon = shapely.Polygon(shape["ring"], shape["holes"])
            if polygon.is_empty or not polygon.is_valid or polygon.area <= 0:
                record = {
                    "state": "invalid_original_footprint",
                    "reason": shapely.is_valid_reason(polygon),
                }
            else:
                record = inspect_shape(shape)
            counts[record["state"]] += 1
            counts.update(record.get("counts", {}))
            counts["domains"] += record.get("domains", 0)
            counts["unreliable_union_statistics"] += "union_error" in record
            rows.append(identity | record)
    return {
        "input": str(path.resolve()),
        "input_sha256": hashlib.sha256(raw).hexdigest(),
        "parent": native["parent"],
        "counts": dict(counts),
        "shapes": rows,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(Path(__file__).resolve().parents[3])
        or target.exists()
    ):
        parser.error("fresh explicit system-temp output outside checkout required")
    reports = [audit(path) for path in args.input]
    counts = Counter()
    for report in reports:
        counts.update(report["counts"])
    target.mkdir(parents=True)
    report = {
        "scope": "Original roof-plan clipping and finite native controls; no opacity, exact union, native mesh parity or runtime acceptance proof. Uncovered controls remain uncovered; invalid originals are not repaired. No full mesh used as selection input.",
        "counts": dict(counts),
        "numpy_version": np.__version__,
        "shapely_version": shapely.__version__,
        "geos_version": shapely.geos_version_string,
        "inputs": reports,
        "sources": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (Path(__file__).name, "building_roof_domains.py")
        },
    }
    (target / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(dict(counts)), flush=True)


if __name__ == "__main__":
    main()

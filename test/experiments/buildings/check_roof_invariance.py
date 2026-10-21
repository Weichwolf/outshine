"""Rigidly transform fixed roof inputs and compare the complete height surface."""

import argparse
import copy
import json
import time
from pathlib import Path

import numpy as np
import shapely
from compare_roof_partition import block_skeletons
from outshine_ref.buildings.modelling import prepare_hip
from verify_hip_fixture import verify


def transform_record(original, degrees, translation):
    row = copy.deepcopy(original)
    angle = np.deg2rad(degrees)
    rotation = np.array(
        [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
    )
    row["outer"] = (np.array(row["outer"]) @ rotation.T + translation).tolist()
    row["holes"] = [
        (np.array(ring) @ rotation.T + translation).tolist() for ring in row["holes"]
    ]
    row["frame"]["origin_xy_m"] = (
        np.array(row["frame"]["origin_xy_m"]) @ rotation.T + translation
    ).tolist()
    expected = row["expected"]
    expected["positions_local_xy_m"] = (
        np.array(expected["positions_local_xy_m"]) @ rotation.T
    ).tolist()
    for facet in expected["facets"]:
        facet["gradient"] = (np.array(facet["gradient"]) @ rotation.T).tolist()
    return row


def surface_error(row, plan):
    expected = row["expected"]
    nodes = np.array(expected["positions_local_xy_m"])
    offset = np.array(plan.origin) - row["frame"]["origin_xy_m"]
    reference = [shapely.Polygon(nodes[f["nodes"]]) for f in expected["facets"]]
    maximum, covered = 0.0, 0.0
    for facet in plan.facets:
        polygon = shapely.Polygon(np.array(facet.polygon.exterior.coords) + offset)
        for original, cut in zip(
            expected["facets"], shapely.intersection(reference, polygon), strict=True
        ):
            if cut.is_empty:
                continue
            points = shapely.get_coordinates(cut)
            before = points @ original["gradient"] + original["intercept_m"]
            after = (points - offset) @ facet.gradient + facet.intercept_m
            maximum = max(maximum, float(np.max(abs(before - after))))
            covered += cut.area
    area_error = abs(covered - plan.footprint.area)
    assert area_error <= max(1e-7, plan.footprint.area * 1e-10)
    return maximum, area_error


def check(row, tolerance):
    with block_skeletons("contour"):
        plan = prepare_hip(shapely.Polygon(row["outer"], row["holes"]), "contour")
    error, area_error = surface_error(row, plan)
    assert error <= tolerance["wavefront_plane_tolerance_m"]
    distances = np.array(row["expected"]["wavefront_distances_m"])
    for case in row["expected"]["cases"]:
        before = distances.max() * case["height_factor"]
        after = plan.distances_m.max() * plan.factor(case["value"], case["mode"])
        assert abs(before - after) <= tolerance["height_tolerance_m"]
    strict = True
    try:
        verify(row, tolerance, lambda _: plan)
    except AssertionError:
        strict = False
    return {
        "maximum_surface_height_error_m": error,
        "coverage_area_error_m2": area_error,
        "strict_facet_ownership_match": strict,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    data = json.loads(
        (Path(__file__).with_name("reference") / "hip-plans.json").read_text()
    )
    rows = []
    for degrees, translation in (
        (0, [100000.0, -200000.0]),
        (37, [0.0, 0.0]),
        (37, [100000.0, -200000.0]),
    ):
        for original in data["records"]:
            result = {
                "id": original["id"],
                "degrees": degrees,
                "translation_m": translation,
            }
            began = time.perf_counter()
            try:
                result.update(
                    check(
                        transform_record(original, degrees, translation),
                        data["comparison"],
                    )
                )
                result["accepted"] = True
            except (ValueError, RuntimeError, ArithmeticError, AssertionError) as error:
                result.update(
                    accepted=False, error=type(error).__name__ + ": " + str(error)
                )
            result["seconds_with_checks"] = time.perf_counter() - began
            rows.append(result)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "invariance.json").write_text(json.dumps(rows, indent=2) + "\n")
    accepted = sum(r["accepted"] for r in rows)
    print(f"Rigid-transform height surfaces: {accepted}/{len(rows)}")
    print(
        "Strict facet matches:",
        sum(r.get("strict_facet_ownership_match", False) for r in rows),
    )
    if accepted != len(rows):
        raise SystemExit(1)


if __name__ == "__main__":
    main()

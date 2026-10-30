"""Whole rail hardware on the already selected raised deck; no routing admission."""

import argparse
import json
from dataclasses import asdict
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from elevated_rail_joints import joints
from experiment_output import create_output
from outshine_ref.transport.rail_hardware import (
    RailBudget,
    RailRecipe,
    build_planar_rail_hardware,
)
from rail_consumers_buildings import bound_products
from stair_consumer_experiment import save
from stair_terrain_experiment import frozen


def bound_input(buildings, receipt):
    report = json.loads((buildings / "comparison.json").read_text())
    proof = json.loads(receipt.read_text())
    if proof.get("report_sha256") != digest(
        buildings / "comparison.json"
    ) or not proof.get("float32_bodies_against_actual_raised_volumes"):
        raise ValueError("Independently read whole raised Buildings input required")
    for name, expected in report["artifacts"].items():
        path = (buildings / name).resolve()
        if not path.is_relative_to(buildings.resolve()) or digest(path) != expected:
            raise ValueError("Complete actual saved Buildings changed")
    products = bound_products(report)
    if (
        report["frame"] != products.state[4]["frame"]
        or report["source_ids"] != products.state[4]["source_ids"]
        or report["separate_elevated_source_ids"]
        != [r["source_id"] for r in products.rows]
    ):
        raise ValueError("Exact whole ground/raised source and frame binding required")
    return products


def chains(rows):
    if not 1 <= len(rows) <= 8 or len({r["source_id"] for r in rows}) != len(rows):
        raise ValueError("Bounded distinct whole raised sources required")
    links = {}
    for joint in joints(rows):
        ends = []
        for index in joint["indices"]:
            row = rows[index]
            if not any(
                shapely.Polygon(p).covers(joint["pad"])
                for p in row.get("junction_pads_xy_m", ())
            ):
                raise ValueError(
                    "Already constructed explicit physical joint pad required"
                )
            end = next(
                k for k in (0, -1) if row["original_mvt_xy"][k] == joint["mvt_xy"]
            )
            ends.append((index, end))
        links[ends[0]], links[ends[1]] = ends[1], ends[0]
    unseen, result, omitted = set(range(len(rows))), [], []
    while unseen:
        component, pending = set(), [min(unseen)]
        while pending:
            index = pending.pop()
            if index in component:
                continue
            component.add(index)
            for end in (0, -1):
                if (index, end) in links:
                    pending.append(links[index, end][0])
        unseen -= component
        outer = [
            (i, end)
            for i in sorted(component)
            for end in (0, -1)
            if (i, end) not in links
        ]
        if len(outer) != 2:
            omitted.extend(
                {
                    "source_id": rows[i]["source_id"],
                    "reason": "periodic_hardware_join_not_qualified",
                }
                for i in sorted(component)
            )
            continue
        index, start = min(outer, key=lambda pair: (pair[0], pair[1] != 0))
        axis, ownership, members, visited = [], [], [], set()
        while index not in visited:
            visited.add(index)
            reverse = start == -1
            points = np.asarray(rows[index]["xy"], float)
            points = points[::-1] if reverse else points
            axis.extend(points if not axis else points[1:])
            ownership.extend([rows[index]["source_id"]] * (len(points) - 1))
            members.append({"source_id": rows[index]["source_id"], "reversed": reverse})
            finish = 0 if reverse else -1
            if (index, finish) not in links:
                break
            index, start = links[index, finish]
        if visited != component:
            raise ValueError("Every whole source must be consumed exactly once")
        result.append(
            {"members": members, "xy": np.array(axis), "segment_sources": ownership}
        )
    return result, omitted


def selected_bed(products):
    top = products.deck.vertices[: products.top_count]
    if not np.all(top[:, 2] == top[0, 2]):
        raise ValueError("This whole hardware consumer requires the qualified flat bed")
    bed = shapely.coverage_union_all(
        shapely.polygons(products.deck.vertices[products.top_faces, :2])
    )
    if bed.is_empty or not bed.is_valid:
        raise ValueError("Complete finite selected bed required")
    return bed, products.deck.vertices[products.top_faces[0]]


def conflict(shapes, previous):
    if not previous:
        return False
    current = np.array(shapes, object)
    prior = np.concatenate([np.array(p, object) for p in previous])
    kinds = np.arange(len(current)) >= 2
    prior_kinds = np.concatenate([np.arange(len(p)) >= 2 for p in previous])
    pairs = shapely.STRtree(prior).query(current, predicate="intersects")
    equal_kind = kinds[pairs[0]] == prior_kinds[pairs[1]]
    areas = shapely.area(
        shapely.intersection(current[pairs[0, equal_kind]], prior[pairs[1, equal_kind]])
    )
    return bool(np.any(areas > 0))


def generate(args):
    commit = frozen()
    create_output(args.output)
    products = bound_input(args.buildings, args.receipt)
    specs, omitted = chains(products.rows)
    bed, triangle = selected_bed(products)
    recipe, budget = RailRecipe(), RailBudget()
    rows, total_sleepers, footprints = [], 0, []
    template = None
    for spec in specs:
        keys = [m["source_id"] for m in spec["members"]]
        source_rows = [r for r in products.rows if r["source_id"] in keys]
        if any(
            r["original_properties"].get("gauge") not in (None, "") for r in source_rows
        ):
            omitted.extend(
                {"source_id": key, "reason": "supplied_gauge_requires_explicit_recipe"}
                for key in keys
            )
            continue
        remaining_sleepers = budget.maximum_sleepers - total_sleepers
        remaining_vertices = budget.maximum_rail_vertices - sum(
            r["construction"]["rail_vertices"] for r in rows
        )
        if remaining_sleepers <= 0 or remaining_vertices < 24 * len(spec["xy"]):
            omitted.extend(
                {"source_id": key, "reason": "whole_hardware_field_budget"}
                for key in keys
            )
            continue
        local_budget = RailBudget(
            budget.maximum_stations, remaining_sleepers, remaining_vertices
        )
        hardware, note = build_planar_rail_hardware(
            spec["xy"], triangle, recipe, local_budget
        )
        if hardware is None:
            omitted.extend({"source_id": key, "reason": note["reason"]} for key in keys)
            continue
        if not shapely.covers(bed, np.array(hardware.footprints, object)).all():
            omitted.extend(
                {"source_id": key, "reason": "whole_hardware_outside_selected_bed"}
                for key in keys
            )
            continue
        if conflict(hardware.footprints, footprints):
            omitted.extend(
                {"source_id": key, "reason": "rail_or_sleeper_volume_overlap"}
                for key in keys
            )
            continue
        if total_sleepers + len(hardware.sleeper_transforms) > budget.maximum_sleepers:
            omitted.extend(
                {"source_id": key, "reason": "whole_hardware_field_sleeper_budget"}
                for key in keys
            )
            continue
        total_sleepers += len(hardware.sleeper_transforms)
        footprints.append(hardware.footprints)
        name = f"component-{len(rows)}.npz"
        vertices = hardware.rails.vertices.copy()
        vertices[:, 2] = hardware.horizontal.rails.vertices[:, 2]
        transforms = hardware.sleeper_transforms.copy()
        transforms[:, 2, 3] = 0
        np.savez_compressed(
            args.output / name,
            rail_vertices=vertices,
            rail_triangles=hardware.rails.triangles,
            sleeper_transforms=transforms,
        )
        template = hardware.horizontal.sleeper
        rows.append(
            {
                "file": name,
                "sha256": digest(args.output / name),
                "members": spec["members"],
                "axis_xy_m": spec["xy"].tolist(),
                "segment_sources": spec["segment_sources"],
                "construction": note,
            }
        )
    artifacts = {r["file"]: r["sha256"] for r in rows}
    if template is not None:
        path = args.output / "sleeper-template.npz"
        np.savez_compressed(
            path, vertices=template.vertices, triangles=template.triangles
        )
        artifacts[path.name] = digest(path)
    report = {
        "version": 2,
        "commit": commit,
        "buildings": str(args.buildings.resolve()),
        "building_report_sha256": digest(args.buildings / "comparison.json"),
        "building_receipt": str(args.receipt.resolve()),
        "building_receipt_sha256": digest(args.receipt),
        "frame": products.state[4]["frame"],
        "ground_source_ids": products.state[4]["source_ids"],
        "raised_source_rows": [
            {k: v for k, v in r.items() if k != "xy"} for r in products.rows
        ],
        "recipe": asdict(recipe),
        "budget": asdict(budget),
        "datum_m": float(triangle[0, 2]),
        "object_origin_m": [0.0, 0.0, float(triangle[0, 2])],
        "object_axes": np.eye(3).tolist(),
        "running_surface_offset_m": recipe.sleeper_height_m + 0.172,
        "gauge_basis": "Explicit procedural recipe; supplied gauge is never silently replaced",
        "components": rows,
        "omitted": omitted,
        "artifacts": artifacts,
        "scope": "Whole hardware in a common local metric object frame on the existing selected finite flat raised bed and explicit physical endpads. Compact shared sleeper prototype/instances; no redundant saved lower corners. No new source/routing/vehicle-clearance/World/native/device admission; all old ground/deck/pier/consumer products retained.",
    }
    save(args.output, "report.json", report)
    print(
        json.dumps(
            {
                "components": len(rows),
                "sleepers": total_sleepers,
                "omitted": len(omitted),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--buildings", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    generate(parser.parse_args())

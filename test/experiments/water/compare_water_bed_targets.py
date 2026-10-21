"""Reuse geographic water for bed targets; reject an independently lowered P1 mask."""

import argparse
import json
import resource
from dataclasses import replace
from itertools import pairwise
from pathlib import Path

import numpy as np
import shapely
from compare_coarse_water_owner import measured
from compare_water_cell_field import read_field
from compare_water_geographic_query import geographic
from outshine_ref.places.sources import tile_geography
from water_bed_targets import bed_targets
from water_cell_mask import root_normal
from water_geographic_query import query_geographic
from water_nodal_height import cell_faces, parameter_triangle, sample_face


def lowered_roots(field, depth_m):
    """Rejected control: shift canonical vertices down, retain the parameter mask.

    Up comes from the original global lattice key, with no PROJ inverse or
    vertex-height roundtrip. Identical shared vertices remain identical.
    """
    directions = []
    for root in field.roots:
        xy = parameter_triangle(cell_faces(field.cell)[int(root)])
        lat, lon = tile_geography(xy, 1, (field.cell[0], 0, 0))
        lat, lon = np.deg2rad(lat), np.deg2rad(lon)
        directions.append(
            np.c_[np.cos(lat) * np.cos(lon), np.cos(lat) * np.sin(lon), np.sin(lat)]
        )
    world = field.world - depth_m * np.asarray(directions)
    return replace(
        field, world=world, normals=np.array([root_normal(t)[1] for t in world])
    )


def near_shore(field):
    """Finite stress poses, not a worldwide error rate or changed shore geometry."""
    faces = cell_faces(field.cell)
    shapes = [
        shapely.Polygon(parameter_triangle(faces[int(r)]) - field.cell[1:])
        for r in field.roots
    ]
    parameters, world, skipped = [], [], 0
    for polygon in shapely.get_parts(field.mask):
        for ring in (polygon.exterior, *polygon.interiors):
            for a, b in pairwise(np.asarray(ring.coords)):
                edge = b - a
                length = np.linalg.norm(edge)
                if length == 0:
                    continue
                normal = np.array([-edge[1], edge[0]]) / length
                for step in (0, -1e-10, 1e-10, -1e-12, 1e-12):
                    point = (a + b) / 2 + step * normal
                    hits = np.flatnonzero(shapely.covers(shapes, shapely.Point(point)))
                    if not len(hits):
                        skipped += 1
                        continue
                    if len(world) == 4096:
                        raise ValueError("shore probe exceeds explicit4096-query limit")
                    at = int(hits[0])
                    parameters.append(point)
                    world.append(
                        sample_face(
                            faces[int(field.roots[at])],
                            field.world[at],
                            np.array([point]),
                            origin=field.cell[1:],
                        )[0]
                    )
    lon_lat, _height = geographic(world)
    return np.asarray(parameters), lon_lat, skipped


def compare(field, depth_m, output):
    parameter, points, skipped = near_shore(field)
    lowered, preparation = measured(lambda: lowered_roots(field, depth_m))
    water, water_cost = measured(lambda: query_geographic(field, points))
    naive, naive_cost = measured(lambda: query_geographic(lowered, points))
    targets, target_cost = measured(lambda: bed_targets(water, depth_m=depth_m))
    repeated, repeated_cost = measured(
        lambda: bed_targets(query_geographic(field, points), depth_m=depth_m)
    )
    np.testing.assert_array_equal(targets.height_m, repeated.height_m)
    np.testing.assert_array_equal(targets.height_bounds_m, repeated.height_bounds_m)
    assert repeated.water.states == water.states
    for name in ("source_offsets", "source_members", "parents"):
        np.testing.assert_array_equal(
            getattr(repeated.water, name), getattr(water, name)
        )
    wet, other = np.array(water.states) == "water", np.array(naive.states) == "water"
    wrong = np.flatnonzero(wet != other)
    exact_results = []
    if len(wrong):
        for model, observed in ((field, water), (lowered, naive)):
            exact = query_geographic(model, points[wrong], exact=True)
            assert exact.states == tuple(observed.states[i] for i in wrong)
            exact_results.append(exact.report)
    report = {
        "cell": field.cell,
        "queries": len(points),
        "skipped_outside_available_roots": skipped,
        "nominal_depth_m": depth_m,
        "water_targets": int(wet.sum()),
        "lowered_mask_lost_bed": int((wet & ~other).sum()),
        "lowered_mask_extra_bed": int((~wet & other).sum()),
        "rational_checks": exact_results,
        "water_work": water.report,
        "target_work": targets.report,
        "naive_work": naive.report,
        "lowered_preparation": preparation,
        "water_cost": water_cost,
        "targets_from_existing_water_cost": target_cost,
        "repeated_water_consumer_cost": repeated_cost,
        "lowered_query_cost": naive_cost,
        "repeated_consumer_exact": True,
    }
    path = output / ("targets-" + "-".join(map(str, field.cell)) + ".npz")
    np.savez_compressed(
        path,
        parameter=parameter,
        longitude_latitude_deg=points,
        water_state=np.asarray(water.states),
        lowered_state=np.asarray(naive.states),
        water_height_m=water.height_m,
        target_height_m=targets.height_m,
        target_height_bounds_m=targets.height_bounds_m,
        source_offsets=water.source_offsets,
        source_members=water.source_members,
    )
    return report, (field, parameter, wet != other)


def display(rows, reports, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(1, 2, figsize=(12, 6))
    origin = np.mean([field.cell[1:] for field, _points, _wrong in rows], axis=0)
    for field, points, wrong in rows:
        offset = field.cell[1:] - origin
        for polygon in shapely.get_parts(field.mask):
            for ring in (polygon.exterior, *polygon.interiors):
                axes[0].plot(*(np.asarray(ring.coords) + offset).T, color="0.6", lw=0.7)
        axes[0].scatter(*(points[wrong] + offset).T, color="#c54720", s=15)
    axes[0].set_aspect("equal")
    axes[0].set_title("Original water masks; orange: changed bed membership")
    axes[0].set_xlabel("Relative Mercator tile units")
    axes[1].bar(
        ["Lost bed below water", "Bed outside water"],
        [
            sum(r["lowered_mask_lost_bed"] for r in reports),
            sum(r["lowered_mask_extra_bed"] for r in reports),
        ],
        color=["#c54720", "#b78b38"],
    )
    axes[1].set_ylabel("Selected near-shore query count")
    axes[1].set_title("Rejected independently lowered P1-mask control")
    figure.suptitle("Bed targets reuse water membership; no bed mesh or observed depth")
    figure.tight_layout()
    figure.savefig(output / "bed-targets.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fields", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--depth-m", type=float, required=True)
    args = parser.parse_args()
    paths = sorted(args.fields.glob("field-*.npz"))
    if not 1 <= len(paths) <= 16 or not np.isfinite(args.depth_m) or args.depth_m < 0:
        raise ValueError("one to16fixed fields and explicit nonnegative depth required")
    args.output.mkdir(parents=True, exist_ok=True)
    reports, rows = [], []
    for path in paths:
        report, row = compare(read_field(path), args.depth_m, args.output)
        reports.append(report)
        rows.append(row)
    display(rows, reports, args.output)
    report = {
        "fields": reports,
        "process_peak_rss_bytes_including_diagnostics": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Selected finite original P1 fields and shore stresses. Repeated-water consumer is a synthetic same-output control, not an existing native path. Point targets do not admit a full bed/terrain mesh, observed bathymetry, global guide coverage or device budget.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "fields": len(rows),
                "lost_bed": sum(r["lowered_mask_lost_bed"] for r in reports),
                "extra_bed": sum(r["lowered_mask_extra_bed"] for r in reports),
            }
        )
    )


if __name__ == "__main__":
    main()

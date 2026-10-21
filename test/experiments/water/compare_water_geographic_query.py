"""Geographic verticals versus parameter aliases, with a rational oracle."""

import argparse
import json
import resource
from collections import Counter
from itertools import pairwise
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT
from compare_coarse_water_owner import measured
from compare_water_cell_field import prepare
from compare_water_parameter_cut import run as render_cuts
from outshine_ref.places.sources import tile_geography
from pyproj import Transformer
from water_cell_field import sample_parameters
from water_geographic_query import query_geographic
from water_nodal_height import cell_faces, parameter_triangle, sample_face

INVERSE = Transformer.from_crs(4978, 4979, always_xy=True)


def geographic(world):
    lon, lat, height = INVERSE.transform(*np.asarray(world).T)
    return np.c_[lon, lat], np.asarray(height)


def naive_parameters(field, points):
    lon, lat = points.T
    n = 2 ** field.cell[0]
    return (
        np.c_[
            (lon + 180) / 360 * n,
            (1 - np.arcsinh(np.tan(np.deg2rad(lat))) / np.pi) / 2 * n,
        ]
        - field.cell[1:]
    )


def compare(field, points):
    result, filtered_cost = measured(lambda: query_geographic(field, points))
    oracle, exact_cost = measured(
        lambda: query_geographic(field, points, exact=True, predicate_limit=16777216)
    )
    if result.states != oracle.states:
        raise ValueError(
            "filtered geographic classification differs from rational oracle"
        )
    for a, b in (
        (result.parents, oracle.parents),
        (result.source_offsets, oracle.source_offsets),
        (result.source_members, oracle.source_members),
    ):
        np.testing.assert_array_equal(a, b)
    wet = np.array(result.states) == "water"
    if not (
        (result.height_bounds_m[wet, 0] <= oracle.height_m[wet]).all()
        and (result.height_bounds_m[wet, 1] >= oracle.height_m[wet]).all()
    ):
        raise ValueError("oracle height outside filtered arithmetic enclosure")
    alias = sample_parameters(field, naive_parameters(field, points))
    alias_wet = np.array(alias.states) == "water"
    both = wet & alias_wet
    displacement = np.linalg.norm(alias.world[both] - result.world[both], axis=1)
    report = {
        "queries": len(points),
        "states": dict(Counter(result.states)),
        "filtered": result.report,
        "rational_oracle": oracle.report,
        "filtered_cost": filtered_cost,
        "rational_cost": exact_cost,
        "naive_false_water": int((alias_wet & ~wet).sum()),
        "naive_false_miss": int((~alias_wet & wet).sum()),
        "naive_maximum_world_displacement_m": float(displacement.max(initial=0)),
        "maximum_height_enclosure_width_m": float(
            np.diff(result.height_bounds_m[wet], axis=1).max(initial=0)
        ),
    }
    return result, alias_wet, report


def banks(field):
    root_shapes = [
        shapely.Polygon(
            parameter_triangle(cell_faces(field.cell)[int(r)]) - field.cell[1:]
        )
        for r in field.roots
    ]
    points, world, expected = [], [], []
    skipped = 0
    for polygon in shapely.get_parts(field.mask):
        for ring in (polygon.exterior, *polygon.interiors):
            for a, b in pairwise(np.asarray(ring.coords)):
                edge = b - a
                length = np.linalg.norm(edge)
                if length == 0:
                    continue
                normal = np.array([-edge[1], edge[0]]) / length
                for side in (-1, 1):
                    xy = (a + b) / 2 + side * 1e-6 * normal
                    if np.any(xy <= 0) or np.any(xy >= 1):
                        skipped += 1
                        continue
                    roots = np.flatnonzero(
                        shapely.covers(root_shapes, shapely.Point(xy))
                    )
                    if not len(roots):
                        skipped += 1
                        continue
                    at = int(roots[0])
                    root = int(field.roots[at])
                    placed = sample_face(
                        cell_faces(field.cell)[root],
                        field.world[at],
                        xy.reshape(1, 2),
                        origin=field.cell[1:],
                    )[0]
                    points.append(xy)
                    world.append(placed)
                    expected.append(field.mask.covers(shapely.Point(xy)))
    return np.asarray(points), np.asarray(world), np.asarray(expected), skipped


def plot(rows, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axis = plt.subplots(figsize=(8, 9))
    centre = np.mean([r[0][1:] for r in rows], axis=0)
    for cell, field, xy, wet, alias in rows:
        placed = xy + cell[1:] - centre
        axis.scatter(*placed[wet].T, color="#147ba6", s=5)
        wrong = wet != alias
        axis.scatter(
            *placed[wrong].T,
            facecolors="none",
            edgecolors="#cb4b40",
            s=18,
            linewidths=0.6,
        )
        for polygon in shapely.get_parts(field.mask):
            outline = np.asarray(polygon.exterior.coords) + cell[1:] - centre
            axis.plot(*outline.T, color="0.65", linewidth=0.7)
    axis.set_aspect("equal")
    axis.set_xlabel("Mercator tile units relative to cohort centre")
    axis.set_ylabel("Mercator tile units")
    axis.set_title(
        "Bank stress queries derived from original water masks\nBlue: vertical hit; red rings: naive Mercator classification differs"
    )
    figure.tight_layout()
    figure.savefig(output / "geographic-banks.png", dpi=150)
    plt.close(figure)


def neighbour_audit(fields, render, output):
    points = []
    for contact in render["contacts"]:
        cell, other = map(tuple, contact["cells"])
        axis = 0 if cell[1] != other[1] else 1
        filename = "cell-" + "-".join(map(str, cell)) + ".npz"
        with np.load(output / filename) as data:
            edges = data["boundary"]
            on_edge = np.all(data["parameter"][edges, axis] == 1, axis=1)
            points.extend(data["world"][edges[on_edge]].mean(axis=1))
    geographic_points, _ = geographic(points)
    samples = [query_geographic(field, geographic_points) for field in fields.values()]
    minimum = len(fields)
    for i in range(len(points)):
        hits = [sample for sample in samples if sample.states[i] == "water"]
        if not hits:
            raise ValueError(
                "geographic query missed both sides of a shared water boundary"
            )
        minimum = min(minimum, len(hits))
        if max(s.height_bounds_m[i, 0] for s in hits) > min(
            s.height_bounds_m[i, 1] for s in hits
        ):
            raise ValueError("shared geographic height intervals do not overlap")
    return {
        "shared_segment_midpoints": len(points),
        "minimum_ready_field_hits": minimum,
        "missed_by_all_fields": 0,
    }


def run(path, output):
    output.mkdir(parents=True, exist_ok=True)
    field_output = output / "fields"
    field_output.mkdir(exist_ok=True)
    rows, plotted = [], []
    with patch(
        "shapely.constrained_delaunay_triangles",
        side_effect=AssertionError("fine water required for geographic query"),
    ):
        fields, preparation, origin, _, _ = prepare(path, field_output)
        for cell, field in sorted(fields.items()):
            grid = (np.arange(16) + 0.5) / 16
            points = np.array(np.meshgrid(grid, grid)).reshape(2, -1).T
            lat, lon = tile_geography(points, 1, cell)
            _, _, regular = compare(field, np.c_[lon, lat])
            xy, world, expected, skipped = banks(field)
            points, height = geographic(world)
            result, alias, bank = compare(field, points)
            wet = np.array(result.states) == "water"
            np.testing.assert_array_equal(wet, expected)
            error = abs(result.height_m[wet] - height[wet])
            bank["maximum_input_world_height_residual_m"] = float(error.max(initial=0))
            if error.max(initial=0) > 64 * np.spacing(6378137.0):
                raise ValueError("bank height lost georeferenced coordinate agreement")
            bank["skipped_test_poses_outside_cell_or_unavailable_support"] = skipped
            rows.append({"cell": cell, "regular": regular, "banks": bank})
            plotted.append((cell, field, xy, wet, alias))
    render = render_cuts(path, output / "render")
    for row in rows:
        field = fields[tuple(row["cell"])]
        filename = "cell-" + "-".join(map(str, field.cell)) + ".npz"
        for name, directory in (
            ("render", output / "render"),
            ("refined", output / "render/refined"),
        ):
            with np.load(directory / filename) as data:
                world = data["world"][data["faces"]].mean(axis=1)
            points, height = geographic(world)
            result, _, report = compare(field, points)
            if any(s != "water" for s in result.states):
                raise ValueError(
                    "a rendered interior point missed the geographic field"
                )
            report["maximum_input_world_height_residual_m"] = float(
                abs(result.height_m - height).max()
            )
            if report["maximum_input_world_height_residual_m"] > 64 * np.spacing(
                6378137.0
            ):
                raise ValueError(
                    "render height lost georeferenced coordinate agreement"
                )
            row[name] = report
    report = {
        "source": origin,
        "preparation": preparation,
        "cells": rows,
        "ready_cells": len(fields),
        "pending_cells": len(preparation) - len(fields),
        "fine_triangulations_before_geographic_queries": 0,
        "render_faces": render["facets"],
        "neighbours": neighbour_audit(fields, render, output / "render"),
        "pre_plot_process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "stored Float64 P1 model and declared source polygons; no observed geoid, bed, global coverage, native or device claim",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    plot(plotted, output)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = run(args.input, args.output)
    print(
        json.dumps(
            {k: report[k] for k in ("ready_cells", "pending_cells", "render_faces")}
        )
    )

"""Functional parameter queries before independently building fine water cuts."""

import argparse
import json
import resource
from collections import Counter, defaultdict
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT
from compare_coarse_water_owner import measured
from compare_water_grid_height import inputs
from compare_water_parameter_cut import run as render_cuts
from outshine_ref.geometry.sampling import owned
from shapely.affinity import translate
from water_cell_field import CellField, field_facet, prepare_field, sample_parameters
from water_colored_reference import ReferenceUpdates
from water_envelope_fixture import acquire, prepare_parent
from water_nodal_height import cell_faces, parameter_triangle
from water_parameter_cut import cut_cell


def replay_field(field, output):
    arrays = {
        "roots": field.roots,
        "world": field.world,
        "normals": field.normals,
        "cell": np.asarray(field.cell),
        "context": np.asarray(field.context),
        "source_keys": np.asarray(json.dumps(field.source_keys)),
        "polygons": np.asarray([p.wkb_hex for p in field.polygons]),
        "mask": np.asarray(field.mask.wkb_hex),
    }
    path = output / ("field-" + "-".join(map(str, field.cell)) + ".npz")
    np.savez_compressed(path, **arrays)
    with np.load(path) as saved:
        for key, value in arrays.items():
            np.testing.assert_array_equal(saved[key], value)
    return read_field(path, report=field.report)


def read_field(path, *, report=None):
    """Read only the generated diagnostic NPZ; this is not a native cache codec."""
    with np.load(path) as saved:
        return CellField(
            tuple(int(v) for v in saved["cell"]),
            saved["context"].item(),
            tuple(json.loads(saved["source_keys"].item())),
            tuple(shapely.from_wkb(saved["polygons"])),
            shapely.from_wkb(saved["mask"].item()),
            *[
                owned(saved[key], saved[key].dtype)
                for key in ("roots", "world", "normals")
            ],
            report or {},
        )


def prepare(path, output):
    data, snapshots, epoch, receipts, order = acquire(path)
    parent, provenance = prepare_parent(data, snapshots, receipts)
    recipe = ReferenceUpdates(
        receipts,
        epoch,
        snapshots,
        data,
        order,
        parent=parent,
        parent_version=parent.version,
    )
    _, records, _, origin = inputs(path)
    context = f"water-param-cut-v1:{parent.version}:{origin['input_sha256']}"
    groups = defaultdict(list)
    for record in records:
        groups[record[0]].append(record)

    def facet(keys):
        nodes = [parent(key) for key in keys]
        if any(node.state != "ready" for node in nodes):
            return "pending_parent", None
        world = np.array([node.world for node in nodes])
        return recipe.admissible(keys, world), world

    fields, rows = {}, []
    for cell, selected in sorted(groups.items()):
        polygons = [r[1].polygon for r in selected]
        keys = tuple((r[0], r[3], r[1].feature, r[1].part, r[2]) for r in selected)
        (field, report), cost = measured(
            lambda cell=cell, polygons=polygons, keys=keys: prepare_field(
                cell, polygons, keys, facet, context=context
            )
        )
        row = {"cell": cell, "prepare": report, "cost": cost}
        if field is not None:
            fields[cell] = replay_field(field, output)
            row["source_polygon_wkb_bytes"] = sum(len(p.wkb) for p in field.polygons)
            row["union_mask_wkb_bytes"] = len(field.mask.wkb)
        rows.append(row)
    return fields, rows, origin, provenance, dict(recipe.work)


def mesh_audit(field, path):
    with np.load(path) as data:
        if data["context"].item() != field.context:
            raise ValueError("render and functional field context differ")
        xy = data["parameter"][data["faces"]].mean(axis=1)
        world = data["world"][data["faces"]].mean(axis=1)
        sample = sample_parameters(field, xy)
        if any(s != "water" for s in sample.states):
            raise ValueError("render interior is not classified as water")
        error = np.linalg.norm(sample.world - world, axis=1)
        # Existing georeferenced-coordinate roundoff criterion, not a relaxed
        # physical bound or a new error permission for the generated surface.
        if error.max() > 64 * np.spacing(6378137.0):
            raise ValueError("functional sample lost the rendered P1 position")
        expected = data["parent_normals"][data["parents"]]
        np.testing.assert_array_equal(sample.normals, expected)
        render_sources = json.loads(data["source_keys_json"].item())
        if render_sources != json.loads(json.dumps(field.source_keys)):
            raise ValueError("functional and render source tables differ")
        for i in range(len(xy)):
            point_members = set(
                sample.source_members[
                    sample.source_offsets[i] : sample.source_offsets[i + 1]
                ]
            )
            face_members = set(
                data["source_members"][
                    data["source_offsets"][i] : data["source_offsets"][i + 1]
                ]
            )
            if not point_members <= face_members:
                raise ValueError("point source missing from render overlap CSR")
        return {
            "queries": len(xy),
            "maximum_world_difference_m": float(error.max()),
            "work": sample.report,
        }


def cached_render_audit(field, path):
    polygons = [
        translate(p, xoff=field.cell[1], yoff=field.cell[2]) for p in field.polygons
    ]
    with patch.object(
        ReferenceUpdates,
        "target",
        side_effect=AssertionError("cached field reacquired source profile"),
    ):
        (cut, report), cost = measured(
            lambda: cut_cell(
                field.cell,
                polygons,
                field.source_keys,
                lambda keys: field_facet(field, keys),
                context=field.context,
            )
        )
    if cut is None:
        raise ValueError("cached functional field could not supply render roots")
    with np.load(path) as data:
        np.testing.assert_array_equal(cut.parameter, data["parameter"][data["faces"]])
        np.testing.assert_array_equal(cut.world, data["world"][data["faces"]])
        np.testing.assert_array_equal(
            cut.normals, data["parent_normals"][data["parents"]]
        )
    return {
        "render_faces": len(cut.world),
        "source_profile_preparations": 0,
        "cut": report,
        "cost": cost,
    }


def plot(fields, queries, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axis = plt.subplots(figsize=(8, 9))
    centre = np.mean([cell[1:] for cell in fields], axis=0)
    for cell, field in fields.items():
        points, samples = queries[cell]
        for state, color, size in (("dry", "0.8", 2), ("water", "#147ba6", 10)):
            selected = np.array(samples.states) == state
            xy = points[selected] + cell[1:] - centre
            axis.scatter(*xy.T, color=color, s=size)
        for polygon in shapely.get_parts(field.mask):
            for ring in (polygon.exterior, *polygon.interiors):
                xy = np.asarray(ring.coords) + cell[1:] - centre
                axis.plot(*xy.T, color="#147ba6", linewidth=0.8)
    axis.set_aspect("equal")
    axis.set_xlabel("Mercator tile units relative to cohort centre")
    axis.set_ylabel("Mercator tile units")
    axis.set_title(
        "Functional water queries before fine triangulation\nBlue: water; grey: dry; finite five-cell snapshot"
    )
    figure.tight_layout()
    figure.savefig(output / "functional-queries.png", dpi=150)
    plt.close(figure)


def run(path, output):
    output.mkdir(parents=True, exist_ok=True)
    # Actual callback refusal proves that acquisition, field preparation and
    # these first functional queries do not call the fine CDT operation.
    with patch(
        "shapely.constrained_delaunay_triangles",
        side_effect=AssertionError("render detail requested before functional query"),
    ):
        fields, rows, origin, provenance, adapter = prepare(path, output)
        grid = (np.arange(32) + 0.5) / 32
        points = np.array(np.meshgrid(grid, grid)).reshape(2, -1).T
        queries = {
            cell: (points, sample_parameters(field, points))
            for cell, field in fields.items()
        }
    statuses = Counter(
        state for _, sample in queries.values() for state in sample.states
    )
    render = render_cuts(path, output / "render")
    for row in rows:
        cell = tuple(row["cell"])
        if cell not in fields:
            continue
        field = fields[cell]
        filename = "cell-" + "-".join(map(str, cell)) + ".npz"
        row["queries"] = queries[cell][1].report
        support = shapely.union_all(
            [
                shapely.Polygon(parameter_triangle(cell_faces(cell)[r]) - cell[1:])
                for r in field.roots
            ]
        )
        point_shapes = shapely.points(queries[cell][0])
        row["unmasked_root_false_water_points"] = int(
            (
                shapely.covers(support, point_shapes)
                & ~shapely.covers(field.mask, point_shapes)
            ).sum()
        )
        row["render_interiors"] = mesh_audit(field, output / "render" / filename)
        row["render_from_cached_field"] = cached_render_audit(
            field, output / "render" / filename
        )
        row["refined_interiors"] = mesh_audit(
            field, output / "render/refined" / filename
        )
    report = {
        "source": origin,
        "source_provenance": provenance,
        "cells": rows,
        "functional_cells": len(fields),
        "pending_cells": len(rows) - len(fields),
        "query_states": dict(statuses),
        "fine_triangulations_during_functional_preparation": 0,
        "field_root_array_bytes": sum(
            f.report["numerical_array_bytes"] for f in fields.values()
        ),
        "query_output_array_bytes": sum(
            sample.report["output_array_bytes"] for _, sample in queries.values()
        ),
        "render_product_bytes": render["indexed_numerical_array_bytes"],
        "physical_adapter_work": adapter,
        "pre_plot_process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "parameter-to-P1 ECEF query; no geographic vertical-height, bed recipe, whole-world, DB IO or native device claim",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    plot(fields, queries, output)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = run(args.input, args.output)
    print(
        json.dumps(
            {
                key: result[key]
                for key in (
                    "functional_cells",
                    "pending_cells",
                    "query_states",
                    "field_root_array_bytes",
                )
            }
        )
    )

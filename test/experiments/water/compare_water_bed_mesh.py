"""Finite-cohort bed candidates; explicit shore recipe and unknown outer ports."""

import argparse
import json
import time
from collections import defaultdict
from pathlib import Path

import numpy as np
from compare_coarse_water_owner import measured
from compare_water_geographic_query import geographic
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from outshine_ref.places.camera import axes
from river_membership import Membership
from water_bed_distance import BankDistance
from water_bed_fit import fitted_bed
from water_bed_mesh import build_bed, refine_bed
from water_indexed_cell import IndexedCell


def timed(call):
    wall, cpu = time.perf_counter(), time.process_time()
    result = call()
    return result, {
        "wall_seconds": time.perf_counter() - wall,
        "process_cpu_seconds": time.process_time() - cpu,
    }


def read_water(path):
    """The existing diagnostic NPZ, not a native asset or new cache format."""
    with np.load(path) as data:
        names = ("parameter", "world", "faces", "parents", "parent_normals", "boundary")
        arrays = {name: data[name] for name in names}
        csr = {
            name: data["source_" + name]
            for name in (
                "offsets",
                "members",
                "representative",
                "member_area_m2",
                "uncovered_area_m2",
                "status",
            )
        }
        for array in (*arrays.values(), *csr.values()):
            array.setflags(write=False)
        return IndexedCell(
            tuple(map(int, data["cell"])),
            data["context"].item(),
            tuple(json.loads(data["source_keys_json"].item())),
            membership=Membership(**csr, work={}),
            report={},
            **arrays,
        )


def shore_inputs(cells):
    """Full admitted cell masks are the prerequisite, not arbitrary mesh fragments.

    A cell-side boundary is an interface or unknown, never inferred dry/shore.
    Other boundaries belong to the producer's full original mask, including holes.
    This finite cohort does not establish a global shore-acquisition stencil.
    """
    if (
        not 0 < len(cells) <= 16
        or len({c.cell for c in cells}) != len(cells)
        or len({c.context for c in cells}) != 1
    ):
        raise ValueError("bounded unique cells from one immutable context required")
    shores, banks, ports = {}, [], defaultdict(list)
    for cell in cells:
        selected = []
        for edge in cell.boundary:
            is_side = any(
                np.all(cell.parameter[edge, axis] == side)
                for axis in (0, 1)
                for side in (0, 1)
            )
            if is_side:
                key = tuple(sorted(map(tuple, cell.world[edge])))
                ports[key].append(cell.cell)
            else:
                selected.append(edge)
                banks.append(cell.world[edge])
        shores[cell.cell] = np.asarray(selected, dtype=np.uint32).reshape(-1, 2)
    unknown = set()
    paired = 0
    for owners in ports.values():
        if len(owners) == 1:
            unknown.add(owners[0])
        elif (
            len(owners) == 2
            and owners[0][0] == owners[1][0]
            and sum(
                abs(a - b) for a, b in zip(owners[0][1:], owners[1][1:], strict=True)
            )
            == 1
        ):
            paired += 1
        else:
            raise ValueError("nonconforming or non-neighbour water interface")
    return shores, np.asarray(banks), unknown, paired


def consumer(bed):
    anchor = bed.vertices.mean(axis=0)
    ll, _ = geographic(anchor.reshape(1, 3))
    basis = axes(ll[0, 1], ll[0, 0])
    local = (bed.vertices - anchor) @ basis.T
    mesh = Mesh(local, bed.faces)
    points = local[bed.faces].mean(axis=1)
    sampled = SurfaceSampler(mesh, xy_tolerance_m=0).sample(points[:, :2])
    if np.any(sampled.face_indices < 0):
        raise ValueError("actual bed query has missing or overlapping facets")
    error = float(abs(sampled.points_enu_m[:, 2] - points[:, 2]).max())
    if error > bed.report["coordinate_roundoff_allowance_m"]:
        raise ValueError("bed query lost the actual mesh height")
    return {
        "queries": len(points),
        "maximum_height_error_m": error,
        "sampling": dict(sampled.report),
    }, local


def depth_inputs(layout, distance, depth_m, grade):
    depth = np.minimum(depth_m, grade * distance)
    depth[layout.bank_vertices] = 0
    ll, _ = geographic(layout.top_vertices)
    return depth, np.array([axes(lat, lon)[2] for lon, lat in ll])


def construct(cell, shores, bank, depth_m, grade, method, measure, options):
    layout, layout_cost = measure(lambda: refine_bed(cell, shores))
    (distance, work), distance_cost = measure(
        lambda: bank.query(layout.top_vertices, cap_m=depth_m / grade, method=method)
    )
    (depth, up), target_cost = measure(
        lambda: depth_inputs(layout, distance, depth_m, grade)
    )
    bed, build_cost = measure(lambda: build_bed(layout, depth, up))
    fit_cost = None
    if options.construction == "fit":
        bed, fit_cost = measure(
            lambda: fitted_bed(
                layout,
                bed,
                directional_grade=options.directional_grade,
                max_depth_m=options.max_depth_m,
                seconds=options.fit_seconds,
                grade_reserve=options.grade_reserve,
            )
        )
    query, local = consumer(bed)
    report = {
        "cell": cell.cell,
        "state": "local_bed_candidate"
        if options.construction == "fit"
        else "unconstrained_control",
        "layout_cost": layout_cost,
        "distance_cost": distance_cost,
        "distance_work": work,
        "target_preparation_cost": target_cost,
        "build_cost": build_cost,
        "physical_fit_cost": fit_cost,
        "geometry": bed.report,
        "consumer": query,
    }
    return bed, depth, local, report


def shared_bed_edges(products):
    edges = defaultdict(list)
    for bed in products.values():
        original = sorted({tuple(sorted(map(int, e))) for e in bed.water.boundary})
        for index, edge in enumerate(original):
            ids = np.asarray(edge, dtype=np.uint32)
            if not any(
                np.all(bed.water.parameter[ids, axis] == side)
                for axis in (0, 1)
                for side in (0, 1)
            ):
                continue
            key = tuple(sorted(map(tuple, bed.water.world[ids])))
            actual = tuple(
                sorted(
                    tuple(sorted(map(tuple, bed.vertices[e])))
                    for e in bed.boundary[2 * index : 2 * index + 2]
                )
            )
            edges[key].append(actual)
    for profiles in edges.values():
        if len(profiles) == 2 and profiles[0] != profiles[1]:
            raise ValueError("adjacent bed boundary profiles differ")
    return {
        "matched_original_segments": sum(len(p) == 2 for p in edges.values()),
        "matched_bed_subsegments": 2 * sum(len(p) == 2 for p in edges.values()),
        "open_bed_port_segments": sum(len(p) == 1 for p in edges.values()),
    }


def display(rows, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    bed, depth, local = rows[len(rows) // 2]
    figure = plt.figure(figsize=(12, 6))
    plan = figure.add_subplot(121)
    picture = plan.tripcolor(
        local[:, 0], local[:, 1], bed.faces, depth, cmap="Blues", shading="gouraud"
    )
    plan.set_aspect("equal")
    plan.set_title("Actual bed vertices / explicit target depth")
    figure.colorbar(picture, ax=plan, label="Nominal depth m")
    view = figure.add_subplot(122, projection="3d")
    triangles = local[bed.faces]
    chosen = np.linalg.norm(triangles.mean(axis=1)[:, :2], axis=1) < 80
    if not chosen.any():
        chosen[:] = True
    visible = triangles[chosen]
    view.add_collection3d(
        Poly3DCollection(
            visible, facecolor="#997445", edgecolor="#57442d", linewidth=0.08
        )
    )
    anchor = bed.vertices.mean(axis=0)
    ll, _ = geographic(anchor.reshape(1, 3))
    basis = axes(ll[0, 1], ll[0, 0])
    water = (bed.water.world - anchor) @ basis.T
    water_tri = water[bed.water.faces]
    near = np.linalg.norm(water_tri.mean(axis=1)[:, :2], axis=1) < 80
    if near.any():
        view.add_collection3d(
            Poly3DCollection(
                water_tri[near], facecolor="#3388bd", alpha=0.2, linewidth=0
            )
        )
    view.auto_scale_xyz(
        visible[:, :, 0].ravel(), visible[:, :, 1].ravel(), visible[:, :, 2].ravel()
    )
    view.set_box_aspect((1, 1, 0.35))
    view.view_init(elev=35, azim=-70)
    view.set_title("Local bed below translucent water; height exaggerated")
    figure.suptitle(
        "Finite-cohort bed candidate; outer ports and terrain integration remain open"
    )
    figure.tight_layout()
    figure.savefig(output / "bed-mesh.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--water", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--depth-m", type=float, required=True)
    parser.add_argument("--bank-grade", type=float, required=True)
    parser.add_argument("--method", choices=("scan", "index"), default="index")
    parser.add_argument("--image", action="store_true")
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--construction", choices=("direct", "fit"), default="fit")
    parser.add_argument("--directional-grade", type=float, required=True)
    parser.add_argument("--max-depth-m", type=float, required=True)
    parser.add_argument("--fit-seconds", type=float, required=True)
    parser.add_argument("--grade-reserve", type=float, required=True)
    args = parser.parse_args()
    if (
        not np.isfinite(args.depth_m)
        or not 0 < args.depth_m <= 100
        or not np.isfinite(args.bank_grade)
        or not 1e-3 <= args.bank_grade <= 2
        or not np.isfinite(args.directional_grade)
        or not 0 < args.directional_grade <= 2
        or not np.isfinite(args.max_depth_m)
        or not args.depth_m <= args.max_depth_m <= 100
        or not np.isfinite(args.fit_seconds)
        or not 0 <= args.fit_seconds <= 60
        or not np.isfinite(args.grade_reserve)
        or not 0 <= args.grade_reserve < 0.5
    ):
        raise ValueError("explicit bounded local recipe required")
    paths = sorted(args.water.glob("cell-*.npz"))
    if not 0 < len(paths) <= 16:
        raise ValueError("one to16 admitted diagnostic cells required")
    measure = measured if args.trace_memory else timed
    cells, input_cost = measure(lambda: [read_water(p) for p in paths])
    (shores, segments, unknown, paired), shore_cost = measure(
        lambda: shore_inputs(cells)
    )
    bank, cost = measure(
        lambda: BankDistance(segments, prepare_index=args.method == "index")
    )
    reports = []
    rows = []
    products = {}
    args.output.mkdir(parents=True, exist_ok=True)
    for cell in cells:
        if cell.cell in unknown:
            reports.append({"cell": cell.cell, "state": "pending_outer_bed_ports"})
            continue
        try:
            bed, depth, local, report = construct(
                cell,
                shores[cell.cell],
                bank,
                args.depth_m,
                args.bank_grade,
                args.method,
                measure,
                args,
            )
        except ValueError as error:
            reports.append(
                {"cell": cell.cell, "state": "bed_refused", "reason": str(error)}
            )
            continue
        reports.append(report)
        rows.append((bed, depth, local))
        products[cell.cell] = bed
        np.savez_compressed(
            args.output / ("bed-" + "-".join(map(str, cell.cell)) + ".npz"),
            vertices=bed.vertices,
            faces=bed.faces,
            parent_faces=bed.parent_faces,
            boundary=bed.boundary,
            normals=bed.normals,
            depth_m=depth,
        )
    if args.image and rows:
        display(rows, args.output)
    report = {
        "cells": reports,
        "bank_segments": len(segments),
        "paired_water_segments": paired,
        "paired_bed_subsegments": shared_bed_edges(products),
        "bank_preparation_cost": cost,
        "bank_index_prepared": args.method == "index",
        "input_read_cost": input_cost,
        "shore_preparation_cost": shore_cost,
        "instrumented": args.trace_memory,
        "construction": args.construction,
        "water_cells_retained": len(cells),
        "method": args.method,
        "scope": "Explicit finite-cohort shore/depth recipe; no globally complete bank stencil, external bed ports, terrain excavation, native codec or device admission. Original-face lineage is not recomputed child source CSR.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"candidate_cells": len(rows), "pending_cells": len(cells) - len(rows)}
        )
    )


if __name__ == "__main__":
    main()

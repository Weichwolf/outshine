"""Replay why uniquely source-owned lattice heights still need compatible levels."""

import argparse
import dataclasses
import json
import resource
import time
from collections import Counter
from pathlib import Path

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_water_grid_height import inputs
from compare_water_owner_inputs import layer_receipts
from water_nodal_height import (
    RECIPE,
    SUBDIVISIONS,
    audit_face,
    cell_faces,
    evaluate_node,
    parameter_triangle,
    sample_face,
)


def source_node_report(node):
    return {
        "key": node.key,
        "state": node.state,
        "world": None if node.world is None else node.world.tolist(),
        "level_m": node.level_m,
        "plan": dataclasses.asdict(node.plan),
        "original_course_rows": node.courses,
        "recipe_report": node.report,
    }


def replay_nodes(nodes, receipts, epoch, snapshots, data, order):
    """Fresh evaluations with only fixed inputs, reversed receipt arrival order."""
    maximum = 0.0
    repeated = 0
    for key, node in reversed(tuple(nodes.items())):
        fixed = {k: receipts[k] for k in reversed(node.plan.required) if k in receipts}
        other = evaluate_node(key, fixed, epoch, snapshots, data, order)
        if node.state != other.state or node.plan.version != other.plan.version:
            raise ValueError("consumer window changed node state or source version")
        if node.state == "ready":
            gap = float(np.linalg.norm(node.world - other.world))
            maximum = max(maximum, gap)
            if not np.array_equal(node.world, other.world):
                raise ValueError("consumer window changed canonical ECEF node")
            repeated += 1
    return {"fresh_ready_nodes": repeated, "maximum_world_node_difference_m": maximum}


def replay_facets(faces, nodes):
    count = 0
    for face in faces:
        keys = face["keys"]
        if any(nodes[key].world is None for key in keys):
            continue
        world = np.array([nodes[key].world for key in keys])
        sample = sample_face(
            keys, world, parameter_triangle(keys).mean(axis=0, keepdims=True)
        )
        if np.linalg.norm(sample[0] - world.mean(axis=0)) > 2e-9:
            raise ValueError("global affine facet replay differs at centroid")
        count += 1
    return count


def run(fixture, output):
    started = time.perf_counter()
    data, raw, _ = observed_snapshots(fixture)
    snapshots = {t: dataclasses.replace(s, complete=True) for t, s in raw.items()}
    epoch = tuple(
        next(iter(snapshots.values())).source[k] for k in ("provider", "revision")
    )
    receipts, order = layer_receipts(data, snapshots, epoch)
    _, records, _, origin = inputs(fixture)
    cells = sorted({r[0] for r in records})
    if len(cells) > 64:
        raise ValueError("diagnostic replay limited to 64 requested cells")
    mask = shapely.union_all([r[1].polygon for r in records])
    nodes, faces, arrays = {}, [], {}
    for cell in cells:
        for keys in cell_faces(cell):
            triangle = shapely.Polygon(parameter_triangle(keys))
            covered = triangle.intersection(mask)
            if covered.area <= 0:
                continue
            for key in keys:
                if key not in nodes:
                    nodes[key] = evaluate_node(
                        key, receipts, epoch, snapshots, data, order
                    )
            wet_point = covered.representative_point()
            result, payload = audit_face(
                keys, nodes, data, water_xy=(wet_point.x, wet_point.y)
            )
            index = len(faces)
            faces.append(
                dict(
                    result,
                    keys=keys,
                    cell=cell,
                    covered_area_tile_units2=float(covered.area),
                )
            )
            arrays.update(
                {f"face_{index}_{name}": value for name, value in payload.items()}
            )
    replay = replay_nodes(nodes, receipts, epoch, snapshots, data, order)
    sample_count = replay_facets(faces, nodes)
    arrays["mask_wkb"] = np.frombuffer(shapely.to_wkb(mask), dtype=np.uint8)
    arrays["source_points_lat_lon"] = data["points_lat_lon"]
    arrays["source_levels"] = data["levels"]
    arrays["source_courses"] = data["courses"]
    arrays["source_inputs_json"] = np.frombuffer(
        json.dumps(data["inputs"]).encode(), dtype=np.uint8
    )
    report = {
        "recipe": RECIPE,
        "subdivisions_per_source_tile": SUBDIVISIONS,
        "origin": origin,
        "node_states": dict(Counter(n.state for n in nodes.values())),
        "face_states": dict(Counter(f["state"] for f in faces)),
        "window_replay": replay,
        "affine_centroid_checks": sample_count,
        "faces": faces,
        "nodes": [source_node_report(n) for n in nodes.values()],
        "work": {
            "distinct_nodes": len(nodes),
            "water_intersecting_faces": len(faces),
            "receipt_lookups": sum(
                n.plan.work["receipt_lookups"] for n in nodes.values()
            ),
            "examined_original_courses": sum(
                n.report.get("examined_original_courses", 0) for n in nodes.values()
            ),
            "examined_raw_polygon_parts": sum(
                n.report.get("examined_raw_polygon_parts", 0) for n in nodes.values()
            ),
            "examined_raw_polygon_coordinates": sum(
                n.report.get("examined_raw_polygon_coordinates", 0)
                for n in nodes.values()
            ),
            "counter_scope": "first construction only; fresh node replay repeats source/target work",
            "total_receipt_lookups_including_replay": 2
            * sum(n.plan.work["receipt_lookups"] for n in nodes.values()),
            "stored_numeric_array_bytes": sum(a.nbytes for a in arrays.values()),
            "elapsed_seconds_including_fresh_replay": time.perf_counter() - started,
            "process_peak_rss_kib_linux": resource.getrusage(
                resource.RUSAGE_SELF
            ).ru_maxrss,
            "allocation_count": None,
            "copied_bytes": None,
            "workspace_scope": "whole-process RSS only, no allocator/copy accounting or bounded GEOS-workspace claim",
        },
        "scope": [
            "negative physical-admission experiment; no complete canonical water product",
            "all source inputs already decoded; no database, IO, native or frame claim",
            "finite delivered fixture completeness only; original source keys retained in input manifest",
            "target recipe uses raw ground-river masks in each fixed nine-source stencil, including overhang",
            "mask selects requested coverage only; no contour-derived mesh or additional QP",
            "rejected/pending facets are not published as water; counts include diagnostic candidates",
            "ECEF P1 position has Earth-curvature chord sag; no geodetic ASL interpolation or water-bed claim",
        ],
    }
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output / "candidate.npz", **arrays)
    plot(mask, nodes, faces, output)
    report["work"]["elapsed_seconds_including_exports_and_plot"] = (
        time.perf_counter() - started
    )
    report["work"]["final_process_peak_rss_kib_linux"] = resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_maxrss
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return {
        k: report[k] for k in ("node_states", "face_states", "window_replay", "work")
    }


def plot(mask, nodes, faces, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    fig, axes = plt.subplots(1, 2, figsize=(14, 7), layout="constrained")
    offset = np.asarray(mask.bounds[:2])
    palette = {
        "pending_nodes": "#b6bac2",
        "physical_rejection": "#c13636",
        "admitted": "#79b6a5",
    }
    for axis in axes:
        axis.add_collection(
            PolyCollection(
                [parameter_triangle(f["keys"]) - offset for f in faces],
                facecolors=[palette[f["state"]] for f in faces],
                edgecolors="#545862",
                linewidths=0.6,
                alpha=0.65,
            )
        )
        for polygon in shapely.get_parts(mask):
            for ring in (polygon.exterior, *polygon.interiors):
                xy = np.asarray(ring.coords) - offset
                axis.plot(*xy.T, color="#163a58", linewidth=0.9)
        axis.autoscale()
        axis.set(
            aspect="equal",
            xlabel="Mercator tile x (offset)",
            ylabel="Mercator tile y (offset)",
        )
        axis.invert_yaxis()
    valid = [n for n in nodes.values() if n.state == "ready"]
    xy = np.asarray([n.key[1:] for n in valid]) / SUBDIVISIONS - offset
    points = axes[1].scatter(
        *xy.T, c=[n.level_m for n in valid], cmap="viridis", s=48, zorder=5
    )
    for node, point in zip(valid, xy, strict=True):
        axes[1].annotate(
            f"{node.level_m:.2f}",
            point,
            xytext=(3, 3),
            textcoords="offset points",
            fontsize=7,
        )
    fig.colorbar(points, ax=axes[1], label="Soft source level [m ASL, unverified]")
    axes[0].set_title(
        "Diagnostic candidates: green admitted, red rejected, grey pending"
    )
    axes[1].set_title("Fixed source-owned node targets; no height QP")
    fig.savefig(output / "nodal-candidate.png", dpi=140)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.fixture, args.output), indent=2))


if __name__ == "__main__":
    main()

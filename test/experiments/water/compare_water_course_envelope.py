"""Automatic finite-snapshot parent heights with metadata before payload work."""

import argparse
import dataclasses
import json
import resource
import time
from collections import Counter
from pathlib import Path

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT
from compare_water_colored_updates import evaluate_faces, plot
from compare_water_grid_height import inputs
from outshine_ref.places.camera import ecef
from outshine_ref.places.sources import tile_geography
from physical_boundary import from_world
from water_colored_reference import ReferenceUpdates
from water_course_envelope import block_minimum
from water_envelope_fixture import CourseParent, acquire, prepare_parent
from water_nodal_height import audit_face, cell_faces, parameter_triangle


def query_point(key):
    lat, lon = tile_geography([[key[1] / 2, key[2] / 2]], 1, (key[0], 0, 0))
    return ecef(lat[0], lon[0], 0.0)


def parent_faces(recipe, parent, records):
    mask = shapely.union_all([r[1].polygon for r in records])
    faces, arrays = [], {}
    for cell in sorted({r[0] for r in records}):
        for keys in cell_faces(cell):
            covered = shapely.Polygon(parameter_triangle(keys)).intersection(mask)
            if covered.area <= 0:
                continue
            original = {p: recipe.target(p) for p in keys}
            if any(n.state != "ready" for n in original.values()):
                faces.append({"keys": keys, "state": "pending_dependencies"})
                continue
            nodes = {p: parent(p) for p in keys}
            if any(n.state != "ready" for n in nodes.values()):
                faces.append({"keys": keys, "state": "pending_dependencies"})
                continue
            wet = covered.representative_point()
            before, _ = audit_face(keys, original, recipe.data, water_xy=(wet.x, wet.y))
            updated = {
                p: dataclasses.replace(original[p], world=nodes[p].world) for p in keys
            }
            after, payload = audit_face(
                keys, updated, recipe.data, water_xy=(wet.x, wet.y)
            )
            faces.append(
                {
                    "keys": keys,
                    "state": after["state"],
                    "direct_targets": before,
                    "updated": after,
                }
            )
            arrays.update({f"face_{len(faces) - 1}_{k}": v for k, v in payload.items()})
    return mask, faces, arrays


def check_queries(parent, keys):
    tested, exhaustive, opened, excluded = 0, 0, 0, 0
    for key in keys:
        query = query_point(key)
        indexed = parent.queries[key]
        if indexed.state != "ready":
            continue
        values = []
        for block in parent.blocks.values():
            if not len(block.segments):
                continue
            height, identity, parameter = block_minimum(
                block, query, parent.index.slope
            )
            values.append((height, (block.key, identity), parameter))
            exhaustive += len(block.segments)
        if min(values) != (indexed.height_m, indexed.winner, indexed.parameter):
            raise ValueError(
                "metadata query disagrees with all-source primitive oracle"
            )
        window = {k: parent.blocks[k] for k in reversed(indexed.opened)}
        repeated = parent.index.query(query, window.get)
        if repeated.state != "ready" or (
            repeated.height_m,
            repeated.winner,
            repeated.parameter,
        ) != (indexed.height_m, indexed.winner, indexed.parameter):
            raise ValueError("residence window changed a completed envelope query")
        tested += indexed.reason["tested_segments"]
        opened += len(indexed.opened)
        excluded += len(indexed.excluded)
    return {
        "queries": len(keys),
        "indexed_tested_segments": tested,
        "exhaustive_tested_segments": exhaustive,
        "opened_block_requests": opened,
        "excluded_nonempty_block_requests": excluded,
        "maximum_oracle_or_window_difference_m": 0.0,
    }


def run(fixture, output):
    began = time.perf_counter()
    data, snapshots, epoch, receipts, order = acquire(fixture)
    parent, provenance = prepare_parent(data, snapshots, receipts)
    prepared = time.perf_counter()
    recipe = ReferenceUpdates(
        receipts,
        epoch,
        snapshots,
        data,
        order,
        parent=parent,
        parent_version=parent.version,
    )
    _, records, _, origin = inputs(fixture)
    mask, faces, arrays = parent_faces(recipe, parent, records)
    initial_keys = tuple(parent.queries)
    initial_queries = check_queries(parent, initial_keys)
    _, _, corrected, corrected_arrays = evaluate_faces(recipe, records)
    all_queries = check_queries(parent, tuple(parent.queries))
    reverse = CourseParent(parent.index, dict(reversed(tuple(parent.blocks.items()))))
    for key in reversed(tuple(parent.nodes)):
        other = reverse(key)
        if other.state != parent.nodes[key].state or not np.array_equal(
            other.world, parent.nodes[key].world
        ):
            raise ValueError("reversed source/window/query order changed the parent")
    report = {
        "origin": origin,
        "parent_version": parent.version,
        "model_slope_m_per_m": parent.index.slope,
        "parent_face_states": dict(Counter(f["state"] for f in faces)),
        "corrected_face_states": dict(Counter(f["state"] for f in corrected)),
        "initial_parent_queries": initial_queries,
        "all_parent_queries": all_queries,
        "source_provenance": provenance,
        "parent_faces": faces,
        "corrected_faces": corrected,
        "queries": [
            {"key": key, **dataclasses.asdict(q)} for key, q in parent.queries.items()
        ],
        "summaries": [dataclasses.asdict(s) for s in parent.index.summaries.values()],
        "work": {
            "acquisition_and_parent_preparation_seconds": prepared - began,
            "declared_source_cells": len(parent.blocks),
            "nonempty_source_blocks": sum(
                bool(len(b.segments)) for b in parent.blocks.values()
            ),
            "source_segments": sum(len(b.segments) for b in parent.blocks.values()),
            "all_coarse_numeric_payload_bytes": sum(
                b.segments.nbytes + b.levels.nbytes for b in parent.blocks.values()
            ),
            "all_coarse_identity_utf8_bytes": sum(
                len(i.encode()) for b in parent.blocks.values() for i in b.ids
            ),
            "unique_requested_payload_keys": len(set(parent.loads)),
            "logical_payload_requests": len(parent.loads),
        },
        "scope": [
            "closed finite declared source snapshot, not all Earth/OSM/source acquisition completeness",
            "raw courses and all coarse blocks decoded at preparation; runtime loader requests are logical, no measured DB/SSD saving",
            "slope0.005 is a conservative parent recipe; physical admission still uses unchanged +/-0.01 rows and source prisms",
            "source levels are unverified soft targets; a scalar guide does not assert hydraulic connection or flow",
            "outward Float64 model bounds; stored coordinates define the model, not exact Earth coordinates",
            "global coarse summaries/hierarchy, lake/coast levels, bed/fine/native/device contracts remain open",
        ],
    }
    output.mkdir(parents=True, exist_ok=True)
    arrays["mask_wkb"] = np.frombuffer(shapely.to_wkb(mask), dtype=np.uint8)
    arrays.update({f"corrected_{k}": v for k, v in corrected_arrays.items()})
    np.savez_compressed(output / "parent.npz", **arrays)
    plot(mask, faces, output, after_title="Source-course parent; no fixed water level")
    plot_geometry(mask, faces, recipe, parent, output)
    report["work"].update(
        elapsed_seconds_including_oracles_replays_export_plot=time.perf_counter()
        - began,
        process_peak_rss_kib_linux=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return {
        k: report[k]
        for k in (
            "parent_face_states",
            "corrected_face_states",
            "initial_parent_queries",
            "work",
        )
    }


def plot_geometry(mask, faces, recipe, parent, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    centroid = mask.centroid
    z = faces[0]["keys"][0][0]
    lat, lon = tile_geography([[centroid.x, centroid.y]], 1, (z, 0, 0))
    anchor = np.array([lon[0], lat[0]])
    geometry = [[], []]
    for face in faces:
        if face["state"] != "admitted":
            continue
        keys = face["keys"]
        for index, world in enumerate(
            (
                np.array([recipe.target(k).world for k in keys]),
                np.array([parent(k).world for k in keys]),
            )
        ):
            geometry[index].append(from_world(anchor, world))
    if not geometry[0]:
        return
    fig = plt.figure(figsize=(14, 6), layout="constrained")
    all_points = np.concatenate(geometry[0] + geometry[1])
    low, high = all_points.min(axis=0), all_points.max(axis=0)
    span = high - low
    for index in range(2):
        axis = fig.add_subplot(1, 2, index + 1, projection="3d")
        axis.add_collection3d(
            Poly3DCollection(
                geometry[index],
                facecolors="#3d88b0",
                edgecolors="#185071",
                linewidths=0.1,
            )
        )
        axis.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2] - 1, high[2] + 1),
            xlabel="East [m]",
            ylabel="North [m]",
            zlabel="ENU Up [m]",
        )
        axis.set_box_aspect((span[0], span[1], 20 * (span[2] + 2)))
        axis.view_init(25, -60)
    fig.axes[0].set_title("Direct nodal support facets; mask separate")
    fig.axes[1].set_title("Parent support facets: vertical exaggeration x20")
    fig.savefig(output / "parent-geometry.png", dpi=140)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.fixture, args.output), indent=2))


if __name__ == "__main__":
    main()

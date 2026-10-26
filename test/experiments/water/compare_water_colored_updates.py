"""Finite local updates with an explicit laboratory parent, not global water."""

import argparse
import dataclasses
import json
import resource
import time
import tracemalloc
from collections import Counter
from pathlib import Path

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_water_grid_height import inputs
from compare_water_owner_inputs import layer_receipts
from water_colored_reference import ReferenceUpdates
from water_nodal_height import audit_face, cell_faces, parameter_triangle


def make_recipe(fixture, parent_level_m):
    data, raw, _ = observed_snapshots(fixture)
    snapshots = {
        key: dataclasses.replace(value, complete=True) for key, value in raw.items()
    }
    epoch = tuple(next(iter(raw.values())).source[k] for k in ("provider", "revision"))
    receipts, order = layer_receipts(data, snapshots, epoch)
    recipe = ReferenceUpdates(
        receipts,
        epoch,
        snapshots,
        data,
        order,
        parent_level_m=parent_level_m,
        parent_version="explicit-laboratory-constant-ASL-v1",
    )
    return recipe, raw


def evaluate_faces(recipe, records):
    mask = shapely.union_all([r[1].polygon for r in records])
    cells = sorted({r[0] for r in records})
    if len(cells) > 64:
        raise ValueError("diagnostic replay limited to 64 requested cells")
    faces, nodes, arrays = [], {}, {}
    for cell in cells:
        for keys in cell_faces(cell):
            covered = shapely.Polygon(parameter_triangle(keys)).intersection(mask)
            if covered.area <= 0:
                continue
            for key in keys:
                if key not in nodes:
                    nodes[key] = recipe.resolve(key)
            if any(nodes[key].state != "ready" for key in keys):
                faces.append({"keys": keys, "state": "pending_dependencies"})
                continue
            wet = covered.representative_point()
            original = {key: recipe.target(key) for key in keys}
            updated = {
                key: dataclasses.replace(original[key], world=nodes[key].world)
                for key in keys
            }
            before, _ = audit_face(keys, original, recipe.data, water_xy=(wet.x, wet.y))
            after, payload = audit_face(
                keys, updated, recipe.data, water_xy=(wet.x, wet.y)
            )
            if after["state"] != "admitted":
                raise ValueError(
                    "completed update failed independent original-water face audit"
                )
            faces.append(
                {
                    "keys": keys,
                    "state": "admitted",
                    "direct_targets": before,
                    "updated": after,
                }
            )
            index = len(faces) - 1
            arrays.update(
                {f"face_{index}_{name}": value for name, value in payload.items()}
            )
            arrays[f"face_{index}_target_world"] = np.array(
                [original[k].world for k in keys]
            )
    return mask, nodes, faces, arrays


def replay(recipe, nodes):
    other = ReferenceUpdates(
        recipe.receipts,
        recipe.epoch,
        recipe.snapshots,
        recipe.data,
        recipe.order,
        parent_level_m=recipe.parent_level_m,
        parent_version=recipe.parent_recipe_version,
    )
    for key, original in reversed(tuple(nodes.items())):
        repeated = other.resolve(key)
        if (
            repeated.state != original.state
            or recipe.plans[key]["version"] != other.plans[key]["version"]
        ):
            raise ValueError("query order changed state or input version")
        if original.state == "ready" and not np.array_equal(
            original.world, repeated.world
        ):
            raise ValueError("query order changed completed world position")
    windows = 0
    for key, original in nodes.items():
        if original.state != "ready":
            continue
        required = recipe.plans[key]["required"]
        receipts = {k: recipe.receipts[k] for k in reversed(required)}
        tiles = {k[0] for k in required}
        local = ReferenceUpdates(
            receipts,
            recipe.epoch,
            {t: recipe.snapshots[t] for t in tiles},
            recipe.data,
            {t: recipe.order[t] for t in tiles},
            parent_level_m=recipe.parent_level_m,
            parent_version=recipe.parent_recipe_version,
        )
        repeated = local.resolve(key)
        if repeated.state != "ready" or not np.array_equal(
            original.world, repeated.world
        ):
            raise ValueError("declared source window changed completed world position")
        windows += 1
    return {
        "node_queries": len(nodes),
        "declared_source_window_replays": windows,
        "completed_world_difference_m": 0.0,
    }


def run(fixture, output, parent_level_m):
    began = time.perf_counter()
    recipe, _ = make_recipe(fixture, parent_level_m)
    _, records, _, origin = inputs(fixture)
    tracemalloc.start()
    mask, nodes, faces, arrays = evaluate_faces(recipe, records)
    traced_current, traced_peak = tracemalloc.get_traced_memory()
    traced_blocks = sum(
        s.count for s in tracemalloc.take_snapshot().statistics("filename")
    )
    tracemalloc.stop()
    reverse = replay(recipe, nodes)
    errors = []
    for key, node in nodes.items():
        if node.state == "ready":
            target = recipe.target(key).world
            errors.append(
                [
                    float(np.linalg.norm(recipe.parent(key) - target)),
                    float(np.linalg.norm(node.world - target)),
                ]
            )
    report = {
        "origin": origin,
        "parent": {
            "level_m": parent_level_m,
            "version": recipe.parent_version,
            "scope": "explicit caller-supplied laboratory datum, not a globally acquired water parent",
        },
        "node_states": dict(Counter(n.state for n in nodes.values())),
        "face_states": dict(Counter(f["state"] for f in faces)),
        "reverse_query_replay": reverse,
        "completed_node_errors_m": errors,
        "faces": faces,
        "plans": [{"key": key, **plan} for key, plan in recipe.plans.items()],
        "updates": [
            {
                "key": key,
                "state": n.state,
                "world": None if n.world is None else n.world.tolist(),
                "denominator": n.denominator,
                "reason": n.reason,
            }
            for key, n in nodes.items()
        ],
        "work": {
            **recipe.work,
            **recipe.field.work,
            "target_profiles": len(recipe.targets),
            "distinct_source_faces": len(recipe.faces),
            "construction_python_traced_live_bytes": traced_current,
            "construction_python_traced_peak_bytes": traced_peak,
            "construction_python_traced_live_allocations": traced_blocks,
            "instrumentation_scope": "tracemalloc only during first construction; no total allocation-event or GEOS allocator accounting",
            "counter_scope": "first construction only; independent reverse-query and declared-window replays excluded from counters, included in time",
            "examined_original_courses": sum(
                n.report.get("examined_original_courses", 0)
                for n in recipe.targets.values()
            ),
            "examined_raw_polygon_coordinates": sum(
                n.report.get("examined_raw_polygon_coordinates", 0)
                for n in recipe.targets.values()
            ),
        },
        "source_target_nodes": [
            {
                "key": key,
                "state": n.state,
                "input_version": None if n.plan is None else n.plan.version,
                "original_course_rows": n.courses,
                "source_level_m": n.level_m,
            }
            for key, n in recipe.targets.items()
        ],
        "scope": [
            "one fixed colour sweep; no QP or convergence claim",
            "all original input already decoded; no IO, native, GPU or speed-win claim",
            "source/physical boxes and grade limits unchanged",
            "source dependencies are preflighted before target cooking; incomplete results stay pending",
            "parent ownership/quality and Earth-wide lake/coast/bed contracts remain open",
        ],
    }
    arrays["mask_wkb"] = np.frombuffer(shapely.to_wkb(mask), dtype=np.uint8)
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output / "updated.npz", **arrays)
    plot(mask, faces, output)
    report["work"].update(
        elapsed_seconds_including_replay_export_plot=time.perf_counter() - began,
        process_peak_rss_kib_linux=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        retained_numeric_proof_bytes=sum(a.nbytes for a in arrays.values()),
        allocation_count=None,
        copied_bytes=None,
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return {
        key: report[key]
        for key in ("node_states", "face_states", "reverse_query_replay", "work")
    }


def plot(
    mask,
    faces,
    output,
    *,
    after_title="One fixed colour sweep from explicit lab parent",
):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    selected = [f for f in faces if f["state"] == "admitted"]
    fig, axes = plt.subplots(1, 2, figsize=(12, 6), layout="constrained")
    offset = np.asarray(mask.bounds[:2])
    palette = {
        "physical_rejection": "#c13636",
        "admitted": "#79b6a5",
        "pending_dependencies": "#b6bac2",
    }
    for axis, before in zip(axes, (True, False), strict=True):
        axis.add_collection(
            PolyCollection(
                [parameter_triangle(f["keys"]) - offset for f in faces],
                facecolors=[
                    palette[
                        (f["direct_targets"]["state"] if before else "admitted")
                        if f["state"] == "admitted"
                        else f["state"]
                    ]
                    for f in faces
                ],
                edgecolors="#545862",
                linewidths=0.5,
                alpha=0.65,
            )
        )
        for polygon in shapely.get_parts(mask):
            for ring in (polygon.exterior, *polygon.interiors):
                axis.plot(
                    *(np.asarray(ring.coords) - offset).T,
                    color="#163a58",
                    linewidth=0.9,
                )
        box = (
            shapely.union_all(
                [shapely.Polygon(parameter_triangle(f["keys"])) for f in selected]
            ).bounds
            if selected
            else mask.bounds
        )
        axis.set(
            xlim=(box[0] - offset[0] - 0.1, box[2] - offset[0] + 0.1),
            ylim=(box[3] - offset[1] + 0.1, box[1] - offset[1] - 0.1),
            aspect="equal",
            xlabel="Mercator tile x (offset)",
            ylabel="Mercator tile y (offset)",
        )
    axes[0].set_title("Direct source targets on the completed subset")
    axes[1].set_title(after_title)
    fig.savefig(output / "updated.png", dpi=140)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--parent-level-m", type=float, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.fixture, args.output, args.parent_level_m), indent=2))


if __name__ == "__main__":
    main()

"""Build a complete angular plant product from cached parent-profile wood poses."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from conifer_whole_products import load_catalog
from conifer_window_render import tree_bounds, tree_image
from conifer_windows import bind_windows
from mature_density import MAX_AXES, MAX_WINDOWS, WINDOW_SETTINGS
from outshine_ref.vegetation import AxisState, Needle, NeedledPlant, NeedlePattern
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.needle_windows import _axis_window_values, capacities
from parent_profile_graph import ParentProfileGraph
from plant_atlas import bake_product


def read_execution(directory, recipe):
    report = json.loads((directory / "execution.json").read_text())
    if report["source_modified"] or not 0 < report["axes"] <= MAX_AXES:
        raise ValueError("verified bounded frozen wood execution required")
    if (
        hashlib.sha256(recipe.read_bytes()).hexdigest()
        != report["source_recipe_sha256"]
    ):
        raise ValueError("physical input must match the executed wood input")
    graph = ParentProfileGraph.from_recipe(
        json.loads((directory / "graph.json").read_text())
    )
    if graph.identity != report["graph_identity"]:
        raise ValueError("cached wood identity changed")
    with np.load(directory / "axes.npz", allow_pickle=False) as saved:
        axes = {name: saved[name] for name in saved.files}
    offsets = axes["path_offsets"]
    orders = np.diff(offsets).astype(np.int64) - 2
    if len(orders) != report["axes"] or np.any((orders < 0) | (orders > 3)):
        raise ValueError("four complete cached wood orders required")
    expected = np.r_[
        np.uint64(0), np.cumsum(axes["needle_counts"], dtype=np.uint64)[:-1]
    ]
    np.testing.assert_array_equal(axes["first_needle_ids"], expected)
    return report, graph, axes, orders


def window_plan(plant, axes, orders, expected_windows):
    groups = capacities(plant, **WINDOW_SETTINGS)
    active = np.flatnonzero(axes["needle_counts"])
    capacity = np.array(
        [
            0 if pattern is None else group * pattern.group_size
            for group, pattern in zip(groups, plant.patterns, strict=True)
        ],
        dtype=np.uint64,
    )[np.asarray(orders)[active]]
    if len(orders) > MAX_AXES or np.any(capacity == 0):
        raise ValueError("bounded physical axes and occupied capacities required")
    counts = axes["needle_counts"][active].astype(np.uint64)
    needed = int(((counts + capacity - 1) // capacity).sum())
    if needed != expected_windows or not 0 < needed <= MAX_WINDOWS:
        raise ValueError("bounded complete window ledger required before allocation")
    columns = {}
    for index in active:
        order = int(orders[index])
        count = int(axes["needle_counts"][index])
        pattern = plant.patterns[order]
        if pattern is None or count != pattern.count(float(axes["lengths"][index])):
            raise ValueError("physical count must match the cached actual axis")
        lo, hi = axes["path_offsets"][index : index + 2]
        path = tuple(int(slot) for slot in axes["path_pairs"][int(lo) + 2 : int(hi), 1])
        axis = AxisState(
            path,
            int(index),
            float(axes["lengths"][index]),
            float(axes["bends"][index]),
            axes["origins"][index],
            axes["rotations"][index],
            0,
            0,
            path_capacity=256,
        )
        values = _axis_window_values(
            axis,
            pattern,
            count,
            groups[order] * pattern.group_size,
            plant.wood.seed,
            int(axes["first_needle_ids"][index]),
        )
        for name, value in values.items():
            columns.setdefault(name, []).append(value)
    result = {name: np.concatenate(parts) for name, parts in columns.items()}
    if len(result["origins"]) != expected_windows or expected_windows > MAX_WINDOWS:
        raise ValueError("cached windows must match the verified bounded ledger")
    first, count = result["first_needle_ids"], result["needle_counts"]
    np.testing.assert_array_equal(first[1:], first[:-1] + count[:-1])
    assert int(first[0]) == 0
    assert int(first[-1]) + int(count[-1]) == int(axes["needle_counts"].sum())
    return result


def wood_mesh(graph, axes, orders):
    vertices, normals, faces, offset = [], [], [], 0
    for order in range(4):
        selected = np.flatnonzero(orders == order)
        shape = graph.by_id[order + 1].shape
        mesh = axis_mesh(
            shape.length,
            shape.bend,
            float(shape.radius.evaluate(0)),
            float(shape.radius.evaluate(1)),
            4 if order == 0 else 2,
            8,
        )
        frames = axes["rotations"][selected]
        points = np.einsum("vj,nij->nvi", mesh["vertices"], frames)
        points *= axes["uniform_scales"][selected, None, None]
        points += axes["origins"][selected, None]
        vertices.append(points.reshape(-1, 3))
        normals.append(np.einsum("vj,nij->nvi", mesh["normals"], frames).reshape(-1, 3))
        indices = np.arange(len(selected)) * len(mesh["vertices"]) + offset
        faces.append((mesh["faces"][None] + indices[:, None, None]).reshape(-1, 3))
        offset += len(selected) * len(mesh["vertices"])
    return {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
    }


def provenance():
    root = Path(__file__).resolve().parents[3]
    names = (
        "parent_window_product.py",
        "conifer_windows.py",
        "conifer_window_render.py",
        "conifer_whole_products.py",
        "plant_atlas.py",
        "textured_raster.py",
        "raster_batch.py",
        "leaf_sheet.py",
    )
    paths = [Path(__file__).with_name(name) for name in names]
    paths.append(root / "lib/src/outshine_ref/vegetation/needle_windows.py")
    return {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", *map(str, paths)],
                cwd=root,
                text=True,
            ).strip()
        ),
        "implementation_sha256": {
            str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in paths
        },
    }


def build(execution, recipe, catalog, output):
    began = time.perf_counter()
    report, graph, axes, orders = read_execution(execution, recipe)
    if not 0 < report["windows"] <= MAX_WINDOWS:
        raise ValueError("bounded executed windows required before catalog decode")
    plant = NeedledPlant.from_recipe(json.loads(recipe.read_text()))
    source = json.loads((catalog / "source.json").read_text())
    if source["recipe"]["needles"] != plant.recipe()["needles"]:
        raise ValueError("catalog must preserve the physical needle recipes")
    with (
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("organ detail")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("organ mesh")),
        patch.object(NeedledPlant, "foliage", side_effect=AssertionError("near plant")),
    ):
        windows = window_plan(plant, axes, orders, report["windows"])
        metadata, banks = load_catalog(catalog)
        windows = bind_windows(plant, windows, metadata)
        wood = wood_mesh(graph, axes, orders)
        output.mkdir(parents=True, exist_ok=True)
        np.savez_compressed(output / "windows.npz", **windows)
        complete = {
            "version": "experimental-parent-needled-1",
            "wood": graph.recipe(),
            "physical_source": plant.recipe(),
        }
        inputs = [
            recipe,
            *(
                execution / name
                for name in ("graph.json", "axes.npz", "execution.json")
            ),
        ]
        inputs += [catalog / "source.json", catalog / "catalog.json"]
        inputs += sorted(catalog.glob("family-*.npz"))
        descriptor = {
            "recipe": complete,
            "plan_identity": hashlib.sha256(
                json.dumps(complete, sort_keys=True).encode()
            ).hexdigest(),
            "input_sha256": {
                str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs
            },
            "bounds_points": tree_bounds(windows, banks, wood),
            "source_scope": "cached experimental parent-profile wood and original finite physical needle windows; complete angular product, unapproved statistical window/radius/botanical/LOD fidelity; constant unlit wood colour",
        }
        progress = 0

        def draw(direction, size):
            nonlocal progress
            result = tree_image(windows, banks, wood, direction, size, samples=1)
            progress += 1
            if progress % 8 == 0:
                print(f"complete views {progress}/64", flush=True)
            return result

        result = bake_product(descriptor, output, count=64, size=256, draw=draw)
    assert not graph._poses and not plant.wood._axes and not plant._groups
    result.update(
        **provenance(),
        elapsed_s=time.perf_counter() - began,
        cached_execution_commit=report["source_commit"],
        axes=report["axes"],
        windows=len(windows["origins"]),
        needles=int(windows["needle_counts"].sum()),
        wood_triangles=len(wood["faces"]),
        needle_generation_disabled=True,
        wood_pose_generation_disabled=True,
    )
    (output / "whole-build.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps({k: result[k] for k in ("axes", "windows", "needles", "elapsed_s")}),
        flush=True,
    )
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("execution", "recipe", "catalog", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    build(args.execution, args.recipe, args.catalog, args.output)

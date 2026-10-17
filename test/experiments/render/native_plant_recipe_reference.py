"""Compare demand-driven native recipe plans with the independent Python planner."""

import argparse
import copy
import json
import math
import subprocess
from pathlib import Path

import numpy as np
from native_plant_plan_reference import fixed_rows, records
from outshine_ref.vegetation import PlantPlan

ARRANGEMENTS = ("spiral", "distichous", "opposite", "decussate", "whorled")
LEVEL_FIELDS = (
    "branches",
    "length",
    "base_radius",
    "tip_radius",
    "tip_scale",
    "length_shape",
    "attachment_jitter",
    "azimuth_variation",
    "upward_bias",
    "first",
    "last",
    "angle",
    "angle_variation",
    "bend",
    "bend_variation",
    "leaf_internode",
    "leaf_first",
    "leaf_last",
    "leaf_angle",
    "leaf_sky_alignment",
    "arrangement",
    "whorl_size",
    "angle_tip",
)
BLADE_FIELDS = (
    "length",
    "width",
    "widest",
    "lobes",
    "lobe_depth",
    "base_fill",
    "bend",
    "fold",
    "fold_power",
    "twist",
    "petiole",
)


def encode(recipe, path, axes, leaves):
    lines = [f"{recipe['seed']} {len(recipe['levels'])} {len(recipe['blades'])}"]
    for level in recipe["levels"]:
        values = []
        for name in LEVEL_FIELDS:
            value = level.get(name, 1 if name == "whorl_size" else None)
            if name == "length_shape":
                values.append(("taper", "rounded").index(value))
            elif name == "arrangement":
                values.append(ARRANGEMENTS.index(value))
            elif name in ("leaf_internode", "angle_tip"):
                values.extend((int(value is not None), 0 if value is None else value))
            else:
                values.append(value)
        lines.append(" ".join(map(str, values)))
    for blade in recipe["blades"]:
        lines.append(" ".join(str(blade[name]) for name in BLADE_FIELDS))
    lines.append(" ".join(map(str, (*path, *([-1] * (7 - len(path))), axes, leaves))))
    return "\n".join(lines) + "\n"


def reference(recipe, path):
    tables = PlantPlan.from_recipe(recipe).tables(100_000, 100_000)
    rows = {
        key: value.reshape((len(value), math.prod(value.shape[1:]))).tolist()
        for key, value in tables.items()
    }
    axes = records(
        rows,
        "axis_",
        (
            "ids",
            "parent_ids",
            "paths",
            "origins",
            "frames",
            "lengths",
            "bends",
            "parent_parameters",
            "radii",
        ),
    )
    leaves = records(
        rows,
        "leaf_",
        (
            "ids",
            "axis_ids",
            "variants",
            "origins",
            "nodes",
            "frames",
            "stem_parameter",
        ),
    )
    paths = {axis[0]: tuple(v for v in axis[2:9] if v >= 0) for axis in axes}
    selected_axes = [
        axis
        for axis in axes
        if paths[axis[0]][: len(path)] == path
        or path[: len(paths[axis[0]])] == paths[axis[0]]
    ]
    selected_leaves = [leaf for leaf in leaves if paths[leaf[1]][: len(path)] == path]
    root = next(key for key, value in paths.items() if value == path)
    return root, selected_axes, selected_leaves


def compare(actual, expected, integers):
    if len(actual) != len(expected):
        raise ValueError("native recipe changed the selected row count")
    for native, source in zip(actual, expected, strict=True):
        if native[:integers] != source[:integers]:
            raise ValueError(
                "native recipe changed source identity, ownership, path or variant"
            )
    left = np.array([row[integers:] for row in actual])
    right = np.array([row[integers:] for row in expected])
    np.testing.assert_allclose(left, right, rtol=0, atol=2e-14)
    return np.abs(left - right) if len(expected) else np.empty((0, 0))


def maximum(errors, start, end):
    return float(np.max(errors[:, start:end])) if errors.size else 0.0


def check(binary, recipe, path, output, name):
    root, axes, leaves = reference(recipe, path)
    result = subprocess.run(
        [str(binary)],
        input=encode(recipe, path, len(axes), len(leaves)),
        capture_output=True,
        text=True,
        timeout=10,
        check=True,
    )
    (output / f"{name}.log").write_text(result.stdout + result.stderr)
    actual = {"A": [], "L": [], "M": []}
    for line in result.stdout.splitlines():
        kind, *fields = line.split()
        integers = {"A": 9, "L": 3, "M": 3}[kind]
        actual[kind].append(
            [
                int(value) if at < integers else float(value)
                for at, value in enumerate(fields)
            ]
        )
    if actual["M"] != [[root, len(axes), len(leaves)]]:
        raise ValueError("native recipe preflight changed the selected root or counts")
    axis_error = compare(actual["A"], axes, 9)
    leaf_error = compare(actual["L"], leaves, 3)
    return {
        "name": name,
        "path": path,
        "axes": len(axes),
        "leaves": len(leaves),
        "position_error_m": max(maximum(axis_error, 0, 3), maximum(leaf_error, 0, 6)),
        "frame_error": max(maximum(axis_error, 3, 12), maximum(leaf_error, 6, 15)),
        "axis_length_error_m": maximum(axis_error, 12, 13),
        "axis_bend_error_rad": maximum(axis_error, 13, 14),
        "attachment_parameter_error": max(
            maximum(axis_error, 14, 15), maximum(leaf_error, 15, 16)
        ),
        "axis_radius_error_m": maximum(axis_error, 15, 17),
    }


def cases():
    metadata, _ = fixed_rows()
    base = metadata["recipe"]
    for at, path in enumerate(((), (0,), (2,), (8, 5))):
        yield f"fixed-{at}", base, path
    for at, arrangement in enumerate(ARRANGEMENTS):
        for shape in ("taper", "rounded"):
            recipe = copy.deepcopy(base)
            recipe["version"] = "branching-plant-3"
            recipe["seed"] = (17, 2**32, 2**63, 2**64 - 1, 0)[at]
            for level in recipe["levels"]:
                level.update(
                    arrangement=arrangement,
                    length_shape=shape,
                    whorl_size=3,
                    attachment_jitter=0.2,
                    upward_bias=0.15,
                    angle_tip=0.8,
                )
            for index, path in enumerate(((), (1,), (8, 5))):
                yield f"{arrangement}-{shape}-{index}", recipe, path
    bare = copy.deepcopy(base)
    bare["version"] = "branching-plant-3"
    bare["blades"] = []
    for level in bare["levels"]:
        level.update(leaf_internode=None, whorl_size=1, angle_tip=None)
    yield "bare", bare, ()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    results = [
        check(args.binary.resolve(), recipe, path, output, name)
        for name, recipe, path in cases()
    ]
    receipt = {
        "cases": results,
        "scope": "finite Float64 recipe cohort; no LOD error bound, mature-tree, world or device acceptance",
    }
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"cases": len(results), "scope": receipt["scope"]}))


if __name__ == "__main__":
    main()

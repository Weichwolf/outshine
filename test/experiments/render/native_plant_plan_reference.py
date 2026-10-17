"""Compare native immutable plant demand against the existing fixed canonical rows."""

import argparse
import ast
import hashlib
import json
import struct
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
REFERENCE = ROOT / "test/experiments/vegetation/reference/tree-entry-sapling-17.json"


def fixed_rows():
    metadata = json.loads(REFERENCE.read_text())
    path = REFERENCE.parent / metadata["tables"]
    if hashlib.sha256(path.read_bytes()).hexdigest() != metadata["tables_sha256"]:
        raise ValueError("fixed canonical input changed")
    rows = {}
    with zipfile.ZipFile(path) as archive:
        for name in archive.namelist():
            data = archive.read(name)
            if data[:8] != b"\x93NUMPY\x01\x00":
                raise ValueError("expected fixed NPY1 records")
            size = struct.unpack_from("<H", data, 8)[0]
            header = ast.literal_eval(data[10 : 10 + size].decode("ascii"))
            if header["fortran_order"]:
                raise ValueError("expected row-major canonical tables")
            element = {"<f8": "d", "<u8": "Q", "<i8": "q", "<i2": "h", "|u1": "B"}[
                header["descr"]
            ]
            values = [
                value[0]
                for value in struct.iter_unpack("<" + element, data[10 + size :])
            ]
            count = header["shape"][0]
            width = len(values) // count if count else 0
            rows[name.removesuffix(".npy")] = [
                values[i * width : (i + 1) * width] for i in range(count)
            ]
    return metadata, rows


def records(rows, prefix, names):
    return [
        [value for name in names for value in rows[prefix + name][i]]
        for i in range(len(rows[prefix + names[0]]))
    ]


def check_surfaces(actual, axes, detail):
    import numpy as np
    from outshine_ref.vegetation.axis_surface import axis_mesh

    position_error = normal_error = 0.0
    triangles = 0
    for axis in axes:
        selected = [row[1:] for row in actual if row[0] == axis[0]]
        source = axis_mesh(
            axis[21],
            axis[22],
            axis[24],
            axis[25],
            detail["axis_segments"],
            detail["axis_sides"],
        )
        frame = np.array(axis[12:21]).reshape(3, 3)
        origin = np.array(axis[9:12])
        positions = (source["vertices"] @ frame.T + origin)[source["faces"]]
        normals = (source["normals"] @ frame.T).reshape(-1, 3, 3)
        result = np.asarray(selected).reshape(-1, 3, 6)
        if result.shape[:2] != positions.shape[:2]:
            raise ValueError("native axis changed the canonical triangle inventory")
        position_error = max(
            position_error, float(np.max(np.abs(result[:, :, :3] - positions)))
        )
        normal_error = max(
            normal_error, float(np.max(np.abs(result[:, :, 3:] - normals)))
        )
        np.testing.assert_allclose(result[:, :, :3], positions, rtol=0, atol=2e-7)
        np.testing.assert_allclose(result[:, :, 3:], normals, rtol=0, atol=1e-7)
        triangles += len(result)
    if len(actual) != triangles:
        raise ValueError("unexpected axis surfaces")
    return {
        "triangles": triangles,
        "maximum_position_error_m": position_error,
        "maximum_normal_component_error": normal_error,
        "scope": "Float32 native corners versus existing Float64 Python surfaces; no runtime LOD/device claim",
    }


def check_blades(actual, counts, metadata):
    import numpy as np
    from outshine_ref.vegetation.blade_surface import Blade

    position_error = normal_error = uv_error = 0.0
    triangles = 0
    detail = metadata["geometry"]
    for variant, recipe in enumerate(metadata["recipe"]["blades"]):
        source = Blade(**recipe).mesh(detail["leaf_segments"], detail["leaf_across"])
        reference = source["vertices"][source["faces"]]
        normals = source["normals"].reshape(-1, 3, 3)
        parameters = source["parameters"][source["faces"]]
        uv = np.stack(((parameters[..., 1] + 1) / 2, parameters[..., 0]), axis=-1)
        result = np.asarray([row[1:] for row in actual if row[0] == variant]).reshape(
            -1, 3, 8
        )
        if result.shape[:2] != reference.shape[:2]:
            raise ValueError("native blade changed the canonical triangle inventory")
        position_error = max(
            position_error, float(np.max(np.abs(result[..., :3] - reference)))
        )
        normal_error = max(
            normal_error, float(np.max(np.abs(result[..., 3:6] - normals)))
        )
        uv_error = max(uv_error, float(np.max(np.abs(result[..., 6:] - uv))))
        np.testing.assert_allclose(result[..., :3], reference, rtol=0, atol=1e-8)
        np.testing.assert_allclose(result[..., 3:6], normals, rtol=0, atol=1e-7)
        np.testing.assert_allclose(result[..., 6:], uv, rtol=0, atol=3e-8)
        triangles += len(result)
    if len(actual) != triangles or len(counts) != len(metadata["recipe"]["blades"]):
        raise ValueError("unexpected blade surfaces")
    return {
        "triangles": triangles,
        "native_counts": counts,
        "maximum_position_error_m": position_error,
        "maximum_normal_component_error": normal_error,
        "maximum_uv_error": uv_error,
        "scope": "eight shared native Float32 blade prototypes versus existing Python surfaces",
    }


def check(binary, output, surfaces=False, blades=False):
    metadata, rows = fixed_rows()
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
        ("ids", "axis_ids", "variants", "origins", "nodes", "frames", "stem_parameter"),
    )
    text = "\n".join(" ".join(map(str, row)) for row in axes + leaves) + "\n"
    if blades:
        names = (
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
        text += (
            "\n".join(
                " ".join(str(blade[name]) for name in names)
                for blade in metadata["recipe"]["blades"]
            )
            + "\n"
        )
    mode = ["plant-surfaces"] if blades else (["axis-surfaces"] if surfaces else [])
    result = subprocess.run(
        [str(binary), *mode],
        input=text,
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )
    (output / "native.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f"native plan consumer exited {result.returncode}")
    actual = {kind: [] for kind in ("A", "L", "S", "D", "G", "B", "C")}
    for line in result.stdout.splitlines():
        kind, *fields = line.split()
        integers = {"A": 9, "L": 3, "S": 11, "D": len(fields), "G": 1, "B": 1, "C": 3}[
            kind
        ]
        actual[kind].append(
            [
                int(value) if i < integers else float(value)
                for i, value in enumerate(fields)
            ]
        )
    if actual["A"] != axes or actual["L"] != leaves:
        raise ValueError(
            "native detail changed canonical IDs, order, ownership or physical rows"
        )
    paths = {axis[0]: tuple(slot for slot in axis[2:9] if slot >= 0) for axis in axes}
    summaries = []
    branches = []
    for axis in axes:
        path = paths[axis[0]]
        descendants = {
            identity
            for identity, candidate in paths.items()
            if candidate[: len(path)] == path
        }
        branches.append([axis[0], *sorted(descendants, key=paths.__getitem__)])
        owned = [leaf for leaf in leaves if leaf[1] in descendants]
        variants = [sum(leaf[2] == variant for leaf in owned) for variant in range(8)]
        summaries.append([axis[0], len(descendants), len(owned), *variants])
    if actual["S"] != summaries:
        raise ValueError(
            "native branch summary differs from independent path-prefix inventory"
        )
    if actual["D"] != branches:
        raise ValueError(
            "native subtree rows differ from independent path-prefix traversal"
        )
    receipt = {
        "identity": metadata["plan_identity"],
        "axes": len(axes),
        "leaves": len(leaves),
        "subtrees": len(summaries),
        "canonical_rows_exact": True,
        "subtree_rows_exact": True,
        "scope": "fixed immutable-row consumption and demand; no generation, geometry or device claim",
    }
    if surfaces:
        receipt["scope"] = (
            "fixed prepared-row consumption plus circular-axis geometry; no recipe/world/device claim"
        )
        receipt["axis_surfaces"] = check_surfaces(
            actual["G"], axes, metadata["geometry"]
        )
    if blades:
        receipt["blade_surfaces"] = check_blades(actual["B"], actual["C"], metadata)
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--surfaces", action="store_true")
    parser.add_argument("--blades", action="store_true")
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(ROOT):
        parser.error(
            "private output must be outside the checkout in the system temporary directory"
        )
    output.mkdir(parents=True, exist_ok=True)
    check(args.binary.resolve(), output, args.surfaces or args.blades, args.blades)


if __name__ == "__main__":
    main()

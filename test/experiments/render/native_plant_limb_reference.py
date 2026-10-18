"""Replay original adult growth limbs through native C1 wood and Python sweeps."""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vegetation"))
from limb_paths import dominant_paths
from limb_surface import limb_mesh, path_recipe
from oak_scaffold import OakRecipe, tapered_radii
from oak_skin_experiment import seed_continuations


def load_source(folder, seed, authored_limbs):
    name = f"oak-{seed}-limbs-{int(authored_limbs)}"
    saved = folder / f"{name}.python.npz"
    original_input = folder / f"{name}.input"
    with np.load(saved) as data:
        points, parents = data["positions"], data["parents"]
    lines = original_input.read_text().splitlines()
    count = int(lines[0].split()[0])
    seed_rows = np.array([list(map(float, s.split())) for s in lines[1 : count + 1]])
    np.testing.assert_array_equal(points[:count], seed_rows[:, 1:])
    hints = seed_continuations(parents, seed_rows[:, 0].astype(np.int32))
    recipe = OakRecipe(seed=seed)
    radii = tapered_radii(parents, recipe.root_radius_m, recipe.terminal_radius_m)
    paths = dominant_paths(points, parents, radii, max_nodes=16384, continuations=hints)
    assert paths is not None
    return name, saved, points, radii, paths


def encode(points, radii, paths, along, sides):
    lines = [str(len(paths))]
    for path in paths:
        values = radii[np.array(path)].copy()
        if path[0] != 0:
            values[0] = values[1]
        values[-1] *= 0.35
        lines.append(f"{len(path)} {along} {sides}")
        for node, radius in zip(path, values, strict=True):
            row = [*points[node], radius]
            lines.append(f"{node} " + " ".join(format(v, ".17g") for v in row))
    return "\n".join(lines) + "\n"


def read_array(stream, dtype, count, shape):
    result = np.fromfile(stream, dtype=dtype, count=count)
    assert len(result) == count, "complete native arrays required"
    return result.reshape(shape)


def compare_arrays(stream, reference_output, points, radii, path, along, sides):
    stations, vertices, faces = map(int, read_array(stream, "<u8", 3, (3,)))
    samples = read_array(stream, "<f8", stations * 12, (stations, 12))
    positions = read_array(stream, "<f4", vertices * 3, (vertices, 3))
    normals = read_array(stream, "<f4", vertices * 3, (vertices, 3))
    indices = read_array(stream, "<u4", faces * 3, (faces, 3))
    spine, profile = path_recipe(points, radii, path)
    parameters = np.linspace(0, 1, stations)
    centre, tangent, speed, curvature = spine.sample(parameters)
    expected_samples = np.c_[
        centre, tangent, curvature, speed, profile(parameters), profile(parameters, 1)
    ]
    sample_errors = np.max(abs(samples - expected_samples), axis=0)
    np.testing.assert_allclose(samples, expected_samples, rtol=0, atol=1e-9)
    reference = limb_mesh(points, radii, path, along, sides)
    assert reference is not None and len(reference["faces"]) == faces
    assert vertices == len(reference["vertices"]) + 2 * sides
    corners = positions[indices].reshape((-1, 3))
    expected_corners = reference["vertices"][reference["faces"]].reshape((-1, 3))
    position_error = float(np.max(abs(corners - expected_corners)))
    normal_error = float(
        np.max(abs(normals[indices].reshape((-1, 3)) - reference["normals"]))
    )
    np.testing.assert_allclose(corners, expected_corners, rtol=0, atol=2e-6)
    np.testing.assert_allclose(
        normals[indices].reshape((-1, 3)), reference["normals"], rtol=0, atol=1e-6
    )
    np.array([stations, len(expected_corners), faces], dtype="<u8").tofile(
        reference_output
    )
    expected_samples.astype("<f8").tofile(reference_output)
    expected_corners.astype("<f4").tofile(reference_output)
    reference["normals"].astype("<f4").tofile(reference_output)
    np.arange(faces * 3, dtype="<u4").tofile(reference_output)
    return sample_errors, position_error, normal_error, vertices, faces


def replay(
    binary, folder, output, seed, authored_limbs, along=2, sides=8, roundtrip=False
):
    name, saved, points, radii, paths = load_source(folder, seed, authored_limbs)
    encoded = encode(points, radii, paths, along, sides)
    (output / f"{name}.input").write_text(encoded)
    product = output / f"{name}.native.bin"
    command = [str(binary), str(product), *(["--roundtrip"] if roundtrip else [])]
    oversized = [path for path in paths if len(path) > 65]
    if oversized:
        for path in oversized:
            try:
                path_recipe(points, radii, path)
            except ValueError as error:
                assert "65" in str(error)
            else:
                raise AssertionError("reference unexpectedly admitted oversized limb")
        process = subprocess.run(
            command,
            input=encoded,
            text=True,
            capture_output=True,
            check=False,
            timeout=60,
        )
        assert process.returncode == 2, (
            "native input must retain the reference capacity"
        )
        return {
            "name": name,
            "status": "rejected",
            "reason": "source limb exceeds 65 anchors",
            "source_nodes": len(points),
            "limbs": len(paths),
            "maximum_limb_anchors": max(map(len, paths)),
            "source_sha256": hashlib.sha256(saved.read_bytes()).hexdigest(),
        }
    process = subprocess.run(
        command,
        input=encoded,
        text=True,
        capture_output=True,
        check=True,
        timeout=60,
    )
    (output / f"{name}.stdout").write_text(process.stdout)
    sample_errors = np.zeros(12)
    position_error, normal_error, vertices, faces = 0.0, 0.0, 0, 0
    reference_product = output / f"{name}.python.bin"
    with product.open("rb") as stream, reference_product.open("wb") as reference_output:
        for path in paths:
            error, position, normal, nv, nf = compare_arrays(
                stream, reference_output, points, radii, path, along, sides
            )
            sample_errors = np.maximum(sample_errors, error)
            position_error, normal_error = (
                max(position_error, position),
                max(normal_error, normal),
            )
            vertices += nv
            faces += nf
        assert stream.read() == b"", "no unaccounted native products"
    row = process.stdout.split()
    assert list(map(int, row[:3])) == [len(paths), vertices, faces]
    return {
        "name": name,
        "status": "compared",
        "source_nodes": len(points),
        "limbs": len(paths),
        "maximum_limb_anchors": max(map(len, paths)),
        "along": along,
        "sides": sides,
        "vertices": vertices,
        "triangles": faces,
        "native_preparation_ms": float(row[3]),
        "native_surface_ms": float(row[4]),
        "native_roundtrip_ms": float(row[5]),
        "sample_max_errors": sample_errors.tolist(),
        "position_max_error_m": position_error,
        "normal_max_error": normal_error,
        "source_sha256": hashlib.sha256(saved.read_bytes()).hexdigest(),
        "input_sha256": hashlib.sha256(encoded.encode()).hexdigest(),
        "output_sha256": hashlib.sha256(product.read_bytes()).hexdigest(),
        "reference_output_sha256": hashlib.sha256(
            reference_product.read_bytes()
        ).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--growth-cohort", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--roundtrip", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    receipt = {
        "source_head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "numpy": np.__version__,
        "stored_roundtrip": args.roundtrip,
        "scope": "original full wood paths; no joint union, bark UV, LOD error, cache or world admission",
        "cases": [],
    }
    for seed in (682, 718):
        for limbs in (False, True):
            row = replay(
                args.binary.resolve(),
                args.growth_cohort,
                args.output,
                seed,
                limbs,
                roundtrip=args.roundtrip,
            )
            receipt["cases"].append(row)
            (args.output / "receipt.json").write_text(
                json.dumps(receipt, indent=2) + "\n"
            )
            print(json.dumps(row), flush=True)


if __name__ == "__main__":
    main()

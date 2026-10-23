"""Compare indexed native height solids with unchanged Python body closures (#1189)."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from native_planar_mesh import ROOT, build_probe
from outshine_ref.contracts import Mesh
from stair_junction_body import close
from surface_solid_checks import audit_closed

UNITS = ("HeightSolid.cpp", "HeightSolidWalls.cpp", "HeightSolidValidation.cpp")


def write_input(path, points, faces, steps, levels):
    tops = points[faces, 2].copy()
    selected = steps >= 0
    tops[selected] = levels[steps[selected], None]
    with path.open("w") as stream:
        stream.write(f"{len(points)} {len(faces)}\n")
        for point in points:
            values = (*point[:2], point[2] - 0.3)
            stream.write(" ".join(format(value, ".17g") for value in values) + "\n")
        for face, heights in zip(faces, tops, strict=True):
            indices = " ".join(str(int(value)) for value in face)
            values = " ".join(format(value, ".17g") for value in heights)
            stream.write(f"{indices} {values}\n")


def read_output(path):
    lines = path.read_text().splitlines()
    vertices, faces, surface_faces = map(int, lines[0].split())
    walls = faces - 2 * surface_faces
    if not (0 <= vertices <= 32768 and 0 <= walls <= faces <= 65536):
        raise ValueError("bounded complete native height solid required")
    if len(lines) != 1 + vertices + faces + walls:
        raise ValueError("complete native geometry and wall metadata required")
    xyz_end = 1 + vertices
    triangles_end = xyz_end + faces
    xyz = np.array([list(map(float, row.split())) for row in lines[1:xyz_end]])
    triangles = np.array(
        [list(map(int, row.split())) for row in lines[xyz_end:triangles_end]]
    )
    metadata = np.array([list(map(int, row.split())) for row in lines[triangles_end:]])
    return Mesh(xyz, triangles), metadata, surface_faces


def compare(binary, source, output):
    with np.load(source, allow_pickle=False) as data:
        args = tuple(data[key] for key in ("points", "faces", "step", "levels"))
    reference, _top, _bottom, _walls, families, _volume = close(*args)
    input_path, output_path = output.with_suffix(".input"), output.with_suffix(".mesh")
    write_input(input_path, *args)
    result = subprocess.run(
        [str(binary), str(input_path), str(output_path)],
        capture_output=True,
        text=True,
        check=True,
    )
    actual, metadata, surface_faces = read_output(output_path)
    np.testing.assert_array_equal(actual.vertices.view(np.uint64), reference.vertices.view(np.uint64))
    np.testing.assert_array_equal(actual.triangles, reference.triangles)
    np.testing.assert_array_equal(metadata[:, 1], families)
    if surface_faces != len(args[1]) or not (metadata[:, 0] < surface_faces).all():
        raise ValueError("every wall must retain its input surface-face owner")
    float32_volume = audit_closed(Mesh(actual.vertices.astype(np.float32), actual.triangles))
    native = json.loads(result.stdout)
    if min(native["volume_m3"], float32_volume) <= 0:
        raise ValueError("positive analytic and closed Float32 volume required")
    return {
        "input": str(source),
        "geometry_words_equal": True,
        "wall_families_equal": True,
        "float32_volume_m3": float32_volume,
        **native,
    }


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    binary = build_probe(
        args.output,
        units=UNITS,
        probe_source=ROOT / "test/harness/spatial/HeightSolidProbe.cpp",
    )
    sources = args.input
    if not sources:
        from mvt_independent_flight_body import construct

        sources = []
        for name in ("stacked", "crossed"):
            data = construct(name)["data"]
            path = args.output / f"{name}.npz"
            np.savez_compressed(
                path,
                points=data["reference_vertices"],
                faces=data["reference_faces"],
                step=data["face_step"],
                levels=data["levels_m"],
            )
            sources.append(path)
    cases = [compare(binary, path, args.output / f"case-{i}") for i, path in enumerate(sources)]
    (args.output / "result.json").write_text(json.dumps(cases, indent=2) + "\n")
    print(json.dumps(cases, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", type=Path, default=[])
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

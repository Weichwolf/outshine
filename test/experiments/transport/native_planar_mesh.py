"""Compare the native weld/conformance consumer with the unchanged Python layout."""

import argparse
import hashlib
import json
import shlex
import subprocess
import time
from pathlib import Path

import numpy as np
from planar_layout_reference import ablation, hanging_pairs, original_layout, repair
from stair_test_transform import rigid_coordinates

ROOT = Path(__file__).resolve().parents[3]
UNITS = (
    "PlanarMeshConformance.cpp",
    "PlanarVertexWelding.cpp",
    "PlanarEdgeSplits.cpp",
    "PlanHierarchy.cpp",
)


def build_probe(output, *, units=UNITS, probe_source=None, libraries=()):
    database = json.loads((ROOT / "compile_commands.json").read_text())
    entries = [row for row in database if Path(row["file"]).name in units]
    if len(entries) != len(units):
        raise ValueError("current complete compilation database required; run make db")
    template = next(row for row in entries if Path(row["file"]).name == units[0])
    sources = [(row, Path(row["file"])) for row in entries]
    sources.append(
        (template, probe_source or ROOT / "test/harness/spatial/PlanarMeshProbe.cpp")
    )
    commands, objects, hashes = [], [], {}
    for row, source in sources:
        hashes[str(source)] = hashlib.sha256(source.read_bytes()).hexdigest()
        argv = row.get("arguments") or shlex.split(row["command"])
        argv = list(argv)
        argv[argv.index(row["file"])] = str(source)
        if source == sources[-1][1]:
            argv.append("-I" + str(ROOT / "test/harness/spatial"))
        target = output / (source.stem + ".o")
        argv[argv.index("-o") + 1] = str(target)
        if "-MF" in argv:
            argv[argv.index("-MF") + 1] = str(target.with_suffix(".d"))
        with (output / (source.stem + ".log")).open("w") as log:
            subprocess.run(
                ["nice", "-n", "10", *argv],
                cwd=row["directory"],
                stdout=log,
                stderr=log,
                check=True,
            )
        commands.append(
            {"directory": row["directory"], "file": str(source), "arguments": argv}
        )
        objects.append(str(target))
    (output / "compile_commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    (output / "source-hashes.json").write_text(json.dumps(hashes, indent=2) + "\n")
    binary = output / "planar-mesh-probe"
    link = [commands[0]["arguments"][0], *objects, *libraries, "-o", str(binary)]
    with (output / "link.log").open("w") as log:
        subprocess.run(["nice", "-n", "10", *link], stdout=log, stderr=log, check=True)
    (output / "link.json").write_text(json.dumps(link) + "\n")
    return binary


def write_input(path, points, faces):
    with path.open("w") as stream:
        stream.write(f"{len(points)} {len(faces)} 1e-8\n")
        for point in points:
            stream.write(" ".join(format(value, ".17g") for value in point) + "\n")
        for face in faces:
            stream.write(" ".join(str(int(value)) for value in face) + "\n")


def read_output(path, input_count):
    lines = path.read_text().splitlines()
    count, faces = map(int, lines[0].split())
    if (
        not 0 <= count <= 4096
        or not 0 <= faces <= 8192
        or len(lines) != count + faces + 2
    ):
        raise ValueError("bounded complete native probe output required")
    points = np.array(
        [list(map(float, line.split())) for line in lines[1 : 1 + count]], dtype=float
    ).reshape(count, 2)
    records = np.array(
        [list(map(int, line.split())) for line in lines[1 + count : 1 + count + faces]],
        dtype=np.int64,
    ).reshape(faces, 4)
    remap = np.array(list(map(int, lines[1 + count + faces].split())))
    if (
        points.shape != (count, 2)
        or records.shape != (faces, 4)
        or remap.shape != (input_count,)
    ):
        raise ValueError("complete native points/faces/parent/remap output required")
    return points, records[:, :3], records[:, 3], remap


def rigid_controls(binary, output, xy, faces):
    results = []
    cases = (
        ("rotate", 0.37, [0, 0]),
        ("shift", 0, [1024, -2048]),
        ("rotate-shift", -0.83, [100000, -120000]),
    )
    for name, angle, translation in cases:
        transform, _rotation = rigid_coordinates(angle, np.asarray(translation))
        points = np.asarray(transform(xy))
        expected, expected_faces, expected_map, _drift, _work = repair(points, faces)
        source, target = output / (name + "-input.txt"), output / (name + "-native.txt")
        write_input(source, points, faces)
        command = [str(binary), str(source), str(target)]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        row = {
            "name": name,
            "angle": angle,
            "translation": translation,
            "command": command,
            "terminal": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }
        results.append(row)
        (output / "rigid-runs.json").write_text(json.dumps(results, indent=2) + "\n")
        result.check_returncode()
        actual, actual_faces, _parents, remap = read_output(target, len(points))
        np.testing.assert_array_equal(actual.view(np.uint64), expected.view(np.uint64))
        np.testing.assert_array_equal(actual_faces, expected_faces)
        np.testing.assert_array_equal(remap, expected_map)
        if len(hanging_pairs(actual, actual_faces)):
            raise ValueError("native rigid control retains a hanging interface")
        row.update(exact=True, hanging_pairs=0)
    return results


def compare(binary, output, repeats):
    reference, raw, args = original_layout()
    expected_points, _faces, _map, _drift, _work = repair(raw[0], raw[1])
    np.testing.assert_array_equal(
        expected_points.view(np.uint64), reference[0].view(np.uint64)
    )
    xy, faces, _owners, _note = raw
    write_input(output / "input.txt", xy, faces)
    np.savez_compressed(output / "raw.npz", xy=xy, faces=faces)
    np.savez_compressed(output / "reference.npz", xy=reference[0], faces=reference[1])
    results = []
    for _index in range(repeats):
        at = time.perf_counter()
        expected, expected_faces, expected_map, drift, _work = repair(xy, faces)
        elapsed = time.perf_counter() - at
        target = output / "native.txt"
        command = [str(binary), str(output / "input.txt"), str(target)]
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        row = {
            "command": command,
            "terminal": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "python_repair_seconds": elapsed,
        }
        results.append(row)
        (output / "runs.json").write_text(json.dumps(results, indent=2) + "\n")
        result.check_returncode()
        points, native_faces, parents, remap = read_output(target, len(xy))
        np.testing.assert_array_equal(points.view(np.uint64), expected.view(np.uint64))
        np.testing.assert_array_equal(native_faces, expected_faces)
        np.testing.assert_array_equal(native_faces, reference[1])
        np.testing.assert_array_equal(remap, expected_map)
        if parents.min() < 0 or parents.max() >= len(faces):
            raise ValueError("native face parent outside original input")
        pending = hanging_pairs(points, native_faces)
        if len(pending):
            raise ValueError("native output retains a hanging interface")
        row.update(
            vertices=len(points),
            faces=len(native_faces),
            exact=True,
            hanging_pairs=0,
            maximum_weld_distance=drift,
        )
    summary = {
        "input_points": len(xy),
        "input_faces": len(faces),
        "runs": results,
        "ablation": ablation(args),
        "rigid_controls": rigid_controls(binary, output, xy, faces),
        "scope": "Actual pre-noded original input; weld/conformance only. No native noding, "
        "height-LP, mesh/nav/terrain integration, frame/IO/GPU/world acceptance. "
        "Native time covers one repair call, Python the same phases. "
        "Allocations/copies/process peak uninstrumented; no complexity optimum claim.",
    }
    (output / "comparison.json").write_text(json.dumps(summary, indent=2) + "\n")
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--repeats", type=int, default=7)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not 1 <= args.repeats <= 100:
        parser.error("fresh system-temp output and 1..100 repeats required")
    output.mkdir(parents=True, exist_ok=False)
    native = output / "native"
    native.mkdir()
    binary = args.binary.resolve() if args.binary else build_probe(native)
    summary = compare(binary, output, args.repeats)
    print(
        json.dumps(
            {"complete": True, "runs": len(summary["runs"]), "output": str(output)}
        )
    )


if __name__ == "__main__":
    main()

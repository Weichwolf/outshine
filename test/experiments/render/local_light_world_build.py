"""Check native world-cell producer bytes, local edits and measured own allocation costs."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from export_local_light_native import inputs
from local_light_inputs import ROOT
from local_light_world import build_world


def edits(packed, target=0):
    states = []
    current = packed.copy()
    for kind in (
        "colour",
        "spot-to-point",
        "move",
        "range",
        "unbounded",
        "direction",
        "finite",
    ):
        light = current[target].copy()
        if kind == "colour":
            light[0, 0] += 1
        elif kind == "spot-to-point":
            light[0, 3] = 1
        elif kind == "move":
            light[1, :3] += [32.75, -0.125, 1 / 30]
        elif kind == "range":
            light[1, 3] /= 2
        elif kind == "unbounded":
            light[1, 3] = 0
        elif kind == "direction":
            light[0, 3], light[1, 3] = 0, 100
        else:
            light[0, 3], light[1, 3] = packed[target, 0, 3], packed[target, 1, 3]
        current[target] = light
        states.append((kind, target, light.copy(), current.copy()))
    return states


def compare(prefix, packed, width, budget, translation, method):
    grid = build_world(packed, width, budget, translation)
    expected, stats = grid.pack(method)
    for suffix, reference in (
        ("words", expected),
        ("lights", grid.local_lights),
        ("cells", grid.light_cells),
    ):
        path = Path(str(prefix) + f".{suffix}.bin")
        if path.read_bytes() != reference.tobytes():
            raise AssertionError((str(prefix), suffix, "native/reference bytes differ"))
    return stats


def verify_rejections(binary, root):
    _, _, packed, _, _ = inputs("small", True, "street")
    source = root / "reject-valid.bin"
    packed.tofile(source)
    prefix = root / "reject-output"
    base = [
        str(binary),
        str(source),
        str(prefix),
        "32",
        "hash",
        "1024",
        "0",
        "0",
        "0",
        "1",
    ]
    cases = []
    for field, value in ((3, "0"), (4, "unknown"), (5, "0"), (6, "nan")):
        argv = base.copy()
        argv[field] = value
        cases.append(argv)
    for index, value in (
        ((0, 0, 0), np.nan),
        ((0, 1, 3), -1),
        ((0, 0, 3), 3),
        ((0, 1, 0), 2**38),
    ):
        changed = packed.copy()
        changed[index] = value
        path = root / f"reject-source-{len(cases)}.bin"
        changed.tofile(path)
        cases.append([base[0], str(path), *base[2:]])
    malformed = root / "reject-short.bin"
    malformed.write_bytes(b"x")
    cases.append([base[0], str(malformed), *base[2:]])
    for record in (
        np.array([0], dtype="<u4"),
        np.r_[np.uint32(len(packed)), packed[0].view("<u4").ravel()],
    ):
        path = root / f"reject-update-{len(cases)}.bin"
        record.astype("<u4").tofile(path)
        cases.append([*base, str(path)])
    rows = []
    for argv in cases:
        result = subprocess.run(
            argv, capture_output=True, text=True, timeout=30, check=False
        )
        rows.append({"argv": argv, "exit": result.returncode, "stderr": result.stderr})
        if result.returncode != 2 or result.stderr:
            raise AssertionError((argv, result.returncode, result.stderr))
    (root / "rejections.json").write_text(json.dumps(rows, indent=2) + "\n")
    return rows


def run(
    binary,
    root,
    name,
    packed,
    width,
    budget,
    translation,
    method,
    repeats,
    update,
    target=0,
):
    source, prefix = root / f"{name}.source.bin", root / name
    packed.tofile(source)
    argv = [str(binary), str(source), str(prefix), str(width), method, str(budget)]
    argv += [str(value) for value in translation] + [str(repeats)]
    states = edits(packed, target) if update else []
    if states:
        path = root / f"{name}.updates.bin"
        np.array(
            [
                np.r_[np.uint32(id_), light.view("<u4").ravel()]
                for _, id_, light, _ in states
            ],
            dtype="<u4",
        ).tofile(path)
        argv.append(str(path))
    result = subprocess.run(
        argv, capture_output=True, text=True, timeout=180, check=False
    )
    (root / f"{name}.stdout").write_text(result.stdout)
    (root / f"{name}.stderr").write_text(result.stderr)
    if result.returncode or result.stderr:
        raise AssertionError((argv, result.returncode, result.stderr))
    stats = compare(prefix, packed, width, budget, translation, method)
    updated = []
    for index, (kind, _, _, state) in enumerate(states):
        item = compare(
            Path(str(prefix) + f".update-{index}"),
            state,
            width,
            budget,
            translation,
            method,
        )
        updated.append({"kind": kind, **item})
    rows = [json.loads(line) for line in result.stdout.splitlines()]
    for row in rows:
        if (
            row["phase"] == "update"
            and row["step"] in (0, 1, 5)
            and (row["sphere_box_tests"] or row["membership_changes"])
        ):
            raise AssertionError(
                "photometry/global-only edits must not retest spatial cells"
            )
    return {
        "name": name,
        "argv": argv,
        "exit": result.returncode,
        "stats": stats,
        "updates": updated,
        "native": rows,
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=1, choices=range(1, 17))
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private external output required")
    args.output.mkdir(parents=True, exist_ok=False)
    verify_rejections(args.binary, args.output)
    rows = []
    cases = []
    for scene in ("small", "street", "dense", "overlap"):
        _, _, packed, _, _ = inputs(scene, True, "street")
        cases.extend(
            (
                f"{scene}-{width}",
                packed,
                width,
                16 * 1024**2,
                [0, 0, 0],
                scene == "dense" and width == 32,
            )
            for width in (16, 32, 64, 128)
        )
    _, _, dense, _, _ = inputs("dense", True, "street")
    cases.extend(
        [
            (
                "earth",
                dense,
                32,
                16 * 1024**2,
                [6_000_000, -4_000_000, 3_200_000],
                True,
            ),
            ("overflow", dense, 32, 1, [0, 0, 0], False),
            ("empty", dense[:0], 32, 16 * 1024**2, [0, 0, 0], False),
        ]
    )
    directional = dense[1:2].copy()
    directional[0, 1, 3] = 100
    cases.append(
        ("directional-unused-range", directional, 32, 16 * 1024**2, [0, 0, 0], False)
    )
    for name, packed, width, budget, translation, update in cases:
        for method in ("hash", "sorted"):
            row = run(
                args.binary,
                args.output,
                f"{name}-{method}",
                packed,
                width,
                budget,
                translation,
                method,
                args.repeats,
                update,
            )
            rows.append(row)
            (args.output / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
            print(row["name"], "native/reference bytes match", flush=True)


if __name__ == "__main__":
    main()

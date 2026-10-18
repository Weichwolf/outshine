"""Compare the native render light-cell producer against independent world-index bytes."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from export_local_light_native import inputs
from local_light_inputs import ROOT
from local_light_world import build_world


def expected_words(packed, actual_lights, translation, full):
    effective = packed.copy()
    effective[:, 1, 3] = actual_lights[:, 1, 3]
    inverse_ranges = effective[:, 1, 3]
    subnormal = (inverse_ranges > 0) & (inverse_ranges < np.finfo(np.float32).tiny)
    effective[subnormal, 1, 3] = 0
    grid = build_world(effective, 32, 16 * 1024**2, translation)
    if full:
        grid.cells.clear()
        grid.globals = np.arange(len(packed), dtype="<u4")
        grid.overflow = True
    old, _ = grid.pack("hash")
    count = len(packed)
    converted = old[: len(old) - 3 * count].copy() if count else old.copy()
    slot_count = int(old[3]) + 1
    for slot in range(slot_count):
        at = int(old[2]) + 5 * slot
        if old[at + 4]:
            converted[at + 3] += 3
    converted[[0, 2, 4]] += 3
    finite = (effective[:, 0, 3] > 0.5) & (effective[:, 1, 3] > 0)
    radius = float(np.max(1 / effective[finite, 1, 3].astype(float), initial=0))
    maximum = (
        np.nextafter(np.float32(radius), np.float32(np.inf))
        if radius
        else np.float32(0)
    )
    return np.r_[converted[:5], [maximum.view("<u4"), count, 1], converted[5:]].astype(
        "<u4"
    )


def check(binary, root, name, packed, translation):
    source = root / (name + ".source")
    source.write_bytes(packed.tobytes())
    paths = [root / (name + suffix) for suffix in (".words", ".lights", ".positions")]
    argv = [str(binary), str(source), *map(str, paths), *map(str, translation)]
    result = subprocess.run(
        argv, capture_output=True, text=True, timeout=60, check=False
    )
    (root / (name + ".log")).write_text(result.stdout + result.stderr)
    if result.returncode or result.stderr:
        raise AssertionError((argv, result.returncode, result.stderr))
    words = np.fromfile(paths[0], dtype="<u4")
    lights = np.fromfile(paths[1], dtype="<f4").reshape(-1, 4, 4)
    positions = np.fromfile(paths[2], dtype="<f8").reshape(-1, 3)
    if not np.array_equal(positions, packed[:, 1, :3].astype(float) + translation):
        raise AssertionError("actual native source world coordinates differ")
    expected = expected_words(
        packed, lights, translation, json.loads(result.stdout)["full"]
    )
    if not np.array_equal(words, expected):
        mismatch = (
            np.flatnonzero(words != expected)[:10]
            if words.shape == expected.shape
            else []
        )
        raise AssertionError(
            (
                name,
                "native/reference index bytes differ",
                words.shape,
                expected.shape,
                mismatch,
            )
        )
    if len(lights) != len(packed) or not np.all(np.isfinite(lights)):
        raise AssertionError("complete finite native Light64 output required")
    return {
        "name": name,
        "argv": argv,
        "exit": result.returncode,
        "stats": json.loads(result.stdout),
        "sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in [source, *paths]
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("external output required")
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for scene in ("small", "street", "dense", "overlap"):
        _, _, packed, _, _ = inputs(scene, True, "street")
        for translation in ([0, 0, 0], [6_000_000, -4_000_000, 3_200_000]):
            name = scene + ("-earth" if translation[0] else "-local")
            rows.append(
                check(args.binary, args.output, name, packed, np.array(translation))
            )
            (args.output / "receipt.json").write_text(json.dumps(rows, indent=2) + "\n")
            print(name, "native/reference exact index bytes", flush=True)
    rows.append(check(args.binary, args.output, "empty", packed[:0], np.zeros(3)))
    directional = packed[1:2].copy()
    directional[:, 1, 3] = 100
    rows.append(
        check(
            args.binary, args.output, "direction-unused-range", directional, np.zeros(3)
        )
    )
    mixed = packed[:4].copy()
    mixed[:, 0, 3] = [1, 0, 1, 2]
    tiny = np.finfo(np.float32).tiny
    mixed[:, 1, 3] = [0.125, 100, 0, tiny / 2]
    for suffix, translation in (
        ("local", np.zeros(3)),
        ("earth", np.array([6_000_000, -4_000_000, 3_200_000])),
    ):
        rows.append(
            check(
                args.binary,
                args.output,
                "mixed-subnormal-" + suffix,
                mixed,
                translation,
            )
        )
    subnormal = mixed[[0, 3]].copy()
    subnormal[:, 1, 3] = [tiny / 2, tiny * 0.75]
    rows.append(
        check(args.binary, args.output, "all-subnormal", subnormal, np.zeros(3))
    )
    (args.output / "receipt.json").write_text(json.dumps(rows, indent=2) + "\n")


if __name__ == "__main__":
    main()

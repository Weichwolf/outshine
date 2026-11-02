"""Qualify spatial mip-page lookup through the native R8 atlas upload path."""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from native_litter_page_sampling import bilinear
from native_litter_pages import probe
from native_terrain_litter_field import capture
from native_terrain_material_pyramid import ROOT, run
from terrain_litter_field import LitterField
from terrain_litter_page import bake_page
from terrain_litter_page_filter import filter_reference, filtered_queries


def key_hash(key):
    h = 0
    for word, multiplier in zip(key, (0x9E3779B9, 0x85EBCA6B, 0xC2B2AE35)):
        h ^= (int(word) * multiplier) & 0xFFFFFFFF
    h = ((h ^ (h >> 16)) * 0x7FEB352D) & 0xFFFFFFFF
    h = ((h ^ (h >> 15)) * 0x846CA68B) & 0xFFFFFFFF
    return h ^ (h >> 16)


def table_for(keys, entries, omit=3):
    table = np.zeros((entries, 4), dtype="<u4")
    for slot, key in enumerate(keys):
        if slot == omit:
            continue
        first = key_hash(key)
        for step in range(8):
            index = (first + step) & (entries - 1)
            if table[index, 3] == 0:
                table[index, :3] = np.asarray(key, dtype="<i4").view("<u4")
                table[index, 3] = slot + 1
                break
        else:
            raise ValueError("fixture exceeded its bounded resident probe count")
    return table


def queries_for(keys, core, rng):
    blocks = []
    centres = []
    for x, y, mip in keys:
        step = 2**mip / 16
        integer = rng.integers(0, core, (64, 2))
        exact = (np.array([x, y]) * core + integer + 0.5) * step
        fractional = (
            np.array([x, y]) * core + rng.uniform(-1, core + 1, (96, 2))
        ) * step
        points = np.concatenate((exact, fractional)).astype("<f4")
        words = np.zeros((len(points), 12), dtype="<u4")
        words[:, :2] = points.view("<u4")
        words[:, 2] = mip
        blocks.append(words)
        centres.extend([True] * len(exact) + [False] * len(fractional))
    return np.concatenate(blocks), np.asarray(centres)


def qualify(output, name, core, keys, pixels, commands, rng):
    side = core + 2
    atlas = np.zeros((3 * side, 3 * side), dtype=np.uint8)
    for slot, key in enumerate(keys):
        row, col = divmod(slot, 3)
        atlas[row * side : (row + 1) * side, col * side : (col + 1) * side] = pixels[
            key
        ]
    words, centres = queries_for(keys, core, rng)
    positions = words[:, :2].view("<f4").astype(float)
    levels = words[:, 2].astype(int)
    texels = positions * np.exp2(4 - levels[:, None])
    tiles = np.floor(texels / core).astype(np.int32)
    requested = np.column_stack((tiles, levels))
    records = []
    for entries in (8, 32):
        table = table_for(keys, entries)
        sampled = capture(
            output,
            f"{name}-{entries}",
            LitterField((0, 0), 1, 1, (atlas,)),
            words.view("<f4"),
            commands,
            np.concatenate(
                (np.array([core, 3, entries, 8], dtype="<u4"), table.ravel())
            ),
        )
        present = {key for slot, key in enumerate(keys) if slot != 3}
        found = np.asarray([tuple(key) in present for key in requested])
        np.testing.assert_array_equal(sampled[:, 1], found)
        np.testing.assert_array_equal(sampled[:, 4:7], requested)
        expected = np.zeros(len(words))
        for key in present:
            use = np.all(requested == key, axis=1)
            local = texels[use] - np.array(key[:2]) * core + 1
            expected[use] = bilinear(pixels[key], local / side)
        error = np.abs(sampled[:, 0] - expected)
        if np.any(sampled[~found, 0] != 0):
            raise ValueError(
                "an absent or colliding page sampled another resident slot"
            )
        centre_error = float(error[centres & found].max(initial=0))
        if centre_error > 8 * np.finfo(np.float32).eps:
            raise ValueError("atlas addressing moved stored texel centres")
        records.append(
            {
                "core": core,
                "levels": sorted(set(levels.tolist())),
                "entries": entries,
                "queries": len(words),
                "misses": int((~found).sum()),
                "centre_error": centre_error,
                "observed_bilinear_error": float(error.max()),
            }
        )
    filtered = filtered_queries(keys, core, rng)
    for mode in ("complete", "hole", "empty"):
        table = (
            np.zeros((32, 4), dtype="<u4")
            if mode == "empty"
            else table_for(keys, 32, 3 if mode == "hole" else None)
        )
        present = (
            set()
            if mode == "empty"
            else {key for slot, key in enumerate(keys) if mode != "hole" or slot != 3}
        )
        native = capture(
            output,
            f"{name}-filter-{mode}",
            LitterField((0, 0), 1, 1, (atlas,)),
            filtered,
            commands,
            np.concatenate((np.array([core, 3, 32, 8], dtype="<u4"), table.ravel())),
        )
        expected, found, missing = filter_reference(filtered, pixels, present, core)
        if mode == "complete" and not found[:-4].all():
            raise ValueError(
                "complete parent/child coverage missed a positive corpus sample"
            )
        np.testing.assert_array_equal(native[:, 7], found)
        np.testing.assert_array_equal(native[~found, 4:7], missing[~found])
        np.testing.assert_array_equal(native[~found, :4], 0)
        np.testing.assert_array_equal(native[found, 2], expected[found, 2])
        error = np.abs(native[found, 0] - expected[found, 0])
        maximum = float(error.max(initial=0))
        if maximum > 1 / 255:
            raise ValueError(
                "qualified atlas filter exceeds one R8 step in this corpus"
            )
        records.append(
            {
                "core": core,
                "levels": sorted(set(levels.tolist())),
                "filtered_queries": len(filtered),
                "mode": mode,
                "misses": int((~found).sum()),
                "observed_filter_error": maximum,
            }
        )
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(p)
        for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private native output belongs in system temp")
    output.mkdir(parents=True, exist_ok=False)
    (output / "assets").mkdir()
    commands = [
        run(
            [
                "nice",
                "-n",
                "10",
                "cmake",
                "--build",
                str(ROOT / "build"),
                "--target",
                "outshine_material_fragment_probe",
                "outshine_litter_page_probe",
                "--parallel",
                str(os.cpu_count() or 1),
            ],
            output,
            "build",
        )
    ]
    compiler = next(
        line.split("=", 1)[1]
        for line in (ROOT / "build/CMakeCache.txt").read_text().splitlines()
        if line.startswith("OUTSHINE_GLSLANG:FILEPATH=")
    )
    for name, path in (
        ("vertex", ROOT / "src/render/shaders/fullscreen.vert"),
        ("fragment", Path(__file__).with_name("terrain_litter_page_probe.frag")),
    ):
        commands.append(
            run(
                [
                    compiler,
                    "-V",
                    "--target-env",
                    "vulkan1.0",
                    "-I" + str(ROOT / "src/render/shaders"),
                    str(path),
                    "-o",
                    str(output / (name + ".spv")),
                ],
                output,
                name,
            )
        )
    rng = np.random.default_rng(1934)
    records = []
    for core in (64, 128):
        for case, (x, y) in enumerate(((0, 0), (-64, -64), (12345, -23456))):
            levels = (2 * case, 2 * case + 1)
            keys, pixels = [], {}
            for dy, dx in ((-1, -1), (-1, 0), (0, -1), (0, 0)):
                tile = (x + dx, y + dy)
                for key in (
                    (*tile, levels[0]),
                    (tile[0] // 2, tile[1] // 2, levels[1]),
                ):
                    if key not in keys:
                        keys.append(key)
            for slot, key in enumerate(keys):
                tile, mip = key[:2], key[2]
                name = f"source-{core}-{case}-{slot}"
                probe(
                    output,
                    name + "-build",
                    output / "assets/pages.sqlite",
                    tile,
                    core,
                    mip,
                    "build",
                    commands,
                )
                probe(
                    output,
                    name,
                    output / "assets/pages.sqlite",
                    tile,
                    core,
                    mip,
                    "hit",
                    commands,
                )
                data = np.fromfile(
                    output / name / f"mip-{mip}.r8", dtype=np.uint8
                ).reshape(core + 2, core + 2)
                np.testing.assert_array_equal(
                    data, bake_page(tile, mip, core_texels=core)
                )
                pixels[key] = data
            records += qualify(
                output, f"atlas-{core}-{case}", core, keys, pixels, commands, rng
            )
    sources = [
        Path(__file__),
        Path(__file__).with_name("terrain_litter_page_filter.py"),
        Path(__file__).with_name("native_litter_page_sampling.py"),
        Path(__file__).with_name("native_terrain_litter_field.py"),
        Path(__file__).with_name("terrain_field_demand.py"),
        Path(__file__).with_name("terrain_litter_page.py"),
        Path(__file__).with_name("terrain_material_plane.py"),
        Path(__file__).with_name("terrain_litter_page_probe.frag"),
        ROOT / "src/render/shaders/fieldPage.glsl",
        ROOT / "src/render/shaders/fieldFootprint.glsl",
        ROOT / "src/render/shaders/groundLitterPages.glsl",
        ROOT / "src/render/shaders/surfaceHash.glsl",
        ROOT / "build/outshine_material_fragment_probe",
        ROOT / "build/outshine_litter_page_probe",
    ]
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "commands": commands,
        "records": records,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources
        },
        "limitations": [
            "Immutable atlas/table qualification; no World consumer, frame time, GPU feedback or residency policy acceptance.",
            "Fractional interpolation error is observed, not a universal hardware bound.",
        ],
    }
    (output / "receipt.json").write_text(json.dumps(result, indent=2) + "\n")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()

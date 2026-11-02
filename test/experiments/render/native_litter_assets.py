"""Native direct mip generation, persistent source-free hits, then R8 GPU consumption."""

import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_terrain_material_basis import load_capture
from native_terrain_litter_field import capture, queries
from native_terrain_material_pyramid import ROOT, run
from probe_terrain_material_substrate import optical_row
from terrain_litter_field import LitterField, bake
from terrain_litter_mip import bake_mip


def exported_rows(folder, output, tile, first, mode, commands):
    began = time.perf_counter()
    commands.append(
        run(
            [
                str(ROOT / "build/outshine_litter_field_probe"),
                str(output / "assets.sqlite"),
                str(folder),
                str(tile[0]),
                str(tile[1]),
                str(first),
                mode,
            ],
            output,
            folder.name,
        )
    )
    milliseconds = (time.perf_counter() - began) * 1000
    rows = []
    for line in (output / (folder.name + ".log")).read_text().splitlines():
        if line.startswith("litter "):
            row = dict(word.split("=", 1) for word in line.split()[1:])
            level = int(row["level"])
            expected = hashlib.sha256(
                struct.pack("<IIii", 0x31464C52, level, *tile)
            ).hexdigest()
            if row["key"] != expected:
                raise ValueError(
                    "native address differs from canonical format/tile/mip identity"
                )
            if mode == "hit" and int(row["generated"]) != 0:
                raise ValueError("source-free hit invoked a generator")
            rows.append(row)
    return {"process_wall_ms": milliseconds, "rows": rows}


def compare(output, tile, case, commands):
    coarse = output / f"case-{case}-coarse"
    coarse_report = exported_rows(coarse, output, tile, 4, "build", commands)
    if sorted(p.name for p in coarse.iterdir()) != [
        f"mip-{i}.r8" for i in range(4, 10)
    ]:
        raise ValueError("coarse demand produced an unrequested fine image")
    gap = subprocess.run(
        [
            str(ROOT / "build/outshine_litter_field_probe"),
            str(output / "assets.sqlite"),
            str(output / f"case-{case}-gap"),
            str(tile[0]),
            str(tile[1]),
            "0",
            "hit",
        ],
        check=False,
        capture_output=True,
        timeout=30,
    )
    (output / f"case-{case}-gap.log").write_bytes(gap.stdout + gap.stderr)
    if (
        gap.returncode != 1
        or b"asset miss has no registered generator" not in gap.stderr
    ):
        raise ValueError(
            "coarse-only storage unexpectedly serves an absent fine product"
        )
    cold = output / f"case-{case}-full"
    cold_report = exported_rows(cold, output, tile, 0, "build", commands)
    hit = output / f"case-{case}-hit"
    hit_report = exported_rows(hit, output, tile, 0, "hit", commands)
    independent = bake(np.array(tile) * 16 - 8, 1, dtype="uint8")
    levels = []
    for level in range(10):
        side = 512 >> level
        values = np.fromfile(hit / f"mip-{level}.r8", dtype=np.uint8).reshape(
            side, side
        )
        np.testing.assert_array_equal(values, bake_mip(tile, level))
        np.testing.assert_array_equal(values, independent.levels[level])
        if (hit / f"mip-{level}.r8").read_bytes() != (
            cold / f"mip-{level}.r8"
        ).read_bytes():
            raise ValueError("stored ready pixels changed between native processes")
        levels.append(values)
    return {
        "tile": [int(value) for value in tile],
        "coarse": coarse_report,
        "full": cold_report,
        "source_free_hit": hit_report,
    }, tuple(levels)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not any(
        output.is_relative_to(p)
        for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("private asset evidence belongs in system temp")
    output.mkdir(parents=True, exist_ok=False)
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
                "outshine_litter_field_probe",
                "outshine_material_fragment_probe",
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
    for name, source in (
        ("vertex", ROOT / "src/render/shaders/fullscreen.vert"),
        ("fragment", Path(__file__).with_name("terrain_litter_texture_probe.frag")),
    ):
        commands.append(
            run(
                [
                    compiler,
                    "-V",
                    "--target-env",
                    "vulkan1.0",
                    str(source),
                    "-o",
                    str(output / (name + ".spv")),
                ],
                output,
                name,
            )
        )
    palette, layout, receipt = load_capture(args.capture)
    anchor = np.asarray(receipt["fine"]["origin_m"]) + [0.137, 0.271]
    tiles = sorted(
        {
            tuple(
                np.floor(
                    anchor / (16 * optical_row(palette, layout, row)["recipe"][2])
                ).astype(int)
            )
            for row in range(layout["rows"])
            if 0 < optical_row(palette, layout, row)["coverage"] < 1
        }
    )
    tiles += [(12345, -23456), (-134217727, 134217726)]
    records = []
    for i, tile in enumerate(tiles):
        record, levels = compare(output, tile, i, commands)
        records.append(record)
        if i == 0:
            field = LitterField(tuple(np.array(tile) * 16 - 8), 32, 1, levels)
            q, centres = queries(field, np.array(field.origin_m) + 16)
            q[q[:, 9] == 1, 9] = 3
            native = capture(output, "loaded-r8", field, q, commands)
            if not np.isfinite(native).all():
                raise ValueError("persisted native image did not reach the GPU")
            centre_error = float(np.max(np.abs(native[q[:, 9] == 0, 0] - centres)))
            if centre_error > 2 * np.finfo(np.float32).eps:
                raise ValueError("stored native mip pixels differ at GPU texel centres")
            record["gpu_queries"] = len(q)
            record["gpu_texel_centre_error"] = centre_error
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "commands": commands,
        "records": records,
        "palette": receipt["palette"],
        "limitations": [
            "Direct synchronous asset-library qualification; engine worker admission/terrain residency are not integrated.",
            "Material-space image products, not Earth-coordinate scene geometry or full-world frame-time acceptance.",
            "Native GPU sampling shown on A18; no Iris device acceptance.",
        ],
    }
    result["source_sha256"] = {
        str(p): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in [
            Path(__file__),
            ROOT / "src/generators/terrain/LitterFieldNoise.cpp",
            ROOT / "src/generators/terrain/PreparedLitterField.cpp",
            ROOT / "src/generators/terrain/LitterFieldNoise.h",
            ROOT / "src/generators/terrain/PreparedLitterField.h",
            ROOT / "test/harness/terrain/NativeLitterFieldProbe.cpp",
            ROOT / "build/outshine_litter_field_probe",
        ]
    }
    (output / "receipt.json").write_text(json.dumps(result, indent=2) + "\n")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()

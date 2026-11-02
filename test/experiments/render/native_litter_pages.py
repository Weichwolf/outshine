"""Native canonical page bytes, persistent source-free hits and 64/128 IO comparison."""

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
from native_terrain_material_pyramid import ROOT, run
from terrain_litter_page import bake_page


def probe(output, name, database, tile, core, first, mode, commands, count=1):
    folder = output / name
    call = [
        str(ROOT / "build/outshine_litter_page_probe"),
        str(database),
        str(folder),
        str(tile[0]),
        str(tile[1]),
        str(first),
        str(core),
        mode,
        str(count),
    ]
    began = time.perf_counter()
    commands.append(run(call, output, name))
    elapsed = (time.perf_counter() - began) * 1000
    rows = []
    for line in (output / (name + ".log")).read_text().splitlines():
        if not line.startswith("litter "):
            continue
        row = dict(word.split("=", 1) for word in line.split()[1:])
        level = int(row["level"])
        expected = hashlib.sha256(
            struct.pack(
                "<IIii", 0x32504C52, level | (core << 8), tile[0] + count - 1, tile[1]
            )
        ).hexdigest()
        if row["key"] != expected or (mode == "hit" and int(row["generated"]) != 0):
            raise ValueError("native page identity or generator-free policy mismatch")
        times = np.fromfile(folder / f"mip-{level}.f64", dtype=np.float64)
        if len(times) != count or not np.isfinite(times).all() or np.any(times < 0):
            raise ValueError("incomplete native resolution measurements")
        row["resolve_p50_p95_p99_us"] = np.percentile(times, [50, 95, 99]).tolist()
        rows.append(row)
    if [int(r["level"]) for r in rows] != list(range(5, first - 1, -1)):
        raise ValueError("incomplete native mip request sequence")
    return {"process_wall_ms": elapsed, "rows": rows}


def compare(output, tile, core, case, commands):
    database = output / "assets.sqlite"
    prefix = f"case-{case}-{core}"
    coarse = probe(
        output, prefix + "-coarse", database, tile, core, 4, "build", commands
    )
    expected_files = {f"mip-{m}.{ext}" for m in (4, 5) for ext in ("r8", "f64")}
    if {p.name for p in (output / (prefix + "-coarse")).iterdir()} != expected_files:
        raise ValueError("coarse demand created unrequested fine page products")
    if int(coarse["rows"][-1]["generated"]) != 2:
        raise ValueError("coarse demand did not generate exactly two requested pages")
    gap = subprocess.run(
        [
            str(ROOT / "build/outshine_litter_page_probe"),
            str(database),
            str(output / (prefix + "-gap")),
            str(tile[0]),
            str(tile[1]),
            "0",
            str(core),
            "hit",
        ],
        capture_output=True,
        check=False,
        timeout=30,
    )
    (output / (prefix + "-gap.log")).write_bytes(gap.stdout + gap.stderr)
    if (
        gap.returncode != 1
        or b"asset miss has no registered generator" not in gap.stderr
    ):
        raise ValueError(
            "absent fine pages unexpectedly passed the source-free request"
        )
    full = probe(output, prefix + "-full", database, tile, core, 0, "build", commands)
    hit = probe(output, prefix + "-hit", database, tile, core, 0, "hit", commands)
    if int(full["rows"][-1]["generated"]) != 4:
        raise ValueError(
            "fine admission repeated already stored coarse page generation"
        )
    for level in range(6):
        pixels = np.fromfile(
            output / (prefix + "-hit") / f"mip-{level}.r8", dtype=np.uint8
        )
        expected = bake_page(tile, level, core_texels=core)
        np.testing.assert_array_equal(pixels.reshape(core + 2, core + 2), expected)
        if (
            pixels.tobytes()
            != (output / (prefix + "-full") / f"mip-{level}.r8").read_bytes()
        ):
            raise ValueError("stored ready page bytes changed in a fresh process")
    return {
        "tile": list(tile),
        "core_texels": core,
        "coarse": coarse,
        "full": full,
        "source_free_hit": hit,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--count", type=int, default=128)
    args = parser.parse_args()
    output = args.output.resolve()
    if not 1 <= args.count <= 4096 or not any(
        output.is_relative_to(p)
        for p in (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    ):
        parser.error("bounded native batch and private system-temp output required")
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
                "outshine_litter_page_probe",
                "--parallel",
                str(os.cpu_count() or 1),
            ],
            output,
            "build",
        )
    ]
    records = []
    batches = []
    for core in (64, 128):
        for case, tile in enumerate(
            ((0, 0), (-65, -64), (12345, -23456), (8388606, -8388607))
        ):
            records.append(compare(output, tile, core, case, commands))
        database = output / f"batch-{core}.sqlite"
        cold = probe(
            output,
            f"batch-{core}-build",
            database,
            (0, -10),
            core,
            0,
            "build",
            commands,
            args.count,
        )
        hits = [
            probe(
                output,
                f"batch-{core}-hit-{i}",
                database,
                (0, -10),
                core,
                0,
                "hit",
                commands,
                args.count,
            )
            for i in range(3)
        ]
        batches.append(
            {
                "core_texels": core,
                "count_per_mip": args.count,
                "build": cold,
                "hits": hits,
            }
        )
    files = [
        Path(__file__),
        Path(__file__).with_name("terrain_litter_page.py"),
        ROOT / "build/outshine_litter_page_probe",
    ]
    files += [
        ROOT / "src/generators/terrain" / name
        for name in (
            "LitterPageNoise.cpp",
            "LitterPageNoise.h",
            "PreparedLitterPage.cpp",
            "PreparedLitterPage.h",
        )
    ]
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "command": [sys.executable, *sys.argv],
        "commands": commands,
        "records": records,
        "batches": batches,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "limitations": [
            "Synchronous native asset-library timings, not engine worker/World/GPU frame-time acceptance.",
            "Resolution scopes include cache work; exports and source-free process startup are separate.",
            "Same batch count compares per-page costs; complete camera working sets differ between page sizes.",
        ],
    }
    (output / "receipt.json").write_text(json.dumps(result, indent=2) + "\n")
    print(output / "receipt.json")


if __name__ == "__main__":
    main()

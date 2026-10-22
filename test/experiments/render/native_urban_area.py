"""Compare a native coarse-area cook against Python for two explicit vector receipts."""

import argparse
import hashlib
import json
import math
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from native_tree_detail_world import checked, compile_unit, libraries
from native_urban_field import packages

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path[:0] = [str(ROOT / "lib/src"), str(ROOT / "test/experiments/buildings")]


def build(native, output):
    database = json.loads((native / "compile_commands.json").read_text())
    source = HERE / "urban_fields/UrbanAreaBake.cpp"
    obj, binary = output / "bake.o", output / "bake"
    command = compile_unit(database, native, "UrbanArea.cpp", source, obj)
    command.insert(1, f"-I{native / 'test/harness/shared'}")
    checked(["nice", "-n", "10", *command], native, output / "compile.log", 180)
    abi = [word for word in command if word.startswith("-stdlib=")]
    link = [
        command[0],
        *abi,
        str(obj),
        str(native / "build/liboutshine.a"),
        *libraries(native),
        "-o",
        str(binary),
    ]
    checked(["nice", "-n", "10", *link], native, output / "link.log", 180)
    return binary, {
        "compile": command,
        "link": link,
        "archive_sha256": hashlib.sha256(
            (native / "build/liboutshine.a").read_bytes()
        ).hexdigest(),
    }


def reference(receipts, output):
    import numpy as np
    from capture_urban_sources import capture
    from urban_patch_bake import bake

    document = capture(receipts[0])
    document["parents"] = [capture(receipts[1], parent=True)]
    started = time.perf_counter()
    product, manifest = bake(document)
    elapsed = time.perf_counter() - started
    z, x, y = document["tile"]
    span = 2 * math.pi * 6378137.0 / 2**z
    left, top = x * span - math.pi * 6378137.0, math.pi * 6378137.0 - y * span
    first = np.ceil(np.array([left, top - span]) / product.frame[3] - 0.5).astype(
        np.int64
    )
    stop = np.ceil(np.array([left + span, top]) / product.frame[3] - 0.5).astype(
        np.int64
    )
    columns, rows = stop - first
    if manifest["cells_considered"] == 0:
        columns, rows = 0, 0
    grid = np.zeros((rows, columns), np.uint8)
    for i, key in enumerate(product.chunks):
        origin = key.astype(np.int64) * product.side - first
        for row, column in np.argwhere(product.tiers[i] > 0):
            grid[origin[1] + row, origin[0] + column] = product.tiers[i, row, column]
    expected = grid[::-1].copy().tobytes()
    (output / "expected.u8").write_bytes(expected)
    (output / "reference-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n"
    )
    return expected, {
        "columns": int(columns),
        "rows": int(rows),
        "python_cook_seconds": elapsed,
        "occupied": int(np.count_nonzero(grid)),
    }


def main():
    from outshine_ref.places.sources import receipt_bytes

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipts", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native", type=Path, default=ROOT)
    args = parser.parse_args()
    output, native = args.output.resolve(), args.native.resolve()
    if not any(
        output.is_relative_to(root.resolve())
        for root in (Path(tempfile.gettempdir()), Path("/tmp"))
    ):
        parser.error("output must be in the system temp directory")
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty")
    document = json.loads(args.receipts.read_text())
    receipts = [document["owner"], *document.get("urban_parents", [])]
    if len(receipts) != 2:
        parser.error("this comparison requires one owner and one coarse urban parent")
    for i, receipt in enumerate(receipts):
        data = receipt_bytes(receipt, max_bytes=8 * 1024**2)
        path = output / f"input-{i}.pbf"
        path.write_bytes(data)
        receipt["path"] = str(path)
    expected, result = reference(receipts, output)
    binary, build_record = build(native, output)
    command = [
        str(binary),
        str(output / "input-0.pbf"),
        *map(str, receipts[0]["tile"]),
        str(output / "input-1.pbf"),
        *map(str, receipts[1]["tile"]),
        str(output / "native.u8"),
    ]
    parameters = {
        "version": "urban-area-ecef-v1",
        "recipe": [32, 0.75, 8],
        "sources": [
            {key: receipt[key] for key in ("url", "tile")} for receipt in receipts
        ],
    }
    request = hashlib.sha256(
        json.dumps(parameters, sort_keys=True).encode()
    ).hexdigest()
    database = output / "assets.sqlite"
    command.extend([str(database), request])
    checked(command, native, output / "native.log", 120)
    before = packages(database)
    for receipt in receipts:
        path = Path(receipt["path"])
        retained = path.with_suffix(".retained")
        path.rename(retained)
        receipt["retained_path"] = str(retained)
    warm = list(command)
    warm[1], warm[5], warm[9] = "-", "-", str(output / "warm.u8")
    checked(warm, native, output / "warm.log", 120)
    assert before == packages(database), "warm read rewrote a native package"
    assert (output / "warm.u8").read_bytes() == expected
    cache = next(
        row
        for row in (output / "warm.log").read_text().splitlines()
        if row.startswith("CACHE ")
    )
    counters = dict(word.split("=", 1) for word in cache.split()[1:])
    assert counters["cooks"] == "0"
    actual = (output / "native.u8").read_bytes()
    summary = next(
        row.split()[1:]
        for row in (output / "native.log").read_text().splitlines()
        if row.startswith("AREA ")
    )
    assert actual == expected, (
        "native categorical cells differ from the independent source cook"
    )
    assert list(map(int, summary[:2])) == [result["columns"], result["rows"]]
    assert int(summary[2]) == result["occupied"]
    result.update(
        {
            "commit": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=native, text=True
            ).strip(),
            "build": build_record,
            "command": command,
            "returncode": 0,
            "sources": receipts,
            "grid_bytes": len(actual),
            "grid_sha256": hashlib.sha256(actual).hexdigest(),
            "native_cook_ms": float(summary[5]),
            "native_lot_queries": int(summary[3]),
            "native_patch_queries": int(summary[4]),
            "all_cells_equal": True,
            "warm_command": warm,
            "warm_factory_calls": 0,
            "package_unchanged": True,
            "warm_total_ms": float(counters["total_ms"]),
            "scope": "source cook and source-free fresh-process cache hit; no GPU/world/240-km admission",
        }
    )
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: result[key]
                for key in (
                    "columns",
                    "rows",
                    "occupied",
                    "native_cook_ms",
                    "all_cells_equal",
                )
            }
        )
    )


if __name__ == "__main__":
    main()

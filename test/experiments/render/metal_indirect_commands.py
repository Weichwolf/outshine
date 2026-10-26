"""Compare Metal command preparation on explicit native GPU readbacks."""

import argparse
import hashlib
import json
import os
import platform
import shlex
import subprocess
import tempfile
from pathlib import Path
from statistics import median

import numpy as np

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            result.update(block)
    return result.hexdigest()


def prepare(source, output):
    jobs = np.fromfile(source / "jobs.bin", dtype="<u4").reshape(-1, 4)
    kept = np.fromfile(source / "kept.bin", dtype="<u4")
    args = np.fromfile(source / "arguments.bin", dtype="<u4").reshape(-1, 5)
    placements = np.fromfile(source / "placements.bin", dtype="<f4").reshape(-1, 36)
    view = np.fromfile(source / "uniform.bin", dtype="<f4")
    indices = np.memmap(source / "indices.bin", dtype="<u4", mode="r")
    vertices = np.memmap(source / "vertices.bin", dtype="<f4", mode="r").reshape(-1, 3)
    if len(view) != 64 or len(jobs) != len(kept) or not len(args):
        raise ValueError("incomplete cull view, jobs or arguments")
    if np.any(jobs[:, 1] >= len(args)) or np.any((kept != 0) & (kept != jobs[:, 3])):
        raise ValueError("invalid batch references or partial kept groups")
    sums = np.zeros(len(args), dtype=np.uint64)
    np.add.at(sums, jobs[:, 1], kept)
    if not np.array_equal(sums, args[:, 0]):
        raise ValueError("kept groups disagree with actual GPU argument counts")
    drawn = (args[:, 0] > 0) & (args[:, 1] > 0)
    if not drawn.any() or np.any(args[:, 0] % 3):
        raise ValueError("input must contain whole drawn triangles")
    if np.any(args[drawn, 4].astype(np.uint64) + args[drawn, 1] > len(placements)):
        raise ValueError("instance models exceed placement storage")
    if not np.isfinite(placements[:, :16]).all() or not np.isfinite(view).all():
        raise ValueError("model matrices or cull view are not finite")
    spans = args[args[:, 0] > 0][:, [2, 0]].astype(np.uint64)
    spans = spans[np.argsort(spans[:, 0])]
    if np.any(spans[1:, 0] < spans[:-1].sum(axis=1)):
        raise ValueError("compacted argument spans overlap")
    end = int(np.max(args[:, 2].astype(np.uint64) + args[:, 0]))
    if end > len(indices):
        raise ValueError("compacted argument span exceeds original index capacity")
    output.mkdir(parents=True, exist_ok=True)
    compacted = np.memmap(
        output / "draw-indices.bin", dtype="<u4", mode="w+", shape=end
    )
    compacted[:] = 0
    cursor = np.zeros(len(args), dtype=np.uint64)
    for job, count in zip(jobs, kept):
        if not count:
            continue
        row, first = int(job[1]), int(job[2])
        count = int(count)
        if first + count > len(indices):
            raise ValueError("source job exceeds original index storage")
        values = indices[first : first + count]
        effective = values.astype(np.int64) + int(args[row, 3].view(np.int32))
        if effective.min() < 0 or effective.max() >= len(vertices):
            raise ValueError("signed base vertex produces an invalid vertex reference")
        if not np.isfinite(vertices[effective]).all():
            raise ValueError("referenced positions are not finite")
        at = int(args[row, 2] + cursor[row])
        compacted[at : at + count] = values
        cursor[row] += count
    compacted.flush()
    args.tofile(output / "arguments.bin")
    placements[:, :16].copy().tofile(output / "models.bin")
    np.r_[view[32:48], view[24:28]].astype("<f4").tofile(output / "frame.bin")
    manifest = {
        "source": str(source),
        "rows": len(args),
        "nonempty_rows": int(drawn.sum()),
        "triangles": int(np.sum(args[:, 0].astype(np.uint64) * args[:, 1])) // 3,
        "vertices_path": str(source / "vertices.bin"),
        "source_sha256": {
            name: digest(source / name)
            for name in (
                "jobs.bin",
                "kept.bin",
                "arguments.bin",
                "placements.bin",
                "uniform.bin",
                "vertices.bin",
                "indices.bin",
            )
        },
    }
    (output / "input.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def run(command, cwd, log, environment=None):
    with log.open("w") as stream:
        result = subprocess.run(
            command,
            cwd=cwd,
            env=environment,
            stdout=stream,
            stderr=subprocess.STDOUT,
            timeout=180,
            check=False,
        )
    if result.returncode:
        raise RuntimeError(
            f"{shlex.join(command)} exited {result.returncode}; see {log}"
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--compile-db", type=Path, default=ROOT / "compile_commands.json"
    )
    parser.add_argument("--rounds", type=int, default=32)
    args = parser.parse_args()
    output = args.output.resolve()
    roots = (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    if platform.system() != "Darwin" or not 1 <= args.rounds <= 1000:
        parser.error("requires Metal/macOS and 1..1000 rounds")
    if not any(output.is_relative_to(p) for p in roots) or output.is_relative_to(
        ROOT.resolve()
    ):
        parser.error("output must be in system temp, outside the checkout")
    if output.exists() and any(output.iterdir()):
        parser.error("output must be empty")
    prepare(args.input.resolve(), output)
    records = json.loads(args.compile_db.read_text())
    row = next(
        (r for r in records if r["file"].endswith("/metal/MetalCommands.mm")), None
    )
    if row is None:
        parser.error("--compile-db must contain native MetalCommands.mm")
    command = row.get("arguments") or shlex.split(row["command"])
    source = Path(__file__).with_suffix(".mm")
    shader = Path(__file__).with_suffix(".metal")
    command[command.index(row["file"])] = str(source)
    command[command.index("-o") + 1] = str(output / "probe.o")
    compile_command = ["nice", "-n", "10", *command]
    run(compile_command, row["directory"], output / "compile.log")
    link = [
        command[0],
        str(output / "probe.o"),
        "-framework",
        "Metal",
        "-framework",
        "Foundation",
        "-o",
        str(output / "probe"),
    ]
    run(["nice", "-n", "10", *link], row["directory"], output / "link.log")
    steps = []
    for name, rounds, validation in (
        ("validation", 2, True),
        ("cost", args.rounds, False),
    ):
        call = [
            str(output / "probe"),
            str(output),
            str(shader),
            str(rounds),
            str(output / (name + ".json")),
        ]
        run(
            call,
            ROOT,
            output / (name + ".log"),
            {**os.environ, "MTL_DEBUG_LAYER": str(int(validation))},
        )
        steps.append({"command": call, "status": 0, "metal_validation": validation})
    receipt = {
        "compile": compile_command,
        "compile_status": 0,
        "link": link,
        "link_status": 0,
        "steps": steps,
        "terminal": True,
        "binary_sha256": digest(output / "probe"),
        "host": {"system": platform.platform(), "machine": platform.machine()},
        "compile_db_sha256": digest(args.compile_db),
        "source_sha256": {str(p): digest(p) for p in (Path(__file__), source, shader)},
    }
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    for mode in json.loads((output / "cost.json").read_text())["modes"]:
        print(
            mode["name"],
            {
                key.replace("_ns", "_ms"): median(s[key] for s in mode["samples"]) / 1e6
                for key in ("cpu_encode_ns", "gpu_ns")
            },
        )


if __name__ == "__main__":
    main()

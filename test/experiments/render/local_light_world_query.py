"""Expose FP32 light-index false negatives and check a conservative query fallback."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from export_local_light_native import inputs
from local_light_inputs import ROOT
from local_light_world import build_world, cell_positions


def record(light, eye, width, method, receiver, index_offset):
    grid = build_world(light, width, 16 * 1024**2, np.zeros(3))
    words, _ = grid.pack(method)
    eye_cell, eye_local = cell_positions(eye[None], 128)
    data = np.zeros((6, 4), dtype="<f4")
    data.view("<i4")[0] = [*grid.light_cells[0], index_offset]
    data[1] = grid.local_lights[0, 1]
    data.view("<i4")[2] = [*eye_cell[0], width]
    data[3] = [*eye_local[0], 1 / float(light[0, 1, 3])]
    data[4, :3] = receiver
    data.view("<i4")[5] = [*np.floor(eye / width).astype(np.int32), method == "sorted"]
    return data, words


def prepare(root):
    cases, tables, labels = [], [], []
    offset = 0
    for width in (16, 32, 64, 128):
        for method in ("hash", "sorted"):
            for axis in range(3):
                light = np.zeros((1, 4, 4), dtype="<f4")
                light[0, 0] = [1, 1, 1, 1]
                light[0, 1] = [7.125, 11.125, 13.125, 10000]
                light[0, 1, axis] = width - 0.0005
                eye = np.zeros(3)
                eye[axis] = -19968
                p = (light[0, 1, :3].astype(float) - eye).astype(np.float32)
                p[(axis + 1) % 3] -= np.float32(0.00005)
                data, words = record(light, eye, width, method, p, offset)
                cases.append(data)
                tables.append(words)
                labels.append(
                    {
                        "kind": "boundary-counter",
                        "width": width,
                        "method": method,
                        "axis": axis,
                    }
                )
                offset += len(words)
    _, frame, packed, _, _ = inputs("dense", True, "street")
    finite = np.flatnonzero((packed[:, 1, 3] > 0) & (packed[:, 0, 3] > 0.5))[:256]
    for width in (16, 32, 64, 128):
        for method in ("hash", "sorted"):
            for source_id in finite:
                light = packed[source_id : source_id + 1].copy()
                relative = (light[0, 1, :3].astype(float) - frame.eye).astype(
                    np.float32
                )
                relative[1] += np.float32(0.25 / float(light[0, 1, 3]))
                data, words = record(light, frame.eye, width, method, relative, offset)
                cases.append(data)
                tables.append(words)
                labels.append(
                    {
                        "kind": "dense-source",
                        "source_id": int(source_id),
                        "width": width,
                        "method": method,
                    }
                )
                offset += len(words)
    for sign in (-1, 1):
        eye = np.array(
            [sign * (2**31 - (2 if sign > 0 else 0)) * 16 + 4, 0, 0], dtype=float
        )
        light = np.zeros((1, 4, 4), dtype="<f4")
        light[0, 0] = [1, 1, 1, 1]
        light[0, 1] = [*eye, 1]
        data, words = record(light, eye, 16, "hash", [sign * 72, 7, 11], offset)
        data[4, 3] = 1  # The invalid old arithmetic is deliberately not executed.
        cases.append(data)
        tables.append(words)
        labels.append({"kind": "key-overflow", "sign": sign})
        offset += len(words)
    np.asarray(cases, dtype="<f4").tofile(root / "queries.bin")
    np.concatenate(tables).astype("<u4").tofile(root / "index.bin")
    (root / "labels.json").write_text(json.dumps(labels, indent=2) + "\n")
    return labels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--shader", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("external output required")
    args.output.mkdir(parents=True, exist_ok=False)
    labels = prepare(args.output)
    argv = [
        str(args.binary),
        str(args.shader),
        str(args.output / "queries.bin"),
        str(args.output / "index.bin"),
        str(args.output / "answers.bin"),
    ]
    result = subprocess.run(
        argv, capture_output=True, text=True, timeout=30, check=False
    )
    (args.output / "stdout.log").write_text(result.stdout)
    (args.output / "stderr.log").write_text(result.stderr)
    if result.returncode or result.stderr:
        raise AssertionError((result.returncode, result.stderr))
    answers = np.fromfile(args.output / "answers.bin", dtype="<u4").reshape(-1, 4)
    if (
        len(answers) != len(labels)
        or np.any(answers[:, [0, 1, 3]] > 1)
        or np.any(answers[:, 2] > 8)
    ):
        raise AssertionError("complete boolean GPU answers required")
    supported = answers[:, 0] != 0
    old_missing = supported & (answers[:, 1] == 0)
    new_missing = supported & (answers[:, 3] == 0)
    counters = np.array([x["kind"] == "boundary-counter" for x in labels])
    overflow = np.array([x["kind"] == "key-overflow" for x in labels])
    ordinary = ~(counters | overflow)
    if not np.all(supported[counters]) or not np.all(old_missing[counters]):
        raise AssertionError("every prescribed old-query counter must really fail")
    if not np.all(answers[overflow, 2] == 0):
        raise AssertionError("checked signed-key overflow must request the full sum")
    if np.any(new_missing):
        raise AssertionError("candidate query loses an actual contributing emitter")
    report = {
        "argv": argv,
        "exit": result.returncode,
        "queries": len(labels),
        "full_sum_supported": int(supported.sum()),
        "old_false_negatives": int(old_missing.sum()),
        "candidate_false_negatives": int(new_missing.sum()),
        "boundary_counters": int(counters.sum()),
        "key_overflows": int(overflow.sum()),
        "ordinary_full_fallbacks": int(np.count_nonzero(answers[ordinary, 2] == 0)),
        "ordinary_multicell_queries": int(np.count_nonzero(answers[ordinary, 2] > 1)),
        "ordinary_queries": int(ordinary.sum()),
        "input_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in args.output.glob("*.bin")
        },
        "scope": "Actual native query/support control; no complete BRDF, raster image, runtime, formal FP32 proof or frame-budget admission",
    }
    (args.output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

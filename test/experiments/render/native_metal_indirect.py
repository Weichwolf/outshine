"""Compare native Metal draw loops and GPU-generated indirect command buffers."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path
from statistics import median

from experiment_output import create_output

ROOT = Path(__file__).resolve().parents[3]
SOURCE = Path(__file__).with_suffix(".mm")
MODES = ("draw_loop", "icb_pipeline_draw_loop", "gpu_icb", "optimized_gpu_icb")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(command, output, label, receipt, environment=None, expected=0):
    row = {"label": label, "command": command, "status": None}
    receipt["commands"].append(row)
    with (output / f"{label}.log").open("w") as log:
        result = subprocess.run(
            command,
            cwd=ROOT,
            env=environment,
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=180,
            check=False,
        )
    row["status"] = result.returncode
    if result.returncode != expected:
        raise RuntimeError(f"{label}: exit {result.returncode}; inspect {label}.log")


def compile_probe(checkout, output, receipt):
    database = json.loads((checkout / "compile_commands.json").read_text())
    entry = next(
        row for row in database if row["file"].endswith("/MetalIndexedDraw.mm")
    )
    arguments = entry.get("arguments") or shlex.split(entry["command"])
    flags, index = [], 0
    while index < len(arguments):
        word = arguments[index]
        if word == "-o":
            index += 2
            continue
        if word != "-c" and word != entry["file"]:
            flags.append(word)
        index += 1
    binary, obj = output / "probe", output / "probe.o"
    command = [*flags, "-c", str(SOURCE), "-o", str(obj)]
    (output / "compile_commands.json").write_text(
        json.dumps(
            [{"directory": entry["directory"], "file": str(SOURCE), "arguments": command}],
            indent=2,
        )
    )
    execute(command, output, "compile", receipt)
    execute(
        [flags[0], str(obj), "-framework", "Foundation", "-framework", "Metal", "-o", str(binary)],
        output,
        "link",
        receipt,
    )
    receipt["binary_sha256"] = digest(binary)
    return binary


def summarize(result):
    samples = result["samples"]
    expected = 3 * 2 * 7 * 4 * len(MODES)
    if result["pixel_controls"] != 3 * 2 * 7 * 4 or len(samples) != expected:
        raise ValueError("incomplete native controls or samples")
    groups = {}
    for sample in samples:
        key = sample["count"], sample["sections"], sample["mode"]
        groups.setdefault(key, []).append(sample)
    return [
        {
            "records": count,
            "requested_sections": sections,
            "mode": MODES[mode],
            "samples": len(rows),
            "encode_median_ms": median(row["encode_ms"] for row in rows),
            "submission_gpu_median_ms": median(row["gpu_ms"] for row in rows),
        }
        for (count, sections, mode), rows in sorted(groups.items())
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    options = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("this experiment requires native Metal on macOS")
    create_output(options.output)
    output, checkout = options.output.resolve(), options.checkout.resolve()
    receipt = {
        "status": "running",
        "commands": [],
        "source_sha256": digest(SOURCE),
        "launcher_sha256": digest(Path(__file__)),
        "source_revision": subprocess.check_output(
            ["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True
        ).strip(),
        "scope": "synthetic native submission; no World, PBR, p99 or full memory claim",
    }
    try:
        binary = compile_probe(checkout, output, receipt)
        summary = {}
        for label, validation in (("validated", "1"), ("profiled", "0")):
            environment = dict(os.environ, MTL_DEBUG_LAYER=validation)
            receipt[label] = {"MTL_DEBUG_LAYER": validation}
            path = output / f"{label}.json"
            execute([str(binary), str(path)], output, label, receipt, environment)
            summary[label] = summarize(json.loads(path.read_text()))
        environment = dict(os.environ, MTL_DEBUG_LAYER="1")
        execute(
            [str(binary), str(output / "negative.json"), "--omit-reset"],
            output,
            "negative",
            receipt,
            environment,
            expected=1,
        )
        if "direct/ICB pixels differ" not in (output / "negative.log").read_text():
            raise ValueError("the missing-reset control did not expose stale GPU commands")
        (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        receipt["status"] = "passed"
        print(f"native Metal: 168 pixel controls/profile; missing-reset control red; {output}")
    except Exception as error:
        receipt.update(status="failed", error=str(error))
        raise
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()

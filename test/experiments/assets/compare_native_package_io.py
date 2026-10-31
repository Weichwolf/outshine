"""Compare native cache IO on immutable package snapshots in fresh processes."""

import argparse
import hashlib
import json
import os
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def invoke(binary, arguments, folder):
    load = os.getloadavg()
    began = time.monotonic()
    with (folder.with_suffix(".out")).open("w") as output, (
        folder.with_suffix(".err")
    ).open("w") as error:
        process = subprocess.Popen([str(binary), *arguments], stdout=output, stderr=error)
        _, status, usage = os.wait4(process.pid, 0)
        process.returncode = os.waitstatus_to_exitcode(status)
    result = {
        "returncode": process.returncode,
        "elapsed_s": time.monotonic() - began,
        "process_peak_rss_bytes": usage.ru_maxrss * (1 if sys.platform == "darwin" else 1024),
        "user_s": usage.ru_utime,
        "system_s": usage.ru_stime,
        "output_blocks": usage.ru_oublock,
        "input_blocks": usage.ru_inblock,
        "loadavg": load,
    }
    if process.returncode:
        raise RuntimeError(f"native cache IO failed: {folder}")
    result["native"] = json.loads(folder.with_suffix(".out").read_text())
    return result


def compare(options):
    if options.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        raise ValueError("private outputs must stay outside the checkout")
    options.output.mkdir(parents=True, exist_ok=False)
    inputs = json.loads(options.inputs.read_text())["inputs"]
    names = [item["name"] for item in inputs]
    if len(set(names)) != len(names) or any(not re.fullmatch(r"[\w-]+", name) for name in names):
        raise ValueError("package names must be unique plain path components")
    for item in inputs:
        if file_sha256(Path(item["input"])) != item["input_sha256"]:
            raise ValueError(f"changed package snapshot: {item['name']}")
    binaries = {"before": options.before.resolve(), "after": options.after.resolve()}
    rows = []
    for item in inputs:
        for pair in range(options.pairs):
            order = ("before", "after") if pair % 2 == 0 else ("after", "before")
            for variant in order:
                folder = options.output / f"{item['name']}-{pair}-{variant}"
                folder.mkdir()
                cache = folder / "assets.sqlite"
                written = invoke(
                    binaries[variant],
                    ["write", item["input"], item["key"], str(cache)],
                    folder / "write",
                )
                write = written["native"]
                if (
                    write["package"] != item["package"]
                    or write["native_bytes"] != item["native_bytes"]
                    or write["records"] != item["records"]
                ):
                    raise ValueError(f"changed native package or members: {folder}")
                loaded = invoke(
                    binaries[variant],
                    ["read", str(cache), item["key"], write["slice_sha256"]],
                    folder / "read",
                )
                read = loaded["native"]
                if (
                    read["package"] != write["package"]
                    or read["slice_sha256"] != write["slice_sha256"]
                    or read["slice_bytes"] != item["slice_bytes"]
                ):
                    raise ValueError(f"fresh process returned different native bytes: {folder}")
                rows.append({
                    "name": item["name"], "pair": pair, "variant": variant,
                    "write": written, "read": loaded,
                })
                (options.output / "samples.json").write_text(json.dumps(rows, indent=2) + "\n")
            last = rows[-2:]
            if last[0]["write"]["native"]["stored_bytes"] != last[1]["write"]["native"]["stored_bytes"]:
                raise ValueError("the two procedures produced different compressed package sizes")
        print(item["name"], "complete", flush=True)
    summary = []
    for item in inputs:
        record = {key: item[key] for key in ("name", "kind", "native_bytes", "records")}
        for variant in binaries:
            selected = [
                row for row in rows
                if row["name"] == item["name"] and row["variant"] == variant
            ]
            record[variant] = {}
            for mode, keys in {
                "write": ("open_ms", "prepare_ms", "publish_ms", "close_ms", "sqlite_extra_bytes", "stored_bytes"),
                "read": ("open_ms", "plan_ms", "read_ms", "close_ms", "buffer_bytes_most"),
            }.items():
                record[variant][mode] = {
                    key: statistics.median(row[mode]["native"][key] for row in selected) for key in keys
                }
                record[variant][mode]["process_peak_rss_bytes"] = statistics.median(
                    row[mode]["process_peak_rss_bytes"] for row in selected
                )
        summary.append(record)
    result = {
        "binaries": {
            label: {"path": str(binary), "sha256": file_sha256(binary),
                    "revision": options.before_revision if label == "before" else options.after_revision}
            for label, binary in binaries.items()
        },
        "inputs_manifest_sha256": hashlib.sha256(options.inputs.read_bytes()).hexdigest(),
        "pairs": options.pairs,
        "scope": (
            "Canonical native package records, no request bindings; preparation, publication "
            "and a fresh offline direct-key hit. Input snapshot decode/validation is outside "
            "timed write phases. No generator/source acquisition, typed consumer, World, GPU "
            "or frame measurement; filesystem cache is not cleared; wait4 process peaks may "
            "include pre-exec inheritance."
        ),
        "samples": rows,
        "medians": summary,
    }
    (options.output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", required=True, type=Path)
    parser.add_argument("--before", required=True, type=Path)
    parser.add_argument("--after", required=True, type=Path)
    parser.add_argument("--before-revision", required=True)
    parser.add_argument("--after-revision", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--pairs", type=int, default=7)
    options = parser.parse_args()
    if options.pairs < 3:
        parser.error("at least three alternating pairs are required")
    try:
        return compare(options)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f"native package comparison: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())

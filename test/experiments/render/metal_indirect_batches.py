"""Compare repeated Metal indirect calls with immutable full and compact ICBs."""

import argparse
import hashlib
import json
import shlex
import statistics
from pathlib import Path

from metal_encoder_profile import ROOT, run, temporary
from PIL import Image


def compile_probe(output):
    database = json.loads((ROOT / "compile_commands.json").read_text())
    row = next(
        item for item in database if item["file"].endswith("/MetalEncoderProfile.mm")
    )
    if Path(row["directory"]).resolve() != ROOT:
        raise ValueError("build a compilation database for this checkout first")
    flags = (row.get("arguments") or shlex.split(row["command"])).copy()
    flags.remove(row["file"])
    flags.remove("-c")
    offset = flags.index("-o")
    del flags[offset : offset + 2]
    source = ROOT / "test/harness/render/MetalIndirectBatchProbe.mm"
    shader = source.with_suffix(".metal")
    obj, binary = output / "probe.o", output / "probe"
    compile_command = [*flags, "-c", str(source), "-o", str(obj)]
    link = [
        flags[0],
        str(obj),
        "-framework",
        "Foundation",
        "-framework",
        "Metal",
        "-o",
        str(binary),
    ]
    for label, command in (("compile", compile_command), ("link", link)):
        run(["nice", "-n", "10", *command], output, label)
    (output / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "directory": str(ROOT),
                    "file": str(source),
                    "arguments": compile_command,
                }
            ]
        )
    )
    return (
        binary,
        shader,
        {
            "compile": compile_command,
            "link": link,
            "compile_status": 0,
            "link_status": 0,
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "shader_sha256": hashlib.sha256(shader.read_bytes()).hexdigest(),
            "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        },
    )


def summarize(document):
    rows, result = document["rows"], []
    if len(rows) != document["rounds"] * 16:
        raise ValueError("incomplete comparison")
    for percent in (0, 1, 10, 100):
        subset = [row for row in rows if row["percent"] == percent]
        if len({row["fnv1a64"] for row in subset}) != 1:
            raise ValueError("mismatched images")
        for mode in ("loop", "icb", "compact_icb", "regular_loop"):
            group = [row for row in subset if row["mode"] == mode]
            if sorted(row["round"] for row in group) != list(range(document["rounds"])):
                raise ValueError("missing or duplicate rounds")
            if any(row["colored_pixels"] != row["active"] * 28 for row in group):
                raise ValueError("incorrect raster coverage")
            summary = {
                key: group[0][key]
                for key in (
                    "percent",
                    "active",
                    "slots",
                    "mode",
                    "build_ns",
                    "full_icb_bytes",
                    "compact_icb_bytes",
                    "argument_bytes",
                    "argument_allocated_bytes",
                    "fnv1a64",
                    "colored_pixels",
                )
            }
            for key in ("encode_ns", "commit_ns", "host_ns", "gpu_ns"):
                values = [row[key] for row in group]
                summary[key] = {
                    "median": statistics.median(values),
                    "max": max(values),
                    "first": values[0],
                    "mean": statistics.mean(values),
                }
            result.append(summary)
    return {"device": document["device"], "rows": result}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = temporary(args.output)
    binary, shader, receipt = compile_probe(output)
    for label, enabled in (("validated", "1"), ("ordinary", "0")):
        path = output / (label + ".json")
        run(
            ["env", "MTL_DEBUG_LAYER=" + enabled, str(binary), str(shader), str(path)],
            output,
            label,
        )
        summary = summarize(json.loads(path.read_text()))
        raw = path.with_suffix(".json.rgba")
        Image.frombytes("RGBA", (1280, 720), raw.read_bytes()).save(
            output / (label + ".png")
        )
        raw.unlink()
        (output / (label + "-summary.json")).write_text(json.dumps(summary, indent=2))
        receipt[label] = {"status": 0, "samples": 384}
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2))
    print(f"384 byte-matched renders per validation mode; costs: {output}")


if __name__ == "__main__":
    main()

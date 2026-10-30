"""Compare shared native resident captures with frozen whole original unions."""

import argparse
import copy
import json
import subprocess
from pathlib import Path

import numpy as np
from compare_field_external_depth import panel, reduced
from experiment_output import create_output
from export_native_field_opponents import ROOT, digest, document, execute, read_samples
from field_native_fixture import DATA, read_fixture


def compare(folder, request, samples, base, outer):
    rows = []
    for query in request["queries"]:
        name = query["name"]
        rgba, depth = read_samples(folder, name, query["capture"])
        a, b = samples[base, name], samples[outer, name]
        original = samples["union-" + outer, name]
        expected = np.where((a.reverse_z >= b.reverse_z)[..., None], a.rgba, b.rgba)
        wanted_depth = np.maximum(a.reverse_z, b.reverse_z)
        row = {
            "source": base,
            "outside": outer,
            "query": name,
            "point_colour_equal": bool(np.array_equal(rgba, expected)),
            "point_depth_equal": bool(np.array_equal(depth, wanted_depth)),
            "original_depth_equal": bool(np.array_equal(depth, original.reverse_z)),
            "original_coverage_equal": bool(
                np.array_equal(rgba[..., 3], original.rgba[..., 3])
            ),
            "original_rgb_max_abs": float(
                np.max(np.abs(rgba[..., :3] - original.rgba[..., :3]))
            ),
        }
        panel(
            folder / (name + ".png"),
            reduced(original.rgba),
            reduced(rgba),
            reduced(expected),
            labels=(
                "Whole original union",
                "One native resident pass",
                "Registered points",
                "Registered residual, x4",
            ),
        )
        rows.append(row)
    return rows


def run(args):
    args.binary = args.binary.resolve()
    args.reference = args.reference.resolve()
    args.inputs = args.inputs.resolve()
    args.output = args.output.resolve()
    create_output(args.output)
    manifest, status, samples, _work = read_fixture(args.inputs)
    reference = document(args.reference / "manifest.json")
    if (
        reference["controls"] != manifest["controls"]
        or reference["original"] != manifest["original"]
    ):
        raise ValueError(
            "The explicit reference geometry must belong to the frozen fixture"
        )
    rows = []
    for base in ("original", "stored"):
        for outer in ("terrain", "external"):
            request = copy.deepcopy(document(args.reference / base / "input.json"))
            request["directory"] = str((args.reference / "original").resolve())
            request["additional_sources"] = [
                {"directory": str((args.reference / outer).resolve())}
            ]
            folder = args.output / (base + "-" + outer)
            result = execute(args.binary, folder, request)
            if (
                result["render_device_creations"] != 1
                or len(result["additional_sources"]) != 1
            ):
                raise ValueError(
                    "The union must use one device and its additional resident source"
                )
            rows.extend(compare(folder, request, samples, base, outer))
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "native_binary_sha256": digest(args.binary),
        "reference_manifest_sha256": digest(args.reference / "manifest.json"),
        "dirty_sources": bool(
            subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT)
        ),
        "reference_status": status,
        "cases": rows,
        "scope": "Finite registered native opaque captures before filtering. Equal native D32F uses original Greater draw order; no uncertain interval, World, pose or device-budget authority.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    if not all(
        all(
            row[key]
            for key in (
                "point_colour_equal",
                "point_depth_equal",
                "original_depth_equal",
                "original_coverage_equal",
            )
        )
        for row in rows
    ):
        raise ValueError(
            "Native resident union differs from its frozen registered operands"
        )
    print(
        json.dumps(
            {
                "cases": len(rows),
                "registered_points_equal": True,
                "original_depth_coverage_equal": True,
                "known_defects": status["known_defects"],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

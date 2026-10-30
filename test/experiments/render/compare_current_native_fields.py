"""Compare the real #1805 source fields with both direct native original unions."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from compare_field_external_depth import panel, reduced
from field_external_depth import compose_registered
from field_native_fixture import DATA, QUERIES, read_fixture


def fixture_compose(a, b):
    world = compose_registered(a, b, wanted_a=a.binding, wanted_b=b.binding)
    if world.rgba is not None or world.reason != "unknown_world_revision":
        raise ValueError("Native fixture was silently admitted as a live World")
    result = compose_registered(
        a, b, wanted_a=a.binding, wanted_b=b.binding, diagnostic_fixture=True
    )
    if result.rgba is None or result.domain != "fixture":
        raise ValueError(
            "Registered native fixture point composition failed: " + result.reason
        )
    return result


def compare(samples, query, role, output):
    original, stored, outside = (
        samples[k, query] for k in ("original", "stored", role)
    )
    direct = samples["union-" + role, query]
    failures = []
    if not np.array_equal(original.reverse_z, stored.reverse_z) or not np.array_equal(
        original.rgba[:, :, 3], stored.rgba[:, :, 3]
    ):
        failures.append("stored_original_coverage_or_depth")
    reference = fixture_compose(original, outside)
    if not np.array_equal(reference.rgba, direct.rgba) or not np.array_equal(
        reference.reverse_z, direct.reverse_z
    ):
        failures.append("independent_native_original_union")
    candidate = fixture_compose(stored, outside)
    if not np.array_equal(candidate.reverse_z, direct.reverse_z) or not np.array_equal(
        candidate.rgba[:, :, 3], direct.rgba[:, :, 3]
    ):
        failures.append("stored_native_visibility_or_coverage")
    source_delta = abs(stored.rgba[:, :, :3].astype(float) - original.rgba[:, :, :3])
    actual, target = reduced(candidate.rgba), reduced(direct.rgba)
    delta = abs(actual[:, :, :3] - target[:, :, :3])
    if delta.sum() > source_delta.sum() / 16 + 1e-9 or delta.max() > source_delta.max():
        failures.append("finite_source_error_contraction")
    af, bf = reduced(stored.rgba), reduced(outside.rgba)
    wrong = af + (1 - af[:, :, 3:]) * bf
    original_mean = reduced(original.rgba)
    wrong_original = original_mean + (1 - original_mean[:, :, 3:]) * bf
    name = query + "-" + role
    panel(
        output / (name + ".png"),
        target,
        actual,
        wrong,
        labels=[
            "Direct native original union",
            "Stored native fields, then filter",
            "Independent filtered over",
            "Incorrect-over error, x4",
        ],
    )
    common = (original.reverse_z > 0) & (outside.reverse_z > 0)
    return {
        "query": query,
        "outside": role,
        "fixture_context": original.binding.fixture_revision,
        "world_admission": "parent:unknown_world_revision",
        "native_original_union_rgba_and_depth_exact": "independent_native_original_union"
        not in failures,
        "stored_depth_and_coverage_exact": "stored_native_visibility_or_coverage"
        not in failures,
        "failures": failures,
        "source_first_common": int(
            np.count_nonzero(common & (original.reverse_z > outside.reverse_z))
        ),
        "outside_first_common": int(
            np.count_nonzero(common & (original.reverse_z < outside.reverse_z))
        ),
        "source_rgb_max_abs": float(source_delta.max()),
        "source_rgb_l1": float(source_delta.sum()),
        "filtered_rgb_max_abs": float(delta.max()),
        "filtered_rgb_l1": float(delta.sum()),
        "filtered_rgb_l1_upper_bound": float(source_delta.sum() / 16),
        "independent_over_rgb_max_abs": float(
            abs(wrong[:, :, :3] - target[:, :, :3]).max()
        ),
        "original_independent_over_rgb_max_abs": float(
            abs(wrong_original[:, :, :3] - target[:, :, :3]).max()
        ),
        "background_rgb_component_ranges": np.ptp(outside.rgba[:, :, :3], axis=(0, 1))
        .astype(float)
        .tolist(),
    }


def run(args):
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh explicit system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    manifest, status, samples, work = read_fixture(args.inputs)
    rows = [
        compare(samples, query, role, args.output)
        for query in QUERIES
        for role in ("terrain", "external")
    ]
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "native_commit": manifest["native_commit"],
        "reference_status": status,
        "reference_failures": manifest["reference_failures"],
        "work": work,
        "cases": rows,
        "illumination_binding": "actual byte-bound executed capture/full PBR camera/light operands identify diagnostic cohorts; query labels are excluded; live World/light revisions remain unknown",
        "scope": "Consumes actual A18/Metal MaterialFieldInput atlas and independent original/outer/union captures from #1805. Fixed finite captured points and PBR observers; no new GPU execution. Known #1806 repeatability failure is preserved; no general error, pose, live World or device-budget claim.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "cases": len(rows),
                "known_defects": status["known_defects"],
                "world_revision": status["world_revision"],
                "physical_arrays": work["physical_arrays"],
            }
        )
    )
    if any(row["failures"] for row in rows):
        raise ValueError(
            "Native comparison failed; complete numerical diagnostics retained"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())

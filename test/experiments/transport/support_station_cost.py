"""One isolated allocation probe of a saved-input procedural support plan."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import memray
import numpy as np
from bridge_support_contacts import ContactSurface
from bridge_support_plan import plan_supports, stations
from camera_height_inputs import digest
from rail_instance_cost import profile_note
from wien_support_experiment import packed, read_inputs


def measure(args):
    if args.output.exists():
        raise ValueError("fresh allocation probe directory required")
    args.output.mkdir(parents=True)
    sources, ground, deck, excluded, _world, _before, _fixture = read_inputs(
        args.network, args.reference_root
    )
    lower, upper = ContactSurface(ground), ContactSurface(deck)
    candidates = stations([r for r in sources if r["role"] == "deck"])
    began = time.perf_counter()
    with memray.Tracker(
        str(args.output / "allocations.bin"),
        trace_python_allocators=True,
        native_traces=True,
    ):
        products, contacts, _shapes, report = plan_supports(
            candidates, lower, upper, excluded, method=args.method
        )
    elapsed = time.perf_counter() - began
    expected_path = args.products / (args.method + ".npz")
    expected_note = json.loads((args.products / "comparison.json").read_text())[
        "methods"
    ][args.method]
    if digest(expected_path) != expected_note["mesh_sha256"]:
        raise ValueError("pinned comparison products changed")
    actual = packed(products, contacts)
    with np.load(expected_path) as expected:
        if set(expected.files) != set(actual) or any(
            not np.array_equal(expected[k], v) for k, v in actual.items()
        ):
            raise ValueError(
                "instrumented plan differs from the accepted saved geometry"
            )
    report.update(
        commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        profile=profile_note(str(args.output / "allocations.bin")),
        total_instrumented_wall_s=elapsed,
        worker_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        original_products_sha256=digest(expected_path),
        emitted_mesh_array_bytes=sum(
            p.vertices.nbytes + p.triangles.nbytes for p in products
        ),
        stored_cap_array_bytes=sum(
            m.vertices.nbytes + m.triangles.nbytes for pair in contacts for m in pair
        ),
        identical_saved_geometry=True,
        scope="One cold planning call after input decode and both contact-index builds. Python/native allocations observed; cumulative bytes are not live memory. Instrumented timing is separate from normal generation. Output packing and independent verification are excluded; peak RSS includes loaded original scene and probe exports. Work source-face/arrangement totals count accepted prisms only; all trials and accepted-pier pair checks are counted. Array payloads are known ownership/copies, not a complete internal copy count.",
    )
    (args.output / "measurement.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "commit",
                    "method",
                    "profile",
                    "total_instrumented_wall_s",
                    "worker_peak_rss_bytes",
                    "emitted_mesh_array_bytes",
                    "stored_cap_array_bytes",
                    "identical_saved_geometry",
                )
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--network", type=Path, required=True)
    parser.add_argument("--reference-root", type=Path, required=True)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--method", choices=("fixed", "shifted"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    measure(args)

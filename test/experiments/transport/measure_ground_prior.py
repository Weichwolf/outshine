"""Compare identical Ground inputs with/without the necessary port preflight."""

import argparse
import hashlib
import json
import subprocess
import time
from dataclasses import fields
from functools import partial
from pathlib import Path
from unittest.mock import patch

import numpy as np
from ground_prior_examples import examples
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground import fit_ground
from outshine_ref.transport.canonical_ports._ground_prior import prior_contradiction


def call(inputs, limit, method, enabled):
    if enabled:
        return fit_ground(inputs, limit, method, PortBudget())
    with patch(
        "outshine_ref.transport.canonical_ports._ground.prior_contradiction",
        return_value=(None, {}),
    ):
        return fit_ground(inputs, limit, method, PortBudget())


def digest(field):
    if field is None:
        return None
    if not hasattr(field, "plan"):
        return {
            "gradient": field.gradient.tolist(),
            "origin": field.origin.tolist(),
            "intercept": field.intercept,
        }
    plan = field.plan
    arrays = {
        m.name: getattr(plan, m.name)
        for m in fields(plan)
        if isinstance(getattr(plan, m.name), np.ndarray)
    }
    arrays.update(vertices=plan.mesh.vertices, faces=plan.mesh.triangles)
    return {k: hashlib.sha256(a.tobytes()).hexdigest() for k, a in arrays.items()}


def allocations(path, invoke):
    import memray

    with memray.Tracker(str(path), trace_python_allocators=True):
        result = invoke()
    reader = memray.FileReader(str(path))
    releases = {
        memray.AllocatorType.FREE,
        memray.AllocatorType.MUNMAP,
        memray.AllocatorType.PYMALLOC_FREE,
    }
    requests = bytes_ = 0
    for record in reader.get_allocation_records():
        if record.allocator not in releases:
            requests += record.n_allocations
            bytes_ += record.size
    return {
        "allocation_requests": requests,
        "cumulative_requested_bytes": bytes_,
        "tracked_peak_bytes": sum(
            r.size for r in reader.get_high_watermark_allocation_records()
        ),
        "rejected": result[0] is None,
    }


def serial(value):
    if hasattr(value, "items"):
        return dict(value)
    if hasattr(value, "tolist"):
        return value.tolist()
    raise TypeError(type(value).__name__)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allocations", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    records = []
    for name, inputs, binding in examples():
        # Warm the actual C1 backend/imports before the paired diagnostic calls.
        call(inputs, 3.0 if name == "chain" else 0.5, "cone", False)
        cases = [("auto", 0.5), ("lp", 0.5)]
        if name == "chain":
            cases.append(("auto", 3.0))
        for method, limit in cases:
            row = {
                "name": name,
                "method": method,
                "limit_m": limit,
                "binding": binding,
                "vertices": len(inputs.surface.vertices),
                "faces": len(inputs.surface.triangles),
                "preflight": prior_contradiction(inputs, limit, PortBudget()),
            }
            for enabled in (False, True):
                began = time.perf_counter()
                field, report = call(inputs, limit, method, enabled)
                label = "with_preflight" if enabled else "baseline"
                row[label] = {
                    "total_ms": 1000 * (time.perf_counter() - began),
                    "rejected": field is None,
                    "reason": report.get("reason", ""),
                    "fit_rounds": len(report.get("rounds", [])),
                    "digest": digest(field),
                    "report": report,
                }
                if args.allocations and name == "chain" and limit == 0.5:
                    row[label]["allocations"] = allocations(
                        args.output / f"{name}-{method}-{label}.bin",
                        partial(call, inputs, limit, method, enabled),
                    )
            assert row["baseline"]["rejected"] == row["with_preflight"]["rejected"]
            assert row["baseline"]["reason"] == row["with_preflight"]["reason"]
            assert row["baseline"]["digest"] == row["with_preflight"]["digest"]
            records.append(row)
    report = {
        "runtime_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True, cwd=Path(__file__).parent
        ).strip(),
        "scope": "Same prepared actual inputs; baseline only bypasses the new necessary preflight. Source/index/DEM/geometry preparation and warmup excluded. Single shared-host diagnostics, no stable/native/device timing. Allocation bytes are not total copied bytes or process peaks. Original Source/ports/World admission unchanged.",
        "records": records,
    }
    (args.output / "report.json").write_text(
        json.dumps(report, default=serial, indent=2) + "\n"
    )
    print(
        json.dumps(
            [
                {k: r[k] for k in ("name", "method", "limit_m", "preflight")}
                | {
                    label: {
                        k: r[label][k]
                        for k in ("total_ms", "rejected", "reason", "fit_rounds")
                    }
                    for label in ("baseline", "with_preflight")
                }
                for r in records
            ],
            default=serial,
            indent=2,
        )
    )


if __name__ == "__main__":
    main()

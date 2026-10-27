"""Compare original-source C1 fits against a frozen earlier slice module."""

import argparse
import importlib.util
import json
import subprocess
import sys
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from bridge_end_surface_costs import traced
from ground_prior_examples import examples
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground import fit_ground
from outshine_ref.transport.port_heights import _slices, _solve


def load_baseline(path):
    name = "outshine_ref.transport.port_heights._slices_comparison_baseline"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def counted_builder(module, counts):
    class Counted(module.SliceBuilder):
        def clip(self, polygon, *args):
            counts["polygon_edge_visits"] += len(polygon)
            counts["clip_calls"] += 1
            return super().clip(polygon, *args)

        def split(self, polygon, *args):
            counts["polygon_edge_visits"] += len(polygon)
            counts["split_calls"] += 1
            return super().split(polygon, *args)

        def crossing(self, *args):
            counts["crossing_calls"] += 1
            return super().crossing(*args)

    return Counted


def plan_arrays(field):
    plan = field.plan
    values = {
        "vertices": plan.mesh.vertices,
        "faces": plan.mesh.triangles,
        "datum_m": plan.datum_m,
    }
    for name in (
        "plane_coefficients",
        "face_parts",
        "grade_frames",
        "source_faces",
        "original_vertex_map",
        "port_edges",
        "port_ids",
        "height_error_bounds_m",
        "targets_m",
        "parameter_intervals",
    ):
        values[name] = getattr(plan, name)
    return {name: np.asarray(value) for name, value in values.items()}


def measure(module, inputs, method, output):
    def invoke():
        field, report = fit_ground(inputs, 0.5, method, PortBudget())
        if field is None:
            raise ValueError(report)
        return field

    counts = dict.fromkeys(
        ("polygon_edge_visits", "clip_calls", "split_calls", "crossing_calls"), 0
    )
    with patch.object(_solve, "slice_mesh", module.slice_mesh):
        invoke()
        started = time.perf_counter()
        field = invoke()
        elapsed = 1000 * (time.perf_counter() - started)
        allocations = traced(output.with_suffix(".bin"), invoke)
        with patch.object(module, "SliceBuilder", counted_builder(module, counts)):
            instrumented = invoke()
    arrays = plan_arrays(field)
    assert all(
        value.tobytes() == plan_arrays(instrumented)[name].tobytes()
        for name, value in arrays.items()
    )
    np.savez_compressed(output.with_suffix(".npz"), **arrays)
    return arrays, {
        "elapsed_ms": elapsed,
        "work": counts,
        **allocations,
        "stored_output_array_bytes": sum(a.nbytes for a in arrays.values()),
    }


def encode(value):
    if hasattr(value, "items"):
        return dict(value)
    raise TypeError(type(value).__name__)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    baseline = load_baseline(args.baseline)
    records = []
    for name, inputs, binding in examples():
        if name != "star":
            continue
        for method in ("auto", "lp"):
            arrays, rows = {}, {}
            for label, module in (("baseline", baseline), ("current", _slices)):
                arrays[label], rows[label] = measure(
                    module, inputs, method, args.output / (method + "-" + label)
                )
            parity = {
                name: values.tobytes() == arrays["baseline"][name].tobytes()
                for name, values in arrays["current"].items()
            }
            assert all(parity.values()), parity
            records.append(
                {
                    "method": method,
                    "binding": binding,
                    "methods": rows,
                    "array_parity": parity,
                }
            )
            print(method, rows, flush=True)
    report = {
        "runtime_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "records": records,
        "scope": "Same prepared actual full-width source input,0.5m prior limit and full affine/preflight/C1/finite-quality fit. Decode/index/DEM preparation, optional solver warmups and independent comparison/export excluded both; repeated subdivision/validation/solver work included. Counters are a separate instrumented fit, outside timings/allocation tracing. Single shared-host calls, no stable native/device proof. Array output bytes are not total copied bytes; tracked peak is not process/device peak. Source/Unknown and World admission are unchanged.",
    }
    (args.output / "report.json").write_text(
        json.dumps(report, default=encode, indent=2) + "\n"
    )


if __name__ == "__main__":
    main()

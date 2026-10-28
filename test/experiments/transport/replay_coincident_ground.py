"""Same-product original-band reuse with frozen source, costs and finite coverage."""

import argparse
import gc
import hashlib
import json
import os
import platform
import subprocess
import time
from dataclasses import fields
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from coincident_ground_checks import audit
from coincident_ground_cover import generate
from coincident_ground_inputs import prepare
from coincident_ground_plot import render
from replay_canonical_ports import process_peak, serializable


def arrays(result):
    inputs, field = result["inputs"], result["field"]
    captured = {
        k: result[k] for k in ("axes", "sections", "observations", "face_parts")
    }
    captured.update(
        vertices=result["mesh"].vertices,
        faces=result["mesh"].triangles,
        prior_vertices=inputs.surface.vertices,
        prior_faces=inputs.surface.triangles,
        field_origin=field.origin,
        field_gradient=field.gradient,
    )
    captured.update(
        {
            f"input_{f.name}": getattr(inputs, f.name)
            for f in fields(inputs)
            if isinstance(getattr(inputs, f.name), np.ndarray)
        }
    )
    captured.update(
        {
            f"query_{k}": v
            for k, v in vars(field.domain).items()
            if isinstance(v, np.ndarray)
        }
    )
    return captured


def held_bytes(captured):
    intervals = []
    for value in captured.values():
        while isinstance(value.base, np.ndarray):
            value = value.base
        intervals.append(np.lib.array_utils.byte_bounds(value))
    total, end = 0, 0
    for low, high in sorted(intervals):
        total += max(0, high - max(end, low))
        end = max(end, high)
    return int(total)


def replay(output, rounds):
    root = Path(__file__).resolve().parents[3]
    dirty = subprocess.check_output(
        ["git", "status", "--porcelain", "--", "test/experiments/transport", "lib"],
        cwd=root,
        text=True,
    )
    if dirty:
        raise ValueError(
            "Freeze transport implementation and fixed inputs before evidence"
        )
    revision = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True
    ).strip()
    started = time.process_time()
    index, case, terrain, budget, metadata = prepare()
    acquisition = time.process_time() - started
    methods = {"fresh": False, "shared": True}
    products = {
        k: generate(index, case["selected"], terrain, budget, shared=v)
        for k, v in methods.items()
    }
    runs, memory = {k: [] for k in methods}, {}
    for turn in range(rounds):
        order = methods if turn % 2 == 0 else reversed(methods)
        for name in order:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            result = generate(
                index, case["selected"], terrain, budget, shared=methods[name]
            )
            runs[name].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            for key, value in arrays(result).items():
                if not np.array_equal(value, arrays(products[name])[key]):
                    raise ValueError("Timed cover changed")
            del result
    for name, shared in methods.items():
        products[name], memory[name] = allocations(
            output / f"{name}.bin",
            lambda shared=shared: generate(
                index, case["selected"], terrain, budget, shared=shared
            ),
        )
    captured = {name: arrays(result) for name, result in products.items()}
    for key, value in captured["fresh"].items():
        if not np.array_equal(value, captured["shared"][key]):
            raise ValueError("Shared cover changed actual fields")
    np.savez_compressed(
        output / "geometry.npz",
        **{
            f"{name}_{key}": value
            for name, row in captured.items()
            for key, value in row.items()
        },
    )
    report = {
        "commit": revision,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "threads": {
            k: os.environ.get(k) for k in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "input_acquisition_cpu_s": acquisition,
        "inputs": metadata,
        "case": case,
        "methods": {
            name: {
                "runs": runs[name],
                "allocations": memory[name],
                "work": result["work"],
                "audit": audit(result, index, case["selected"]),
                "known_referenced_numpy_bytes": held_bytes(captured[name]),
                "original_sources": serializable(result["source"]),
            }
            for name, result in products.items()
        },
        "exact_equal_fields": len(captured["fresh"]),
        "process_peak_rss": process_peak(),
        "total_internal_copied_bytes": None,
        "scope": "Same complete source/physical product. All original-band/source preparation, bounded union/triangulation, twelve DEM observations, affine construction and finite quality inside timed calls; source-index acquisition, independent audit/export/render and trace processing outside. Numeric costs only, no native/device/p99 or routing admission.",
    }
    paths = sorted(Path(__file__).parent.glob("coincident_ground*.py")) + [
        Path(__file__)
    ]
    paths += sorted(
        Path(__file__).with_name("fixtures").glob("coincident-ground-vienna-dem.*")
    )
    report["implementation_sha256"] = {
        str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in paths
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    render(products["shared"], output)
    print(
        json.dumps(
            {
                "commit": revision,
                "equal_fields": report["exact_equal_fields"],
                "audit": report["methods"]["shared"]["audit"],
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=7)
    args = parser.parse_args()
    output = args.output.resolve()
    if (
        output == Path("/tmp")
        or not output.is_relative_to(Path("/tmp"))
        or not 1 <= args.rounds <= 11
    ):
        parser.error("Fresh explicit /tmp subdirectory and1..11 rounds required")
    output.mkdir(parents=True, exist_ok=False)
    replay(output, args.rounds)


if __name__ == "__main__":
    main()

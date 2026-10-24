"""Scoped real recipe costs and exact exports for intermediate-lifetime comparisons."""

import argparse
import gc
import hashlib
import json
import os
import platform
import resource
import sys
import time
import tracemalloc
from collections.abc import Mapping
from dataclasses import fields, is_dataclass
from pathlib import Path

import numpy as np


def consumer(case):
    domain = "vegetation" if case.startswith("plant") else "buildings"
    sys.path.insert(0, str(Path(__file__).parents[1] / domain))
    if domain == "vegetation":
        from recipe_plant_operators import prepared
        from recipe_plant_values import seed_input

        return prepared(), {"seed": seed_input(718)}, (case.split("-")[1],)
    from recipe_hall_operators import inputs_for, prepare
    from verify_industrial_halls import sources

    row, neighbors = sources()[2]
    return prepare(), inputs_for(row, neighbors), ("shell",)


def array_streams(value, path, result):
    if isinstance(value, np.ndarray):
        result[path] = value
    elif isinstance(value, Mapping):
        for key, child in value.items():
            array_streams(child, path + "/" + str(key), result)
    elif isinstance(value, (tuple, list)):
        for index, child in enumerate(value):
            array_streams(child, path + "/" + str(index), result)
    elif is_dataclass(value):
        for item in fields(value):
            array_streams(getattr(value, item.name), path + "/" + item.name, result)


def measure(graph, inputs, outputs, repeats):
    samples = []
    for _ in range(repeats + 1):
        gc.collect()
        wall, cpu = time.perf_counter(), time.process_time()
        run = graph.evaluate(inputs, outputs=outputs)
        elapsed_cpu = time.process_time() - cpu
        samples.append((time.perf_counter() - wall, elapsed_cpu))
        del run
    gc.collect()
    tracemalloc.start()
    run = graph.evaluate(inputs, outputs=outputs)
    current, peak = tracemalloc.get_traced_memory()
    blocks = sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"))
    tracemalloc.stop()
    return run, {
        "first_wall_cpu_s": samples[0],
        "following_wall_cpu_s": samples[1:],
        "median_wall_cpu_s": np.median(samples[1:], axis=0).tolist(),
        "python_traced_peak_bytes": peak,
        "python_traced_live_bytes": current,
        "python_traced_live_blocks": blocks,
    }


def export(run, output, reference):
    from recipe_graph_data import canonical, plain

    arrays = {}
    for key, product in run.outputs.items():
        array_streams(product.value, key, arrays)
    record = canonical(
        {
            "definition": run.definition_fingerprint,
            "instance": run.instance_fingerprint,
            "outputs": {
                key: {
                    "fingerprint": product.fingerprint,
                    "type": product.type_id,
                    "nbytes": product.nbytes,
                    "metadata": plain(product.metadata),
                }
                for key, product in run.outputs.items()
            },
            "stats": plain(run.stats),
        }
    )
    if reference is not None:
        if (reference / "products.json").read_bytes() != record:
            raise ValueError(
                "product identities, metadata, accounting or stats changed"
            )
        with np.load(reference / "arrays.npz") as previous:
            if set(previous.files) != set(arrays) or any(
                previous[key].dtype != value.dtype
                or previous[key].shape != value.shape
                or previous[key].tobytes() != value.tobytes()
                for key, value in arrays.items()
            ):
                raise ValueError("actual product array words changed")
    (output / "products.json").write_bytes(record)
    np.savez(output / "arrays.npz", **arrays)
    return len(arrays)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--case", choices=("plant-detail", "plant-plan", "hall-shell"), required=True
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--kernel", type=Path)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--repeats", type=int, default=9)
    args = parser.parse_args()
    if not 1 <= args.repeats <= 100:
        parser.error("bounded positive repeat count required")
    if args.kernel is not None:
        if args.kernel.name != "recipe_graph.py" or not args.kernel.is_file():
            parser.error("explicit trusted recipe_graph.py source required")
        sys.path.insert(0, str(args.kernel.resolve().parent))
    load = os.getloadavg()
    began = time.perf_counter()
    graph, inputs, outputs = consumer(args.case)
    prepared_s = time.perf_counter() - began
    rss_scale = 1 if platform.system() == "Darwin" else 1024
    before_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * rss_scale
    run, report = measure(graph, inputs, outputs, args.repeats)
    after_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * rss_scale
    import recipe_graph

    kernel = Path(recipe_graph.__file__)
    args.output.mkdir(parents=True, exist_ok=True)
    stream_count = export(run, args.output, args.reference)
    timer_pairs = []
    for _ in range(101):
        wall, cpu = time.perf_counter(), time.process_time()
        elapsed_cpu = time.process_time() - cpu
        timer_pairs.append((time.perf_counter() - wall, elapsed_cpu))
    report.update(
        case=args.case,
        requested_outputs=outputs,
        preparation_including_domain_imports_s=prepared_s,
        kernel_sha256=hashlib.sha256(kernel.read_bytes()).hexdigest(),
        start_load=load,
        python=platform.python_version(),
        platform=platform.platform(),
        before_evaluation_process_peak_rss_bytes=before_rss,
        process_peak_rss_bytes_before_export=after_rss,
        exported_array_streams=stream_count,
        exact_reference_match=args.reference is not None,
        timer_pair_median_wall_cpu_s=np.median(timer_pairs, axis=0).tolist(),
        scope="Finite existing recipe consumer; prepared graph/inputs, separate memory trace, GC before every invocation. RSS includes imports and prior calls. Kernel strategies run in separate processes under other host work; no isolated CPU win, native/UMA or device admission. Declared bytes/live Python blocks are not physical unique buffers/total allocations. Native domain callbacks may use multiple CPU threads.",
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "following_wall_cpu_s"}))


if __name__ == "__main__":
    main()

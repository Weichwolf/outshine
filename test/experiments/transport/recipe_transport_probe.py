"""Fresh bounded worker: real graph replay or same trusted operators called directly."""

import argparse
import contextlib
import gc
import json
import resource
import statistics
import subprocess
import time
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import recipe_transport_values as values
from camera_height_inputs import digest
from rail_instance_cost import profile_note
from recipe_graph_data import digest as content_digest
from recipe_transport_inputs import SOUTH, inputs_for
from recipe_transport_operators import body_operator, prepare, profile_operator
from recipe_transport_values import descriptor
from stair_junction_verify import display_proof
from stair_terrain_experiment import PALETTE


@contextlib.contextmanager
def copy_counts():
    counts = {
        "snapshot_arrays": 0,
        "snapshot_bytes": 0,
        "descriptor_arrays": 0,
        "descriptor_bytes": 0,
    }
    original_seal, original_describe = values.sealed, values.descriptor

    def seal(item):
        if isinstance(item, np.ndarray):
            counts["snapshot_arrays"] += 1
            counts["snapshot_bytes"] += item.nbytes
        return original_seal(item)

    def describe(item):
        if isinstance(item, np.ndarray):
            counts["descriptor_arrays"] += 1
            counts["descriptor_bytes"] += item.nbytes
        return original_describe(item)

    with (
        patch.object(values, "sealed", seal),
        patch.object(values, "descriptor", describe),
    ):
        yield counts


def evaluate(recipe, inputs, mode, kind):
    exports = ("plan",) if kind == "plan" else ("plan", "body")
    if mode == "graph":
        run = recipe.evaluate(inputs, outputs=exports)
        return run.outputs, {
            "definition": run.definition_fingerprint,
            "instance": run.instance_fingerprint,
            "products": {key: value.fingerprint for key, value in run.outputs.items()},
            "stats": dict(run.stats),
        }
    node = next(
        n
        for n in recipe.nodes.values()
        if n.operator.key.id == "transport.solve-profiles"
    )
    plan = profile_operator(node.parameters, inputs)["plan"]
    outputs = {"plan": plan}
    if kind == "body":
        outputs["body"] = body_operator({}, {"source": inputs["source"], "plan": plan})[
            "body"
        ]
    return outputs, {"definition": recipe.definition_fingerprint}


def summary(outputs):
    return {
        name: {
            "value_digest": content_digest(descriptor(product.value)),
            "metadata": dict(product.metadata),
            "accounted_bytes": product.nbytes,
        }
        for name, product in outputs.items()
    }


def reference(args):
    if args.baseline is None:
        return None
    report_path = args.baseline / "comparison.json"
    report = json.loads(report_path.read_text())
    verification = json.loads((args.baseline / "verification.json").read_text())
    if (
        not verification["valid_local_candidate"]
        or verification["products_report_sha256"] != digest(report_path)
        or report["fixture_sha256"] != digest(args.fixture)
    ):
        raise ValueError("Original verified whole-source reference required")
    expected = {}
    for filename, key in (
        ("body.npz", "body_sha256"),
        ("height-input.npz", "height_input_sha256"),
    ):
        path = args.baseline / "after" / filename
        if digest(path) != report["after"][key]:
            raise ValueError("Original reference arrays changed")
        with np.load(path, allow_pickle=False) as saved:
            expected[filename] = dict(saved)
    return report, expected


def compare_reference(outputs, baseline, directory):
    if baseline is None:
        return
    report, expected = baseline
    plan = outputs["plan"].value
    actual = {
        "height-input.npz": {
            "xy": plan["layout"]["xy"],
            "heights": plan["heights"],
            **{
                "faces" + str(i): part for i, part in enumerate(plan["layout"]["parts"])
            },
        }
    }
    if "body" in outputs:
        actual["body.npz"] = outputs["body"].value["data"]
    for name, arrays in actual.items():
        original = expected[name]
        if set(arrays) != set(original) or any(
            value.dtype != original[key].dtype
            or not np.array_equal(value, original[key])
            for key, value in arrays.items()
        ):
            raise ValueError(
                "Actual graph/direct output differs from prior verified arrays"
            )
    if "body" in outputs:
        display_proof(
            SimpleNamespace(output=directory / "after", palette_scene=PALETTE),
            actual["body.npz"],
            report["after"],
        )


def frozen():
    dirty = subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--",
            "lib",
            "test/experiments/transport",
            "test/experiments/assets",
        ],
        text=True,
    ).strip()
    if dirty:
        raise ValueError("Fresh recipe workers require frozen clean source")
    return subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()


def measure(args):
    commit = frozen()
    if (
        args.output.exists()
        or (args.profile and args.profile.exists())
        or not 1 <= args.runs <= 7
    ):
        raise ValueError("Fresh explicit worker paths and one to seven runs required")
    args.output.mkdir(parents=True)
    began = time.perf_counter()
    with copy_counts() as source_copies:
        inputs = inputs_for(args.fixture)
    capture_ms = (time.perf_counter() - began) * 1000
    began = time.perf_counter()
    recipe = prepare()
    compile_ms = (time.perf_counter() - began) * 1000
    expected = reference(args)
    with copy_counts() as phase_copies:
        warm, warm_identity = evaluate(recipe, inputs, args.mode, args.kind)
    compare_reference(warm, expected, args.baseline)
    canonical = summary(warm)
    del warm
    times, identity = [], None
    for _ in range(1 if args.profile else args.runs):
        gc.collect()
        tracker = contextlib.nullcontext()
        if args.profile:
            import memray

            tracker = memray.Tracker(
                str(args.profile), trace_python_allocators=False, native_traces=False
            )
        began = time.perf_counter()
        with tracker:
            outputs, identity = evaluate(recipe, inputs, args.mode, args.kind)
        times.append((time.perf_counter() - began) * 1000)
        if summary(outputs) != canonical or identity != warm_identity:
            raise ValueError("Same source/definition did not replay deterministically")
        compare_reference(outputs, expected, args.baseline)
        if "body" in outputs:
            np.savez_compressed(
                args.output / "body.npz", **outputs["body"].value["data"]
            )
        del outputs
    result = {
        "commit": commit,
        "fixture_sha256": digest(args.fixture),
        "mode": args.mode,
        "kind": args.kind,
        "valid_complete_output": True,
        "identity": identity,
        "products": canonical,
        "capture_ms_with_copy_counters": capture_ms,
        "compile_ms": compile_ms,
        "source_explicit_copies": source_copies,
        "warm_phase_explicit_copies": phase_copies,
        "phase_ms": times,
        "median_ms": statistics.median(times),
        "profiled": args.profile is not None,
        "worker_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "exact_original_arrays": args.baseline is not None,
        "baseline_report_sha256": digest(args.baseline / "comparison.json")
        if args.baseline
        else None,
        "scope": "Same real source snapshot, trusted plan/build/physical operators and cooperative bounds. Direct calls omit graph typing/identities/reservations, retain operator metadata/digests. One diagnostic warm run excluded; complete evaluate timed, source capture/compile/verification/IO outside. Explicit snapshot/descriptor tobytes payload counted only in warm diagnostic; JSON/GEOS/solver/internal copies remain unmeasured. No equal-feature evaluator contest, native speedup, world/frame/device or cache claim.",
    }
    if args.profile:
        allocations = profile_note(str(args.profile))
        allocations.update(
            trace_python_allocators=False,
            native_traces=False,
            scope="One complete evaluation after imports/source capture/compile and warm evaluation. Native allocation requests observed; internal total copies and Python allocator events unobserved.",
        )
        result["allocations"] = allocations
    (args.output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in ("mode", "kind", "median_ms", "valid_complete_output")
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=SOUTH)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--mode", choices=("graph", "direct"), default="graph")
    parser.add_argument("--kind", choices=("plan", "body"), default="body")
    parser.add_argument("--runs", type=int, default=1)
    parser.add_argument("--profile", type=Path)
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    measure(parser.parse_args())

"""Frozen own predecessor versus current binding on an unchanged complete cached plan."""

import argparse
import hashlib
import importlib.util
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
import spruce_growth_product as current
from outshine_ref.vegetation import NeedlePattern
from window_portrait_source import arrays


def reference(commit, output):
    modules, hashes = [], {}
    for name in ("conifer_windows", "spruce_growth_product"):
        text = subprocess.check_output(
            ["git", "show", f"{commit}:test/experiments/vegetation/{name}.py"]
        )
        path = output / f"reference-{name}.py"
        path.write_bytes(text)
        spec = importlib.util.spec_from_file_location(f"frozen_{name}", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        modules.append(module)
        hashes[name] = hashlib.sha256(text).hexdigest()
    patterns, product = modules
    product.pattern_key = patterns.pattern_key
    product.bind_patterns = patterns.bind_patterns
    return patterns, product, hashes


def identical(a, b):
    assert set(a) == set(b)
    for key in a:
        np.testing.assert_array_equal(a[key], b[key])
        assert a[key].dtype == b[key].dtype


def measure_binding(function, patterns, plan, catalog, output, profile):
    samples = []
    for _ in range(3):
        start = time.perf_counter()
        result = function(patterns, plan, catalog)
        samples.append(time.perf_counter() - start)
    if profile:
        import memray

        with memray.Tracker(str(output), trace_python_allocators=True):
            profiled = function(patterns, plan, catalog)
        identical(result, profiled)
    return result, samples


def compare(product, catalog_path, commit, output, profile=False):
    output.mkdir(parents=True, exist_ok=False)
    old_patterns, old_product, hashes = reference(commit, output)
    recipe = json.loads((product / "plant-recipe.json").read_text())
    patterns = tuple(
        NeedlePattern.from_recipe(p) if p else None
        for p in recipe["physical_source"]["patterns"]
    )
    metadata = json.loads((catalog_path / "catalog.json").read_text())
    axes = arrays(product / "execution/axes.npz")
    saved = arrays(product / "windows.npz")
    expected, seed = len(saved["origins"]), recipe["wood"]["seed"]
    captured = {}

    def capture(patterns, plan, catalog):
        captured["plan"] = plan
        return old_patterns.bind_patterns(patterns, plan, catalog)

    old_product.bind_patterns = capture
    began = time.perf_counter()
    old = old_product.windows(axes, patterns, metadata, seed, expected)
    old_s = time.perf_counter() - began
    identical(old, saved)
    began = time.perf_counter()
    new = current.windows(axes, patterns, metadata, seed, expected)
    new_s = time.perf_counter() - began
    identical(new, saved)
    plan = captured["plan"]
    from conifer_windows import bind_patterns

    a, old_samples = measure_binding(
        old_patterns.bind_patterns,
        patterns,
        plan,
        metadata,
        output / "old-binding.bin",
        profile,
    )
    b, new_samples = measure_binding(
        bind_patterns, patterns, plan, metadata, output / "new-binding.bin", profile
    )
    identical(a, b)
    own = [
        Path(__file__),
        Path(current.__file__),
        Path(__file__).with_name("conifer_windows.py"),
    ]
    report = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "reference_commit": commit,
        "reference_sha256": hashes,
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in own
        },
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                product / "plant-recipe.json",
                product / "windows.npz",
                product / "execution/axes.npz",
                catalog_path / "catalog.json",
            )
        },
        "axes": len(axes["origins"]),
        "windows": expected,
        "complete_windows_s": {"old": old_s, "new": new_s},
        "binding_samples_s": {"old": old_samples, "new": new_samples},
        "input_array_bytes": sum(v.nbytes for v in plan.values()),
        "output_array_bytes": sum(v.nbytes for v in b.values()),
        "new_retained_array_bytes": sum(
            v.nbytes for k, v in b.items() if k in ("frames", "families", "weights")
        ),
        "all_complete_product_fields_exact": True,
        "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "profile_scope": "Optional Python allocator tracking around bind_patterns only; full window timings exclude decode/validation/output and have no allocation instrumentation.",
        "scope": "Same complete real cached axes and physical windows; no growth or organ expansion. Single observed complete-window pair and three isolated binding pairs, not a device/frame/total-build claim.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("product", "catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--reference-commit", required=True)
    parser.add_argument("--profile", action="store_true")
    args = parser.parse_args()
    compare(
        args.product, args.catalog, args.reference_commit, args.output, args.profile
    )

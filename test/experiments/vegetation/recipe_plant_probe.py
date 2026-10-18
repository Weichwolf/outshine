"""Fresh real-consumer replay and scoped direct/shared-kernel costs, not a cache."""

import argparse
import json
import os
import resource
import statistics
import subprocess
import sys
import time
import tracemalloc
from pathlib import Path

from europe_conifer_recipes import conifers
from outshine_ref.contracts import immutable
from recipe_plant_baseline import compare
from recipe_plant_detail import physical_detail
from recipe_plant_operators import SITE, prepared
from recipe_plant_sources import site_input, source_site
from recipe_plant_values import PlantDetail, arrays, seed_input

SPECIES = next(s for s in conifers() if s.key == "pinus-sylvestris")


def direct(seed, mode):
    plan = SPECIES.plant(seed)
    if mode == "plan":
        return plan.wood.recipe()
    value = PlantDetail(
        immutable(plan.recipe()),
        arrays(plan.tables(512, 16384)),
        tuple(arrays(n.mesh(8, 8)) for n in plan.needles),
    )
    physical_detail(value)
    return value


def worker(output, seed, mode):
    started = time.perf_counter()
    inputs = {"seed": seed_input(seed)}
    if mode == "placement":
        inputs["site"] = site_input(source_site())
    sampled = time.perf_counter()
    graph = prepared(SITE) if mode == "placement" else prepared()
    loaded = time.perf_counter()
    result = graph.evaluate(inputs, outputs=(mode,))
    evaluated = time.perf_counter()
    product = result.outputs[mode]
    report = {
        "seed": seed,
        "mode": mode,
        "definition": result.definition_fingerprint,
        "instance": result.instance_fingerprint,
        "product": product.fingerprint,
        "content": product.metadata["content_sha256"],
        "nbytes": product.nbytes,
        "inputs_s": sampled - started,
        "prepare_s": loaded - sampled,
        "evaluate_s": evaluated - loaded,
        "stats": dict(result.stats),
        "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


def fresh(output):
    rows = []
    for seed in (718, 719):
        for mode in ("plan", "detail", "placement"):
            previous = None
            for repeat in range(2):
                path = output / f"fresh-{seed}-{mode}-{repeat}.json"
                command = [
                    sys.executable,
                    str(Path(__file__).resolve()),
                    "--worker",
                    "--seed",
                    str(seed),
                    "--mode",
                    mode,
                    "--output",
                    str(path),
                ]
                start = time.perf_counter()
                completed = subprocess.run(
                    command, capture_output=True, text=True, check=False, timeout=120
                )
                path.with_suffix(".log").write_text(completed.stdout + completed.stderr)
                completed.check_returncode()
                report = json.loads(path.read_text())
                signature = {
                    k: report[k]
                    for k in ("definition", "instance", "product", "content", "nbytes")
                }
                if previous is not None and previous != signature:
                    raise ValueError("Fresh real vegetation consumer replay changed")
                previous = signature
                rows.append(
                    {
                        "seed": seed,
                        "mode": mode,
                        "repeat": repeat,
                        "wall_s": time.perf_counter() - start,
                        "exit_code": completed.returncode,
                    }
                )
    return rows


def compare_controls(graph, inputs, runs):
    rows = []
    for mode in ("plan", "detail"):
        samples = {"direct": [], "graph": []}
        for repeat in range(runs + 2):
            for method in (
                ("direct", "graph") if repeat % 2 == 0 else ("graph", "direct")
            ):
                start = time.perf_counter()
                if method == "direct":
                    value = direct(718, mode)
                else:
                    value = graph.evaluate(inputs, outputs=(mode,)).outputs[mode].value
                elapsed = time.perf_counter() - start
                if repeat >= 2:
                    samples[method].append(elapsed)
                del value
        traced = {}
        for method in ("direct", "graph"):
            tracemalloc.start()
            value = (
                direct(718, mode)
                if method == "direct"
                else graph.evaluate(inputs, outputs=(mode,)).outputs[mode].value
            )
            live, peak = tracemalloc.get_traced_memory()
            blocks = sum(
                s.count for s in tracemalloc.take_snapshot().statistics("filename")
            )
            tracemalloc.stop()
            traced[method] = {
                "live_bytes": live,
                "peak_bytes": peak,
                "live_blocks": blocks,
            }
            del value
        rows.append(
            {
                "mode": mode,
                "median_s": {k: statistics.median(v) for k, v in samples.items()},
                "raw_s": samples,
                "traced_separate": traced,
            }
        )
    return rows


def run(output, baseline, runs):
    output.mkdir(parents=True, exist_ok=False)
    if not 1 <= runs <= 41:
        raise ValueError("Bounded comparison repeat count required")
    graph = prepared()
    inputs = {"seed": seed_input(718)}
    value = graph.evaluate(inputs, outputs=("detail",)).outputs["detail"].value
    facts = physical_detail(value)
    prior = compare(baseline, value) if baseline else None
    expected = direct(718, "detail")
    if value.descriptor() != expected.descriptor():
        raise ValueError("Direct and graph physical detail differ")
    buffers = (value.tables, *value.needles)
    owned_bytes = sum(a.nbytes for block in buffers for a in block.values())
    before = os.getloadavg()
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "physical": facts,
        "prior_render_inputs": prior,
        "fresh": fresh(output),
        "comparison": compare_controls(graph, inputs, runs),
        "known_detail_buffers": {
            "arrays": sum(len(block) for block in buffers),
            "payload_bytes": owned_bytes,
            "explicit_freeze_payload_writes_B": owned_bytes,
            "descriptor_hash_byte_copies_B": owned_bytes * 2,
        },
        "load_start": before,
        "load_end": os.getloadavg(),
        "scope": "Uncached direct existing procedure versus typed/hash-checked graph, same physical outputs/reader. Known freeze writes/hash copies exclude generation/solver temporaries and total allocations. Traced heap excludes native/GEOS; Linux RSS includes imports. No image/device/cache/native ABI admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": report["commit"],
                "physical": facts,
                "baseline": prior,
                "fresh_processes": len(report["fresh"]),
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--runs", type=int, default=21)
    parser.add_argument("--worker", action="store_true")
    parser.add_argument("--seed", type=int, default=718)
    parser.add_argument(
        "--mode", choices=("plan", "detail", "placement"), default="detail"
    )
    args = parser.parse_args()
    if args.worker:
        resource.setrlimit(resource.RLIMIT_CPU, (115, 120))
        resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
        worker(args.output, args.seed, args.mode)
    else:
        run(args.output, args.baseline, args.runs)

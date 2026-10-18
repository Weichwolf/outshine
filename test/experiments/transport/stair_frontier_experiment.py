"""Automatically choose bounded original-source modules, then admit whole products."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path
from types import SimpleNamespace

from camera_height_inputs import digest
from stair_frontier_dispositions import source_dispositions
from stair_frontier_sources import proposals
from stair_module_cache import load_cached
from stair_module_experiment import generate, write_json
from stair_module_inputs import FIXTURE, source_context
from stair_terrain_experiment import frozen


def module_args(args, index):
    return SimpleNamespace(
        fixture=args.output / ("recipe-" + str(index) + ".json"),
        output=args.output / ("outshine-module-" + str(index)),
        base=args.base,
        supports=args.supports,
        cached_module=args.cached_module,
        variable_mode="fixed_displacement",
    )


def reference(args):
    if args.reference.resolve() != FIXTURE.with_name(args.reference.name).resolve():
        raise ValueError("known versioned experiment reference required")
    return {
        "reference_fixture": args.reference.name,
        "reference_fixture_sha256": digest(args.reference),
    }


def run(args):
    commit = frozen()
    if (
        args.output.exists()
        or not args.output.name.startswith("outshine-")
        or not args.output.resolve().is_relative_to(tempfile.gettempdir())
    ):
        raise ValueError("fresh explicit outshine-* system-temp output required")
    if type(args.max_attempts) is not int or not 1 <= args.max_attempts <= 3:
        raise ValueError("at most three bounded module attempts required")
    started = time.perf_counter()
    ref = reference(args)
    state = load_cached(args, ref)
    context, sources, _sampler, _layer = source_context(args.reference)
    if context["origin_lat_lon"] != state["support_report"]["origin_lat_lon"]:
        raise ValueError("original source and actual cached frame differ")
    plan, records, roots, deferred, recipes = proposals(
        state, sources, ref, method=args.method, strategy=args.strategy
    )
    args.output.mkdir(parents=True)
    selection = {
        "source_commit": commit,
        "reference": ref,
        "vector_sha256": context["vector"]["sha256"],
        "origin_lat_lon": context["origin_lat_lon"],
        "roots": roots,
        "deferred_profiles": deferred,
        "plan": plan,
    }
    write_json(args.output / "selection.json", selection)
    cached_ids = state["report"]["candidate_source_ids"]
    del state, sources, _sampler, _layer, records
    attempts, chosen, artifacts = (
        [],
        None,
        {"selection.json": digest(args.output / "selection.json")},
    )
    for index, recipe in enumerate(recipes[: args.max_attempts]):
        child = module_args(args, index)
        write_json(child.fixture, recipe)
        result = generate(child)
        attempts.append(
            {
                "index": index,
                "valid_local_module": result["valid_local_module"],
                "reason": result.get("reason", ""),
                "source_ids": recipe["source_ids"],
            }
        )
        artifacts[child.fixture.name] = digest(child.fixture)
        artifacts[str(child.output.relative_to(args.output) / "comparison.json")] = (
            digest(child.output / "comparison.json")
        )
        if result["valid_local_module"]:
            chosen = index
            break
    report = {
        "commit": commit,
        "reference": ref,
        "attempts": attempts,
        "method": args.method,
        "strategy": args.strategy,
        "max_attempts": args.max_attempts,
        "chosen_module": chosen,
        "artifacts": artifacts,
        "cached_sources": cached_ids,
        "source_dispositions": source_dispositions(plan, attempts, chosen),
        "valid_automatic_module": chosen is not None,
        "valid_retained_baseline": chosen is None,
        "seconds": time.perf_counter() - started,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "One automatically selected bounded whole-source module or unchanged verified parents; all proposals need whole geometry admission. No complete/native world or global cache graph.",
    }
    write_json(args.output / "comparison.json", report)
    print(
        json.dumps(
            {
                k: report[k]
                for k in ("commit", "chosen_module", "seconds", "peak_rss_bytes")
            }
        ),
        flush=True,
    )
    return report


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--reference",
        type=Path,
        default=FIXTURE.with_name("stair-north-bridges-vienna.json"),
    )
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--supports", type=Path, required=True)
    parser.add_argument(
        "--cached-module", type=Path, nargs=2, action="append", default=[]
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--method", choices=("index", "scan"), default="index")
    parser.add_argument("--strategy", choices=("whole", "prefix"), default="whole")
    parser.add_argument("--max-attempts", type=int, default=3)
    return parser


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(argument_parser().parse_args())

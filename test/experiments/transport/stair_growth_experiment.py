"""Choose and admit one automatic local module after verified resident products."""

import argparse
import resource
import time
from pathlib import Path
from types import SimpleNamespace

from camera_height_inputs import digest
from stair_frontier_dispositions import source_dispositions
from stair_frontier_experiment import reference
from stair_growth_sources import select
from stair_local_generate import run as generate
from stair_local_replay import load_chain
from stair_local_verify import verify
from stair_module_experiment import write_json
from stair_module_inputs import FIXTURE, source_context
from stair_patch_inputs import fresh
from stair_seed_sources import select as select_seed
from stair_terrain_experiment import frozen


def selection(args):
    began = time.perf_counter()
    view, assets, _supports, _contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    context, sources, _sample, _layer = source_context(args.reference)
    if context["origin_lat_lon"] != manifest["origin_lat_lon"]:
        raise ValueError("source and actual resident frames differ")
    selector = select_seed if getattr(args, "family", "attached") == "seed" else select
    result = selector(
        view,
        assets,
        manifest,
        sources,
        reference(args),
        method=args.method,
        strategy=args.strategy,
        schedule=args.schedule,
    )
    result.update(
        resident_sha256=digest(args.resident / "resident.json"),
        parent_reports=manifest["parent_reports"],
        before_source_ids=manifest["source_ids"],
        vector_sha256=context["vector"]["sha256"],
        origin_lat_lon=context["origin_lat_lon"],
        method=args.method,
        strategy=args.strategy,
        seconds=time.perf_counter() - began,
    )
    return result


def run(args):
    commit, began = frozen(), time.perf_counter()
    fresh(args.output)
    if type(args.max_attempts) is not int or not 1 <= args.max_attempts <= 3:
        raise ValueError("at most three whole-geometry attempts required")
    if type(args.proposal_offset) is not int or not 0 <= args.proposal_offset <= 31:
        raise ValueError("bounded explicit proposal continuation required")
    chosen, attempts = None, []
    selected = selection(args)
    args.output.mkdir()
    write_json(args.output / "selection.json", selected)
    artifacts = {"selection.json": digest(args.output / "selection.json")}
    window = selected["recipes"][
        args.proposal_offset : args.proposal_offset + args.max_attempts
    ]
    for index, recipe in enumerate(window):
        fixture = args.output / ("recipe-" + str(index) + ".json")
        output = args.output / ("outshine-module-" + str(index))
        write_json(fixture, recipe)
        child = SimpleNamespace(
            resident=args.resident,
            prior_module=args.prior_module,
            fixture=fixture,
            output=output,
            max_context_faces=8192,
        )
        report = generate(child)
        verify(child)
        attempts.append(
            {
                "index": index,
                "source_ids": recipe["source_ids"],
                "valid_local_module": report["valid_local_module"],
                "reason": report.get("reason", ""),
            }
        )
        artifacts[fixture.name] = digest(fixture)
        for name in ("comparison.json",):
            artifacts[str(output.relative_to(args.output) / name)] = digest(
                output / name
            )
        if report["valid_local_module"]:
            chosen = index
            break
    report = {
        "commit": commit,
        "resident_sha256": selected["resident_sha256"],
        "parent_reports": selected["parent_reports"],
        "before_source_ids": selected["before_source_ids"],
        "proposal_offset": args.proposal_offset,
        "proposal_count": len(selected["recipes"]),
        "max_attempts": args.max_attempts,
        "attempts": attempts,
        "chosen_module": chosen,
        "source_dispositions": source_dispositions(selected["plan"], attempts, chosen),
        "artifacts": artifacts,
        "valid_automatic_module": chosen is not None,
        "valid_retained_baseline": chosen is None,
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "scope": "One bounded automatic resident module; verification is required even for refusals. No complete world graph/native admission.",
    }
    write_json(args.output / "comparison.json", report)
    print(
        {k: report[k] for k in ("chosen_module", "seconds", "peak_rss_bytes")},
        flush=True,
    )
    return report


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resident", type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument(
        "--reference",
        type=Path,
        default=FIXTURE.with_name("stair-north-bridges-vienna.json"),
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--method", choices=("index", "scan"), default="index")
    parser.add_argument("--strategy", choices=("whole", "prefix"), default="prefix")
    parser.add_argument(
        "--schedule", choices=("roots_first", "prefix_first"), default="roots_first"
    )
    parser.add_argument("--max-attempts", type=int, default=3)
    parser.add_argument("--proposal-offset", type=int, default=0)
    parser.add_argument("--family", choices=("attached", "seed"), default="attached")
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (300, 300))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(arguments())

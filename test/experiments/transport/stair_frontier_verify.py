"""Replay original-node selection and independently inspect actual saved modules."""

import json
import resource

from camera_height_inputs import digest
from stair_frontier_dispositions import source_dispositions
from stair_frontier_experiment import argument_parser, module_args, reference
from stair_frontier_sources import proposals
from stair_module_cache import load_cached
from stair_module_experiment import write_json
from stair_module_inputs import source_context
from stair_module_verify import verify as verify_module
from stair_terrain_experiment import frozen


def verify(args):
    commit = frozen()
    report = json.loads((args.output / "comparison.json").read_text())
    ref = reference(args)
    if (
        report["reference"] != ref
        or report["method"] != args.method
        or report.get("strategy", "whole") != args.strategy
        or report["max_attempts"] != args.max_attempts
        or not 1 <= args.max_attempts <= 3
    ):
        raise ValueError("automatic selection reference or limits changed")
    state = load_cached(args, ref)
    context, sources, _sampler, _layer = source_context(args.reference)
    if context["origin_lat_lon"] != state["support_report"]["origin_lat_lon"]:
        raise ValueError("original source and actual cached frame differ")
    plan, _records, roots, deferred, recipes = proposals(
        state, sources, ref, method=args.method, strategy=args.strategy
    )
    expected = {
        "source_commit": report["commit"],
        "reference": ref,
        "vector_sha256": context["vector"]["sha256"],
        "origin_lat_lon": context["origin_lat_lon"],
        "roots": roots,
        "deferred_profiles": deferred,
        "plan": plan,
    }
    selection = json.loads((args.output / "selection.json").read_text())
    if selection != json.loads(json.dumps(expected)):
        raise ValueError("saved automatic original-source selection differs")
    if report["cached_sources"] != state["report"]["candidate_source_ids"]:
        raise ValueError("automatic selection lost its actual cached parents")
    actual, chosen = {"selection.json"}, None
    proofs = []
    for index, attempt in enumerate(report["attempts"]):
        if index >= min(len(recipes), args.max_attempts) or chosen is not None:
            raise ValueError("unexpected or excessive saved module attempt")
        child = module_args(args, index)
        recipe = json.loads(child.fixture.read_text())
        if (
            recipe != json.loads(json.dumps(recipes[index]))
            or attempt["index"] != index
            or attempt["source_ids"] != recipe["source_ids"]
        ):
            raise ValueError("saved recipe was not the bounded automatic proposal")
        child_report = json.loads((child.output / "comparison.json").read_text())
        if attempt["reason"] != child_report.get("reason", ""):
            raise ValueError("automatic attempt lost its actual rejection reason")
        proof = verify_module(child)
        if proof["valid_local_module"] != attempt["valid_local_module"]:
            raise ValueError("saved automatic module admission changed")
        actual.update(
            [
                child.fixture.name,
                str(child.output.relative_to(args.output) / "comparison.json"),
            ]
        )
        proofs.append(proof["products_report_sha256"])
        if proof["valid_local_module"]:
            chosen = index
    if (
        report["chosen_module"] != chosen
        or report["valid_automatic_module"] != (chosen is not None)
        or report["valid_retained_baseline"] != (chosen is None)
        or set(report["artifacts"]) != actual
    ):
        raise ValueError("incomplete automatic module selection/admission products")
    if chosen is None and len(report["attempts"]) != min(
        len(recipes), args.max_attempts
    ):
        raise ValueError("automatic search stopped without its declared attempts")
    if "source_dispositions" in report and report[
        "source_dispositions"
    ] != source_dispositions(plan, report["attempts"], chosen):
        raise ValueError("saved physical source dispositions changed")
    if args.strategy == "prefix" and "source_dispositions" not in report:
        raise ValueError("connected-prefix admission needs actual source dispositions")
    for name, expected_hash in report["artifacts"].items():
        if digest(args.output / name) != expected_hash:
            raise ValueError("saved automatic module artifact changed: " + name)
    proof = {
        "generation_commit": report["commit"],
        "reader_commit": commit,
        "products_report_sha256": digest(args.output / "comparison.json"),
        "module_report_sha256": proofs,
        "chosen_module": chosen,
        "valid_automatic_module": chosen is not None,
        "valid_retained_baseline": chosen is None,
    }
    write_json(args.output / "verification.json", proof)
    print(json.dumps(proof), flush=True)
    return proof


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    verify(argument_parser().parse_args())

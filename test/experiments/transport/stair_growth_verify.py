"""Fresh saved-reader audit of bounded resident source discovery and actual meshes."""

import json
import resource
import time
from types import SimpleNamespace

from camera_height_inputs import digest
from stair_frontier_dispositions import source_dispositions
from stair_growth_experiment import arguments, selection
from stair_local_verify import verify as verify_module
from stair_module_experiment import write_json
from stair_terrain_experiment import frozen


def checked_selection(args, report):
    if (
        report["max_attempts"] != args.max_attempts
        or type(args.max_attempts) is not int
        or not 1 <= args.max_attempts <= 3
        or report["proposal_offset"] != args.proposal_offset
        or type(args.proposal_offset) is not int
        or not 0 <= args.proposal_offset <= 31
    ):
        raise ValueError("saved bounded attempt window changed")
    expected = selection(args)
    actual = json.loads((args.output / "selection.json").read_text())
    expected.pop("seconds")
    actual.pop("seconds")
    if (
        actual != json.loads(json.dumps(expected))
        or any(
            report[key] != actual[key]
            for key in ("resident_sha256", "parent_reports", "before_source_ids")
        )
        or report["proposal_count"] != len(actual["recipes"])
    ):
        raise ValueError("saved resident automatic source selection changed")
    return actual


def verify(args):
    commit, began = frozen(), time.perf_counter()
    report = json.loads((args.output / "comparison.json").read_text())
    selected = checked_selection(args, report)
    window = selected["recipes"][
        args.proposal_offset : args.proposal_offset + args.max_attempts
    ]
    chosen, proofs, actual = None, [], {"selection.json"}
    for index, attempt in enumerate(report["attempts"]):
        if index >= len(window) or chosen is not None:
            raise ValueError("unexpected or excessive automatic attempt")
        fixture = args.output / ("recipe-" + str(index) + ".json")
        output = args.output / ("outshine-module-" + str(index))
        recipe = json.loads(fixture.read_text())
        child = json.loads((output / "comparison.json").read_text())
        if (
            recipe != json.loads(json.dumps(window[index]))
            or attempt["index"] != index
            or attempt["source_ids"] != recipe["source_ids"]
            or attempt["reason"] != child.get("reason", "")
        ):
            raise ValueError("saved attempt lost its exact automatic recipe/reason")
        proof = verify_module(
            SimpleNamespace(
                resident=args.resident,
                prior_module=args.prior_module,
                fixture=fixture,
                output=output,
            )
        )
        if proof["valid_local_module"] != attempt["valid_local_module"]:
            raise ValueError("saved automatic physical admission changed")
        proofs.append(proof["products_report_sha256"])
        actual.update(
            [fixture.name, str(output.relative_to(args.output) / "comparison.json")]
        )
        if proof["valid_local_module"]:
            chosen = index
    if (
        report["chosen_module"] != chosen
        or report["valid_automatic_module"] != (chosen is not None)
        or report["valid_retained_baseline"] != (chosen is None)
        or (chosen is None and len(report["attempts"]) != len(window))
        or set(report["artifacts"]) != actual
        or report["source_dispositions"]
        != source_dispositions(selected["plan"], report["attempts"], chosen)
    ):
        raise ValueError("incomplete saved automatic selection/admission")
    for name, sha in report["artifacts"].items():
        if digest(args.output / name) != sha:
            raise ValueError("saved automatic artifact changed: " + name)
    proof = {
        "reader_commit": commit,
        "products_report_sha256": digest(args.output / "comparison.json"),
        "module_reports": proofs,
        "chosen_module": chosen,
        "valid_automatic_module": chosen is not None,
        "valid_retained_baseline": chosen is None,
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
    }
    write_json(args.output / "verification.json", proof)
    print(json.dumps(proof), flush=True)
    return proof


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (300, 300))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    verify(arguments())

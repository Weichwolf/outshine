"""Fresh full physical readers of a bounded saved batch without rewriting producers."""

import json
import os
import resource
import shutil
import time
from types import SimpleNamespace

from camera_height_inputs import digest
from stair_batch_experiment import checked_step
from stair_batch_inputs import (
    arguments,
    candidate,
    config_for,
    identity,
    limits,
    namespace,
    prefix,
)
from stair_growth_experiment import selection
from stair_local_replay import load_chain
from stair_local_verify import verify as verify_module
from stair_module_experiment import write_json
from stair_patch_inputs import fresh
from stair_prepared_surface_scope import optional_terrain
from stair_terrain_experiment import frozen


def read_step(args, path, step, prior, tried, output):
    config = {"selection": config_for(args, prior), "tried": sorted(tried)}
    if json.loads((path / "input.json").read_text()) != config:
        raise ValueError("saved batch changed its ordered inputs or attempted recipes")
    scoped = namespace(config["selection"])
    with optional_terrain(scoped, []):
        selected = selection(scoped)
        selected.pop("seconds")
        found = candidate(selected, tried)
        if identity(selected) != step["selection_sha256"]:
            raise ValueError("saved whole-source selection changed")
        if step["status"] == "exhausted":
            if found is not None:
                raise ValueError("untried automatic recipe was silently omitted")
            return None
        fixture = json.loads((path / "fixture.json").read_text())
        if found is None or found[0] != step["recipe_index"] or found[1] != fixture:
            raise ValueError("trial was not the first untried automatic recipe")
        clone = output / "outshine-reader-module"
        shutil.copytree(
            path / "outshine-module",
            clone,
            copy_function=os.link,
            ignore=shutil.ignore_patterns("verification.json", "display"),
        )
        try:
            receipt = verify_module(
                SimpleNamespace(
                    resident=args.resident,
                    prior_module=prior,
                    fixture=path / "fixture.json",
                    output=clone,
                )
            )
        finally:
            shutil.rmtree(clone)
    return receipt


def verify(args):
    commit, began = frozen(), time.perf_counter()
    limits(args)
    fresh(args.verification_output)
    args.verification_output.mkdir()
    report = json.loads((args.output / "comparison.json").read_text())
    current, resident_sha = prefix(args)
    if (
        report["before_source_ids"] != current
        or report["resident_sha256"] != resident_sha
        or report["selection_config"] != config_for(args, args.prior_module)
        or report["max_attempts"] != args.max_attempts
        or report["wall_budget_seconds"] != args.seconds
        or report["attempt_budget_seconds"] != args.attempt_seconds
        or len(report["attempts"]) > args.max_attempts
    ):
        raise ValueError("batch input or work limits changed")
    prior, tried, modules, proofs = list(args.prior_module), set(), [], []
    with optional_terrain(args, []):
        load_chain(args.resident, prior)
    terminal = None
    for index, record in enumerate(report["attempts"]):
        if record["index"] != index or terminal is not None:
            raise ValueError("noncontiguous trial or work after terminal failure")
        path = args.output / ("outshine-step-" + str(index))
        if record["input_sha256"] != digest(path / "input.json"):
            raise ValueError("saved attempted input changed")
        if record["status"] in ("timeout", "process_error"):
            terminal = record["status"]
            continue
        if record["step_sha256"] != digest(path / "step.json"):
            raise ValueError("saved complete step changed")
        step = json.loads((path / "step.json").read_text())
        if record["status"] != step["status"]:
            raise ValueError("saved step status changed")
        current = checked_step(path, step, current, tried)
        proof = read_step(args, path, step, prior, tried, args.verification_output)
        if proof is None:
            terminal = "bounded_proposals_exhausted"
            continue
        proof_name = "reader-" + str(index) + ".json"
        write_json(args.verification_output / proof_name, proof)
        proofs.append(digest(args.verification_output / proof_name))
        tried.add(step["recipe_sha256"])
        if step["status"] == "accepted":
            module = path / "outshine-module"
            prior.append(module)
            modules.append(str(module.relative_to(args.output)))
    expected_stop = terminal or (
        "attempt_budget"
        if len(report["attempts"]) == args.max_attempts
        else "wall_budget"
    )
    if (
        report["stop_reason"] != expected_stop
        or report["candidate_source_ids"] != current
        or report["modules"] != modules
        or report["admitted_source_ids"]
        or report["valid_verified_prefix"] is not True
    ):
        raise ValueError("batch prefix, termination or non-admission changed")
    proof = {
        "reader_commit": commit,
        "products_report_sha256": digest(args.output / "comparison.json"),
        "module_receipts": proofs,
        "candidate_source_ids": current,
        "valid_verified_prefix": True,
        "seconds": time.perf_counter() - began,
    }
    write_json(args.verification_output / "verification.json", proof)
    return proof


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (600, 600))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    verify(arguments())

"""Grow a valid prefix through a finite, wall-bounded sequence of physical trials."""

import json
import subprocess
import sys
import time
from pathlib import Path

from camera_height_inputs import digest
from stair_batch_inputs import arguments, config_for, identity, limits, prefix
from stair_module_experiment import write_json
from stair_patch_inputs import fresh
from stair_terrain_experiment import frozen


def isolated(path, seconds, *, worker=None):
    worker = worker or Path(__file__).with_name("stair_batch_step.py")
    with (path / "process.log").open("wb") as log:
        try:
            result = subprocess.run(
                [sys.executable, str(worker), str(path)],
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=seconds,
                check=False,
            )
        except subprocess.TimeoutExpired:
            return {"status": "timeout", "timeout_seconds": seconds}
    if result.returncode:
        return {"status": "process_error", "returncode": result.returncode}
    output = path / "step.json"
    if not output.exists():
        raise ValueError("successful process omitted its fresh complete export")
    return json.loads(output.read_text())


def checked_step(path, step, before, tried):
    if step["status"] not in ("accepted", "refused", "exhausted"):
        raise ValueError("unknown complete physical step status")
    if (
        step.get("valid_complete_step") is not True
        or step["input_sha256"] != digest(path / "input.json")
        or step["before_source_ids"] != before
    ):
        raise ValueError("complete step lost its input or valid prefix binding")
    if step["status"] == "exhausted":
        return before
    for name, sha in step["artifacts"].items():
        target = (path / name).resolve()
        if not target.is_relative_to(path.resolve()) or digest(target) != sha:
            raise ValueError("step export changed: " + name)
    recipe = json.loads((path / "fixture.json").read_text())
    report = json.loads((path / "outshine-module/comparison.json").read_text())
    receipt = json.loads((path / "outshine-module/verification.json").read_text())
    accepted = step["status"] == "accepted"
    expected = before + recipe["source_ids"] if accepted else before
    expected_files = {"source-catalog.json"}
    if accepted:
        expected_files.update(
            {
                "module.npz",
                "local-layout.npz",
                "local-field.npz",
                "selection.npz",
                "planning.json",
                "local-keys.json",
            }
        )
    if set(report["artifacts"]) != expected_files:
        raise ValueError("complete module export required")
    for name, sha in report["artifacts"].items():
        if digest(path / "outshine-module" / name) != sha:
            raise ValueError("module artifact changed: " + name)
    if (
        identity(recipe) != step["recipe_sha256"]
        or identity(recipe) in tried
        or recipe["source_ids"] != step["source_ids"]
        or set(recipe["source_ids"]) & set(before)
        or report["valid_local_module"] != accepted
        or receipt["valid_local_module"] != accepted
        or (not accepted and receipt.get("valid_retained_baseline") is not True)
        or report["fixture_sha256"] != digest(path / "fixture.json")
        or receipt["products_report_sha256"]
        != digest(path / "outshine-module/comparison.json")
        or report["candidate_source_ids"] != expected
        or step["candidate_source_ids"] != expected
    ):
        raise ValueError("unverified or repeated trial cannot extend the valid prefix")
    return expected


def run(args):
    commit, began = frozen(), time.perf_counter()
    limits(args)
    fresh(args.output)
    before, resident_sha = prefix(args)
    args.output.mkdir()
    prior, current, tried, attempts, modules = (
        list(args.prior_module),
        before,
        set(),
        [],
        [],
    )
    stop = "attempt_budget"
    deadline = began + args.seconds
    for index in range(args.max_attempts):
        remaining = deadline - time.perf_counter()
        if remaining <= 0:
            stop = "wall_budget"
            break
        path = args.output / ("outshine-step-" + str(index))
        path.mkdir()
        config = {"selection": config_for(args, prior), "tried": sorted(tried)}
        write_json(path / "input.json", config)
        step = isolated(path, min(remaining, args.attempt_seconds))
        record = {
            "index": index,
            "status": step["status"],
            "input_sha256": digest(path / "input.json"),
        }
        if step["status"] in ("timeout", "process_error"):
            record.update(step)
            attempts.append(record)
            stop = step["status"]
            break
        current = checked_step(path, step, current, tried)
        record["step_sha256"] = digest(path / "step.json")
        attempts.append(record)
        if step["status"] == "exhausted":
            stop = "bounded_proposals_exhausted"
            break
        tried.add(step["recipe_sha256"])
        if step["status"] == "accepted":
            module = path / "outshine-module"
            prior.append(module)
            modules.append(str(module.relative_to(args.output)))
        print(index, step["status"], len(current), flush=True)
    report = {
        "commit": commit,
        "selection_config": config_for(args, args.prior_module),
        "resident_sha256": resident_sha,
        "before_source_ids": before,
        "candidate_source_ids": current,
        "admitted_source_ids": [],
        "max_attempts": args.max_attempts,
        "wall_budget_seconds": args.seconds,
        "attempt_budget_seconds": args.attempt_seconds,
        "attempts": attempts,
        "modules": modules,
        "stop_reason": stop,
        "valid_verified_prefix": True,
        "seconds": time.perf_counter() - began,
        "scope": "Existing whole physical admission for each sequential module. Isolated trials share a real wall deadline; prefix metadata validation, process termination and final report IO add measured overhead. No maximality, native or device admission.",
    }
    write_json(args.output / "comparison.json", report)
    return report


if __name__ == "__main__":
    run(arguments())

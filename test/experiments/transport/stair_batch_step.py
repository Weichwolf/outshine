"""Isolated selection, construction and full admission of one next seed."""

import json
import resource
import sys
import time
from pathlib import Path
from types import SimpleNamespace

from camera_height_inputs import digest
from stair_batch_inputs import candidate, identity, namespace
from stair_growth_experiment import selection
from stair_local_generate import run as generate
from stair_local_verify import verify
from stair_module_experiment import write_json
from stair_prepared_surface_scope import optional_terrain
from stair_terrain_experiment import frozen


def run(path):
    began = time.perf_counter()
    config = json.loads((path / "input.json").read_text())
    args = namespace(config["selection"])
    with optional_terrain(args, []):
        selected = selection(args)
        elapsed = selected.pop("seconds")
        found = candidate(selected, set(config["tried"]))
        summary = {
            "commit": frozen(),
            "input_sha256": digest(path / "input.json"),
            "selection_sha256": identity(selected),
            "before_source_ids": selected["before_source_ids"],
            "selection_seconds": elapsed,
            "proposal_count": len(selected["recipes"]),
            "selection_work": selected["seed_work"],
            "valid_complete_step": True,
        }
        if found is None:
            summary.update(status="exhausted", seconds=time.perf_counter() - began)
            write_json(path / "step.json", summary)
            return summary
        index, recipe = found
        write_json(path / "fixture.json", recipe)
        child = SimpleNamespace(
            resident=args.resident,
            prior_module=args.prior_module,
            fixture=path / "fixture.json",
            output=path / "outshine-module",
            max_context_faces=8192,
        )
        report = generate(child)
        receipt = verify(child)
        summary.update(
            recipe_index=index,
            recipe_sha256=identity(recipe),
            source_ids=recipe["source_ids"],
            status="accepted" if report["valid_local_module"] else "refused",
            reason=report.get("reason", ""),
            artifacts={
                name: digest(path / name)
                for name in (
                    "fixture.json",
                    "outshine-module/comparison.json",
                    "outshine-module/verification.json",
                )
            },
            candidate_source_ids=report["candidate_source_ids"],
            products_report_sha256=receipt["products_report_sha256"],
            seconds=time.perf_counter() - began,
            peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        )
        write_json(path / "step.json", summary)
        return summary


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(Path(sys.argv[1]))

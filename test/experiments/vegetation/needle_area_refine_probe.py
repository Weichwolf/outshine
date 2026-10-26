"""Measure a local refinement heuristic against an independent stored full query."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from needle_area_heldout import difference
from needle_joint_contacts import prepare_source
from needle_response_study import source_digest
from surface_area_light import integrate


def query(source, scene, view, lights, mask, order):
    before = dict(scene.work)
    began = time.perf_counter()
    result, work = integrate(source, scene, view, lights, order=order, pixel_mask=mask)
    return result, {
        "work": work,
        "wall_s": time.perf_counter() - began,
        "rays": {k: scene.work[k] - before[k] for k in before},
    }


def run(source_path, parent, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    previous = json.loads((parent / "report.json").read_text())
    if previous["source_sha256"] != source_digest(source_path):
        raise ValueError("same immutable selected source required")
    index = previous["selected_view"]
    record = previous["views"][index]
    with np.load(parent / "fields.npz") as fields:
        q2, q4, q6 = (fields[f"view-{index}-q{order}"] for order in (2, 4, 6))
    mask = abs(q2 - q4).max(0) > 0.002
    output.mkdir(parents=True, exist_ok=False)
    source, scene = prepare_source(source_path)
    view = np.array(record["view"])
    lights = np.array(record["lights"])
    replay, replay_work = query(source, scene, view, lights, mask, 4)
    np.testing.assert_allclose(replay[:, mask], q4[:, mask], rtol=0, atol=1e-12)
    refined, refinement_work = query(source, scene, view, lights, mask, 8)
    hybrid = np.where(mask[None], refined, q4)
    differences = abs(q4 - q6).max(0)
    result = {
        "schema": "outshine-needle-area-demand-probe-v1",
        "source_sha256": source_digest(source_path),
        "parent_report_sha256": source_digest(parent / "report.json"),
        "parent_fields_sha256": source_digest(parent / "fields.npz"),
        "selected_view": index,
        "trigger": 0.002,
        "requested_pixels": int(mask.sum()),
        "total_pixels": mask.size,
        "q4_q6": difference(q4, q6),
        "hybrid_q6": difference(hybrid, q6),
        "unselected_max_abs": float(differences[~mask].max(initial=0)),
        "unselected_pixels_over_trigger": int(((differences > 0.002) & ~mask).sum()),
        "partial_replay_max_abs": float(
            abs(replay[:, mask] - q4[:, mask]).max(initial=0)
        ),
        "replay_work": replay_work,
        "refinement_work": refinement_work,
        "uniform_q6_work": previous["refinement"],
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=Path(__file__).parent, text=True
        ).strip(),
    }
    result["implementation_sha256"] = {
        name: source_digest(Path(__file__).with_name(name))
        for name in (
            "needle_area_refine_probe.py",
            "surface_area_light.py",
            "surface_area_cells.py",
        )
    }
    np.savez_compressed(
        output / "fields.npz", requested=mask, partial_q8=refined, hybrid=hybrid
    )
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in (
                    "requested_pixels",
                    "q4_q6",
                    "hybrid_q6",
                    "unselected_max_abs",
                    "unselected_pixels_over_trigger",
                )
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.parent, args.output)

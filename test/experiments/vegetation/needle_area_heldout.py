"""Frozen held-out directions; refine and bias-probe the largest observed residual."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from needle_area_heldout_plot import plot
from needle_area_light import integrate
from needle_joint_contacts import prepare_source
from needle_response_study import source_digest


def evaluate(source, scene, view, lights, order, bias, size):
    before = dict(scene.work)
    began = time.perf_counter()
    fields, work = integrate(
        source, scene, view, lights, size=size, order=order, bias=bias
    )
    facts = {
        "order": order,
        "bias_m": bias,
        "wall_s": time.perf_counter() - began,
        "work": work,
        "ray_work": {k: scene.work[k] - before[k] for k in before},
        "backlight_max": float(fields[4].max()),
        "backlight_mean": float(fields[4].mean()),
    }
    np.testing.assert_array_equal(fields[1], fields[2])
    assert np.isfinite(fields).all() and np.all(fields >= 0)
    return fields, facts


def difference(a, b):
    delta = a - b
    return {
        "max_abs": float(abs(delta).max()),
        "rmse": float(np.sqrt(np.mean(delta * delta))),
        "radiance_max_abs": float(abs(delta[2::2]).max()),
        "radiance_rmse": float(np.sqrt(np.mean(delta[2::2] ** 2))),
    }


def run(source_path, scenario_path, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under/tmp")
    if scenario_path.stat().st_size > 4096:
        raise ValueError("bounded held-out scenario required")
    control = json.loads(scenario_path.read_text())
    if (
        control["schema"] != "outshine-needle-area-heldout-v1"
        or len(control["views"]) != 3
        or len(control["world_lights"]) != 4
        or control["orders"] != [2, 4]
        or len(control["probe_biases_m"]) != 2
    ):
        raise ValueError("frozen bounded held-out scenario required")
    output.mkdir(parents=True, exist_ok=False)
    source, scene = prepare_source(source_path)
    views = np.array(control["views"], float)
    views /= np.linalg.norm(views, axis=1)[:, None]
    world = np.array(control["world_lights"], float)
    world /= np.linalg.norm(world, axis=1)[:, None]
    arrays = {}
    reports = []
    directions = []
    for index, view in enumerate(views):
        lights = np.r_[view[None], -view[None], world]
        directions.append(lights)
        facts = {"view": view.tolist(), "lights": lights.tolist(), "orders": []}
        for order in control["orders"]:
            fields, record = evaluate(
                source,
                scene,
                view,
                lights,
                order,
                control["central_bias_m"],
                control["native_size"],
            )
            arrays[f"view-{index}-q{order}"] = fields
            facts["orders"].append(record)
        facts["q2_q4"] = difference(
            arrays[f"view-{index}-q2"], arrays[f"view-{index}-q4"]
        )
        reports.append(facts)
    worst = max(range(3), key=lambda i: reports[i]["q2_q4"]["max_abs"])
    fields, refinement = evaluate(
        source,
        scene,
        views[worst],
        directions[worst],
        control["refinement_order"],
        control["central_bias_m"],
        control["native_size"],
    )
    arrays[f"view-{worst}-q6"] = fields
    refinement["q4_q6"] = difference(arrays[f"view-{worst}-q4"], fields)
    probes = []
    for bias in control["probe_biases_m"]:
        fields, record = evaluate(
            source,
            scene,
            views[worst],
            directions[worst],
            4,
            bias,
            control["native_size"],
        )
        record["against_central_q4"] = difference(fields, arrays[f"view-{worst}-q4"])
        arrays[f"view-{worst}-bias-{bias}"] = fields
        probes.append(record)
    result = {
        "schema": control["schema"],
        "source_sha256": source_digest(source_path),
        "source_owner": json.loads(source_path.with_name("source.json").read_text()),
        "scenario_sha256": source_digest(scenario_path),
        "scenario": control,
        "views": reports,
        "selected_view": worst,
        "refinement": refinement,
        "bias_probes": probes,
        "bvh_builds": 1,
        "bvh_setup": scene.index.setup,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=Path(__file__).parent, text=True
        ).strip(),
    }
    result["implementation_sha256"] = {
        name: source_digest(Path(__file__).with_name(name))
        for name in (
            "needle_area_heldout.py",
            "needle_area_heldout_plot.py",
            "needle_area_light.py",
            "needle_area_cells.py",
            "needle_joint_contacts.py",
        )
    }
    np.savez_compressed(output / "fields.npz", **arrays)
    plot(arrays, output / "comparison.png")
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "selected_view": worst,
                "view_errors": [r["q2_q4"] for r in reports],
                "refinement": refinement["q4_q6"],
                "bias_errors": [r["against_central_q4"] for r in probes],
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument(
        "--scenario",
        type=Path,
        default=Path(__file__).with_name("data") / "needle-area-heldout.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.scenario, args.output)

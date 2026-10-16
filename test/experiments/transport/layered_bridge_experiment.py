"""Stage locally valid profiles; rejected work retains the complete original scene."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from curb_experiment import save_products
from layered_bridge_display import scenes
from layered_bridge_inputs import base_inputs, retained_proof
from layered_bridge_plan import candidate
from layered_bridge_problem import prepare
from layered_bridge_supports import layout
from layered_bridge_topology import topology
from outshine_ref.contracts import Mesh

HERE = Path(__file__).resolve().parent


def store_profile(output, name, value, piers):
    p, h, ribbon, note = value
    target = output / (name + ".npz")
    np.savez_compressed(
        target,
        body_vertices=ribbon.body.vertices,
        body_faces=ribbon.body.triangles,
        lower_vertices=ribbon.lower.vertices,
        upper_vertices=ribbon.upper.vertices,
        cap_faces=ribbon.upper.triangles,
        table=np.array(note["geometry"]["table"]),
    )
    save_products(output / (name + "-piers.npz"), piers)
    return {
        "body_sha256": digest(target),
        "piers_sha256": digest(output / (name + "-piers.npz")),
        "profile_input": {k: p[k] for k in ("stations", "lower", "upper", "rate")},
        "heights": h,
    }


def source_ledger(args, fixture, accepted, reports):
    rows = json.loads((args.baseline / "source-usage.json").read_text())
    groups = {r["source_id"]: r["profile"] for r in fixture["sources"]}
    for row in rows:
        profile = groups.get(row["source_id"])
        if profile is None:
            continue
        if profile in accepted:
            row.update(
                status="used", reason="", height_owner="layered_bridge/" + profile
            )
        elif row["status"] == "not_used":
            row.update(
                previous_reason=row["reason"],
                reason="layered_profile: " + reports[profile]["reason"],
            )
    path = args.output / "source-usage.json"
    path.write_text(json.dumps(rows, indent=2) + "\n")
    return digest(path), sum(r["status"] == "used" for r in rows)


def stages(args, fixture, ground, borders, chains):
    field = Mesh(np.array(fixture["field_vertices"]), np.array(fixture["field_faces"]))
    accepted, reports, bodies, inputs = {}, {}, [], {}
    for name in ("path", "motorway"):
        lower = accepted.get("path") if name == "motorway" else None
        value, note = candidate(fixture, name, lower=lower)
        note["requested_sources"] = [
            r["source_id"] for r in fixture["sources"] if r["profile"] == name
        ]
        if value is not None:
            try:
                _patch, protection = retained_proof(args, value, ground, borders, field)
                comparison = {}
                choices = {}
                for method in ("fixed", "shifted"):
                    products, result = layout(
                        fixture, value, chains, lower=lower, method=method
                    )
                    comparison[method] = result
                    choices[method] = products
                method = "fixed" if name == "path" else args.support_method
                if not comparison[method]["valid"]:
                    note["support_comparison"] = comparison
                    raise ValueError("support_station_gap_exceeds_recipe")
                piers = choices[method]
                saved = store_profile(args.output, name, value, piers)
                inputs.update(
                    {name + "_" + k: v for k, v in saved.pop("profile_input").items()}
                )
                inputs[name + "_heights"] = saved.pop("heights")
                note.update(
                    protection=protection,
                    support_comparison=comparison,
                    support_method=method,
                    **saved,
                )
            except ValueError as error:
                note.update(
                    valid=False,
                    reason=str(error),
                    retained_previous_source_decisions=True,
                )
            else:
                accepted[name] = value
                bodies.extend([(value[2].body, 3), *[(p.body, 10) for p in piers]])
        reports[name] = note
    return accepted, reports, bodies, inputs


def generate(args):
    began = time.perf_counter()
    if (
        args.output.exists()
        or subprocess.check_output(
            ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
        ).strip()
    ):
        raise ValueError("fresh output and frozen clean research source required")
    before, ground, borders = base_inputs(args)
    fixture = json.loads(args.fixture.read_text())
    root = prepare(fixture, "motorway")
    chains, travel = topology(fixture, root["axis"])
    args.output.mkdir(parents=True)
    accepted, reports, bodies, inputs = stages(args, fixture, ground, borders, chains)
    np.savez_compressed(args.output / "profile-input.npz", **inputs)
    usage, used = source_ledger(args, fixture, accepted, reports)
    display = scenes(args, before, bodies)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "fixture_sha256": digest(args.fixture),
        "admitted_profiles": list(accepted),
        "candidates": reports,
        "attempted_source_flow": travel,
        "source_usage_sha256": usage,
        "used_sources": used,
        "profile_input_sha256": digest(args.output / "profile-input.npz"),
        "presentation": display,
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "status": "valid_partial_scene_requires_saved_mesh_replay",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "commit",
                    "admitted_profiles",
                    "used_sources",
                    "seconds",
                    "peak_rss_kib",
                )
            }
        ),
        flush=True,
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture", type=Path, default=HERE / "fixtures/layered-bridges-vienna.json"
    )
    parser.add_argument(
        "--support-method", choices=("fixed", "shifted"), default="shifted"
    )
    for name in (
        "network",
        "baseline",
        "borders",
        "supports",
        "reference-root",
        "output",
    ):
        parser.add_argument("--" + name, type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3, 4 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    generate(arguments())

"""Measure explicit fixed-contact binding and its actual native-point limitations."""

import argparse
import hashlib
import json
import math
import resource
import subprocess
import sys
import time
import tracemalloc
from fractions import Fraction
from pathlib import Path

import numpy as np
from mvt_t_connection_inputs import FIXTURE, inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_contacts import (
    audit_fixed_contacts,
    contact_residuals,
    prepare_contacts,
)
from mvt_t_profile_layout import prepare_profiles, prior_values
from mvt_t_profile_products import shared_products
from mvt_t_profile_solve import solve_profiles
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_junction_body import build
from stair_network_geometry import vertex_links


def fixed(key, source, xyz, provenance):
    return {
        "contact_id": key,
        "source_id": source,
        "xyz_m": xyz,
        "binding": "fixed",
        "provenance": provenance,
    }


def native_contacts(case, sources, way_id, ends):
    owner = next(i for i, way in enumerate(case["ways"]) if way["native_way"] == way_id)
    return [
        fixed(
            f"native:{way_id}:{end}",
            sources[owner]["source_id"],
            [case["ways"][owner]["prepared_stations_xyz_m"][end]],
            "recorded native point, fixed only for this conditional test; boundary role unqualified",
        )
        for end in ends
    ]


def native_changes(plan, heights, case, sources):
    rows = []
    for owner, (way, source) in enumerate(zip(case["ways"], sources, strict=True)):
        sampler = SurfaceSampler(Mesh(np.c_[plan["xy"], heights], plan["parts"][owner]))
        xyz = np.array(way["prepared_stations_xyz_m"])
        result = sampler.sample(xyz[:, :2])
        if any(result.reasons):
            raise ValueError(
                "recorded native comparison point is not on the selected source"
            )
        rows.append(
            {
                "source_id": source["source_id"],
                "recorded_xyz_m": xyz.tolist(),
                "new_minus_recorded_z_m": (
                    result.points_enu_m[:, 2] - xyz[:, 2]
                ).tolist(),
            }
        )
    return rows


def finite_prior_bound(plan, contact):
    datum = float(
        np.median(np.concatenate([r["height_prior_xyz_m"][:, 2] for r in plan["rows"]]))
    )
    row = plan["rows"][contact["owner"]]
    vertices = np.unique(plan["parts"][contact["owner"]])
    prior = prior_values(row, plan["xy"][vertices]) - datum
    lookup = {
        int(v): Fraction.from_float(float(p))
        for v, p in zip(vertices, prior, strict=True)
    }
    bounds = []
    for face, weights, target in zip(
        contact["faces"], contact["weights"], contact["target_z_m"], strict=True
    ):
        w = [Fraction.from_float(float(v)) for v in weights]
        rhs = Fraction.from_float(float(target - datum))
        estimate = sum(a * lookup[int(i)] for a, i in zip(w, face, strict=True))
        bounds.append(abs(rhs - estimate) / sum(abs(a) for a in w))
    bound = max(bounds)
    lower = float(bound)
    if Fraction.from_float(lower) > bound:
        lower = math.nextafter(lower, -math.inf)
    return {
        "lower_bound_m": lower,
        "exact_fraction": str(bound),
        "derivation": "abs(target - sum(weight * vertex_prior)) / sum(abs(weight))",
        "scope": "exact for these binary64 LP coefficients and vertex-prior bounds; not a different mesh or world impossibility",
    }


def solve_case(plan, contacts, method):
    began = time.perf_counter()
    heights, note = solve_profiles(plan, fixed_contacts=contacts, contact_method=method)
    report = {"solve": note, "fixed_inputs": contacts}
    data = None
    if heights is not None:
        report["contact_audit"] = audit_fixed_contacts(plan, heights, contacts)
        body, data, report["geometry"] = build(plan, heights)
        report["profile_audit"] = audit_profiles(plan, heights, data)
        vertex_links(body.vertices, body.triangles)
    report["wall_s"] = time.perf_counter() - began
    return heights, data, report


def run(method, *, output=None, trace=False):
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    case, sources = inputs()
    plan = prepare_profiles(
        case, sources, plan_connections(sources), mesh_recipe="outward_binary_grid"
    )
    baseline, note = solve_profiles(plan)
    if baseline is None:
        raise ValueError(note)
    _body, original, _geometry = build(plan, baseline)
    baseline_products = shared_products(plan, baseline, original)
    outer = [
        fixed(
            f"synthetic:{i}",
            p["source_id"],
            p["profile_xyz_m"],
            "synthetic roundtrip of baseline outer profile; not native boundary evidence",
        )
        for i, p in enumerate(baseline_products["external_contacts"])
    ]
    prepared, work = prepare_contacts(plan, outer, method=method)
    digest = hashlib.sha256()
    for item in prepared:
        for key in ("faces", "xy", "weights", "target_z_m"):
            digest.update(item[key].tobytes())
    _roundtrip, _rd, roundtrip = solve_case(plan, outer, method)
    upper = native_contacts(case, sources, 431, (0, -1))
    matched, matched_data, upper_report = solve_case(plan, upper, method)
    lower = native_contacts(case, sources, 432, (0,))
    _h, _d, lower_report = solve_case(plan, lower, method)
    lower_rows, _work = prepare_contacts(plan, lower, method=method)
    controls = {}
    for scale, presolve in ((1.0, True), (1024.0, True)):
        result, control = solve_profiles(
            plan,
            fixed_contacts=outer,
            contact_row_scale=scale,
            contact_presolve=presolve,
        )
        if result is not None:
            control["independent_audit"] = audit_fixed_contacts(plan, result, outer)
        controls[f"scale={scale},presolve={presolve}"] = control
    if (
        matched is None
        or not roundtrip["solve"]["valid"]
        or lower_report["solve"]["valid"]
    ):
        raise ValueError(
            "positive controls failed or the measured finite budget countercase changed"
        )
    products = shared_products(plan, matched, matched_data)
    source_id = upper[0]["source_id"]
    result = {
        "method": method,
        "contact_work": work,
        "contact_coefficient_sha256": digest.hexdigest(),
        "baseline_solve": note,
        "baseline_native_changes": native_changes(plan, baseline, case, sources),
        "baseline_roundtrip_residuals_m": contact_residuals(prepared, baseline),
        "whole_profile_roundtrip": roundtrip,
        "conditional_upper_points": upper_report,
        "conditional_lower_point": lower_report,
        "finite_lower_bound": finite_prior_bound(plan, lower_rows[0]),
        "conditioning_controls": controls,
        "elapsed_s": time.perf_counter() - began,
        "memory_scope": "fresh worker through evaluation, excluding artifact export/plot; live blocks are not all allocations",
        "plot_series": {
            "upper_before": baseline_products["navigation"]["axis_profiles_xyz_m"][
                source_id
            ],
            "upper_after": products["navigation"]["axis_profiles_xyz_m"][source_id],
            "upper_native": case["ways"][1]["prepared_stations_xyz_m"],
        },
    }
    if trace:
        _current, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    else:
        peak, blocks = None, None
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    result.update(
        python_traced_peak_bytes=peak,
        python_traced_live_blocks=blocks,
        evaluation_peak_rss_bytes=rss if sys.platform == "darwin" else rss * 1024,
    )
    if output is not None:
        np.savez_compressed(
            output / "conditional-upper.npz",
            xy=plan["xy"],
            heights=matched,
            **matched_data,
            **{f"source_faces_{i}": f for i, f in enumerate(plan["parts"])},
        )
        products["fixed_contact_inputs"] = upper
        products["contact_qualification"] = (
            "conditional point controls; native outer boundary roles remain unqualified"
        )
        (output / "conditional-upper-plan.json").write_text(
            json.dumps(products, indent=2) + "\n"
        )
    return result


def plot(report, path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import shapely

    result = report["index"]
    fig, axes = plt.subplots(2, 2, figsize=(12, 8))
    labels, residuals = [], []
    for row in result["baseline_native_changes"]:
        residuals.extend(row["new_minus_recorded_z_m"])
        labels.extend(
            f"{row['source_id'].split(':')[-1]}/{i}"
            for i in range(len(row["new_minus_recorded_z_m"]))
        )
    axes[0, 0].bar(range(len(labels)), residuals)
    axes[0, 0].set_xticks(range(len(labels)), labels, rotation=45)
    axes[0, 0].set(
        title="Unbound plan minus recorded native points",
        ylabel="height difference / m",
    )
    original = np.array(result["plot_series"]["upper_native"])
    line = shapely.LineString(original[:, :2])
    for key, label in (
        ("upper_before", "unbound proposal"),
        ("upper_after", "conditional fixed endpoints"),
    ):
        xyz = np.array(result["plot_series"][key])
        station = shapely.line_locate_point(line, shapely.points(xyz[:, :2]))
        axes[0, 1].plot(station, xyz[:, 2], label=label)
    arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(original[:, :2], axis=0), axis=1))]
    axes[0, 1].plot(arc, original[:, 2], "x--", label="recorded native profile")
    axes[0, 1].set(
        title="Actual upper-source fields",
        xlabel="axis station / m",
        ylabel="height / m",
    )
    axes[0, 1].legend(fontsize=8)
    bound = result["finite_lower_bound"]["lower_bound_m"]
    axes[1, 0].bar(
        ["declared budget", "fixed-point lower bound"],
        [2, bound],
        color=["#387a9d", "#b75c49"],
    )
    axes[1, 0].set(
        title="Only if the lower native point is fixed",
        ylabel="finite prior-change requirement / m",
    )
    axes[1, 0].text(1, bound + 0.02, f"{bound:.6f} m", ha="center")
    axes[1, 1].plot(
        [0, 1, 2], [0, 1, 0], "o-", label="facet field with matching endpoints"
    )
    axes[1, 1].plot([0, 2], [0, 0], "o--", label="required whole profile")
    axes[1, 1].set(
        title="Analytic control: endpoints hide a 1-m mismatch",
        xlabel="profile station / m",
        ylabel="height / m",
    )
    axes[1, 1].legend(fontsize=8)
    for axes_item in axes.ravel():
        axes_item.grid(alpha=0.2)
    fig.suptitle(
        "Explicit contact binding; native boundary roles and world admission remain open"
    )
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def invoke(method, *, output=None, trace=False):
    cmd = [sys.executable, str(Path(__file__).resolve()), "--worker", method]
    if output is not None:
        cmd += ["--output", str(output)]
    if trace:
        cmd += ["--trace-memory"]
    completed = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if completed.returncode:
        raise RuntimeError(completed.stderr)
    return json.loads(completed.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--worker", choices=("index", "scan"))
    parser.add_argument("--trace-memory", action="store_true")
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(run(args.worker, output=args.output, trace=args.trace_memory)))
        return
    if args.output is None:
        parser.error("an explicit output directory is required")
    args.output.mkdir(parents=True, exist_ok=True)
    report = {
        method: invoke(method, output=args.output if method == "index" else None)
        for method in ("index", "scan")
    }
    report["memory"] = invoke("index", trace=True)
    if (
        report["index"]["contact_coefficient_sha256"]
        != report["scan"]["contact_coefficient_sha256"]
    ):
        raise ValueError("complete indexed/reference contact coefficients differ")
    report["commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    report["input_sha256"] = hashlib.sha256(FIXTURE.read_bytes()).hexdigest()
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    plot(report, args.output / "contacts.png")
    print(json.dumps({key: report[key]["contact_work"] for key in ("index", "scan")}))


if __name__ == "__main__":
    main()

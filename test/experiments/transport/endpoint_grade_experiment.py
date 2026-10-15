"""Identical saved axes and complete selected contacts; local failures retain sources."""

import argparse
import hashlib
import json
import time
from collections import Counter
from pathlib import Path

import numpy as np
from endpoint_grade_contacts import ContactSurface
from outshine_ref.contracts import Mesh
from outshine_ref.transport import (
    close_surface,
    plan_endpoint_transition,
    plan_vertical_transition,
    sample_vertical_transition,
)
from surface_height_checks import normal_gradients
from surface_solid_checks import audit_closed
from vertical_transition_experiment import ribbon

HERE = Path(__file__).resolve().parent
FIXTURES = HERE / "fixtures"
OUTPUT = HERE.parents[2] / "build/experiments/transport/endpoint-grades"
AXES = FIXTURES / "vertical-transition-cases.json"
SURFACE = FIXTURES / "surface-sampling-vienna-4ea502363.npz"
RECEIPT = SURFACE.with_suffix(".json")


def prepare_cases():
    began = time.perf_counter_ns()
    receipt = json.loads(RECEIPT.read_text())
    if hashlib.sha256(SURFACE.read_bytes()).hexdigest() != receipt["fixture_sha256"]:
        raise ValueError("selected surface bytes changed")
    with np.load(SURFACE) as data:
        selected = ContactSurface(Mesh(data["vertices"], data["triangles"]))
        road_faces = int(data["road_face_count"])
    preparation_ns = time.perf_counter_ns() - began
    began = time.perf_counter_ns()
    rows = []
    for original in json.loads(AXES.read_text())["cases"]:
        xy = np.asarray(original["xy"])
        direction = xy[1] - xy[0]
        length = float(np.linalg.norm(direction))
        direction /= length
        width = 4.0 if original["width_m"] is None else original["width_m"]
        if not np.isfinite(width) or width <= 0:
            rows.append(dict(original, valid=False, reason="missing_source_width"))
            continue
        ports = [
            selected.strip(xy[i], direction, width, exterior_side=side)
            for i, side in ((0, -1), (1, 1))
        ]
        for port in ports:
            port["face_kinds"] = [
                "road" if i < road_faces else "exterior-soil"
                for i in port.get("face_indices", [])
            ]
        reason = next((p["reason"] for p in ports if not p["valid"]), "")
        if not reason and abs(ports[0]["crossfall"] - ports[1]["crossfall"]) > 1e-10:
            reason = "incompatible_contact_crossfalls"
        crossfall = ports[0].get("crossfall", 0)
        if not reason and abs(crossfall) >= 0.025:
            reason = "contact_crossfall_out_of_bounds"
        row = dict(
            original,
            contacts=ports,
            valid=not reason,
            reason=reason,
            length_m=length,
            direction=direction.tolist(),
            recipe_width_m=float(width),
            width_provenance="source"
            if original["width_m"] is not None
            else "explicit-4m-recipe-as-PR96",
        )
        if not reason:
            row.update(
                end_heights_m=[p["center_height_m"] for p in ports],
                end_grades=[p["longitudinal_grade"] for p in ports],
                crossfall=crossfall,
                grade_bound=original["longitudinal_grade"]
                * np.sqrt(1 - (crossfall / 0.025) ** 2),
            )
        rows.append(row)
    return rows, preparation_ns, time.perf_counter_ns() - began


def plan_case(row, method):
    return plan_endpoint_transition(
        *row["end_heights_m"],
        row["length_m"],
        *row["end_grades"],
        grade=row["grade_bound"],
        second_derivative=0.002,
        method=method,
    )


def audit_ribbon(row, profile, table):
    direction = np.asarray(row["direction"])
    mesh = ribbon(
        np.asarray(row["xy"][0]),
        direction,
        table,
        row["recipe_width_m"],
        row["crossfall"],
    )
    gradients = normal_gradients(
        mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]
    )
    transverse = np.array([-direction[1], direction[0]])
    frame = np.array([direction / row["longitudinal_grade"], transverse / 0.025])
    normalized = float(np.linalg.norm(gradients @ frame.T, axis=1).max())
    if normalized > 1.0000001:
        raise ValueError("whole finite ribbon violates its elliptic grade limit")
    z = profile.evaluate([0, row["length_m"]])
    heights = np.c_[
        z - row["crossfall"] * row["recipe_width_m"] / 2,
        z + row["crossfall"] * row["recipe_width_m"] / 2,
    ]
    target = np.array(row["end_heights_m"])[:, None] + row["crossfall"] * np.array(
        [-row["recipe_width_m"] / 2, row["recipe_width_m"] / 2]
    )
    height_error = float(abs(heights - target).max())
    grade_error = float(
        abs(profile.evaluate([0, row["length_m"]], 1) - row["end_grades"]).max()
    )
    query = np.linspace(0, row["length_m"], 4097)
    chord_error = float(
        abs(profile.evaluate(query) - np.interp(query, table[:, 0], table[:, 1])).max()
    )
    if height_error > 1e-7 or grade_error > 1e-10 or chord_error > 0.002 + 1e-7:
        raise ValueError("full-width endpoint or independent chord test failed")
    body = close_surface(mesh, 0.25)
    volume = audit_closed(body)
    if not np.isclose(
        volume, row["length_m"] * row["recipe_width_m"] * 0.25, atol=1e-6, rtol=1e-9
    ):
        raise ValueError("ribbon body topology/volume changed")
    return mesh, {
        "maximum_normalized_face_grade": normalized,
        "full_width_contact_height_error_m": height_error,
        "analytic_contact_grade_error": grade_error,
        "independent_chord_error_m": chord_error,
        "closed_oriented_body_volume_m3": volume,
        "display_depth_m": 0.25,
    }


def worker(method, output):
    output.mkdir(parents=True, exist_ok=True)
    rows, preparation_ns, contacts_ns = prepare_cases()
    compatible = [r for r in rows if r["valid"]]
    began = time.perf_counter_ns()
    plan_endpoint_transition(0, 1, 100, 0, 0, method=method)
    first_profile_ns = time.perf_counter_ns() - began
    timings = []
    for _ in range(7):
        began = time.perf_counter_ns()
        profiles = [plan_case(row, method) for row in compatible]
        elapsed = time.perf_counter_ns() - began
        timings.append(elapsed)
    mesh_began = time.perf_counter_ns()
    for index, (row, (profile, note)) in enumerate(zip(compatible, profiles)):
        row.update(valid=note["valid"], reason=note["reason"], profile=note)
        if profile is None:
            continue
        table, sampling = sample_vertical_transition(profile, height_error_m=0.002)
        if table is None:
            row.update(valid=False, reason=sampling["reason"])
            continue
        mesh, check = audit_ribbon(row, profile, table)
        row.update(audit=check, sampling=sampling)
        baseline, flat_note = plan_vertical_transition(
            *row["end_heights_m"], row["length_m"], grade=row["grade_bound"]
        )
        row["flat_endpoint_baseline"] = {
            "valid": flat_note["valid"],
            "analytic_contact_grade_jump": float(
                max(abs(np.asarray(row["end_grades"])))
            ),
            "assumption": "Flat-end comparator deliberately ignores actual nonzero parent grades; diagnostic only.",
        }
        if baseline is not None:
            flat_table, _ = sample_vertical_transition(baseline, height_error_m=0.002)
            np.savez_compressed(output / f"case-{index:03d}-flat.npz", table=flat_table)
        path = output / f"case-{index:03d}.npz"
        np.savez_compressed(
            path,
            vertices=mesh.vertices,
            triangles=mesh.triangles,
            table=table,
            breaks=profile.breaks,
            coefficients=profile.coefficients,
        )
        row["mesh"] = str(path)
    mesh_ns = time.perf_counter_ns() - mesh_began
    hwm = next(
        line
        for line in Path("/proc/self/status").read_text().splitlines()
        if line.startswith("VmHWM:")
    )
    report = {
        "method": method,
        "cases": rows,
        "omissions": dict(Counter(r["reason"] for r in rows if not r["valid"])),
        "valid": sum(r["valid"] for r in rows),
        "compatible_contacts": len(compatible),
        "plan_batch_ns": timings,
        "first_lazy_import_and_profile_ns": first_profile_ns,
        "surface_preparation_ns": preparation_ns,
        "contact_selection_ns": contacts_ns,
        "mesh_sample_audit_write_ns": mesh_ns,
        "current_exec_peak_rss_bytes": int(hwm.split()[1]) * 1024,
        "inputs": {
            str(p.relative_to(HERE.parents[2])): hashlib.sha256(
                p.read_bytes()
            ).hexdigest()
            for p in (AXES, SURFACE, RECEIPT)
        },
        "frame": json.loads(RECEIPT.read_text())["frame"],
        "selected_surface_kind": "shared-road-soil-height-mesh",
        "scope": "Fixed original axes and source widths where present; otherwise explicit unchanged PR96 4m recipe, recorded separately from absent source width. All compatible contacts in this corpus lie on exterior-soil faces of one affine plane, not existing road-junction contacts. Independent ribbons, not assembled replacement network/terrain or runtime. C1 analytic profile; finite faces C0. Incompatible contacts remain local omissions. No flat-port pins changed.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in ("method", "valid", "omissions", "current_exec_peak_rss_bytes")
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--method", choices=("envelope", "hermite"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    worker(args.method, args.output)


if __name__ == "__main__":
    main()

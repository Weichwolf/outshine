from outshine_ref.transport.port_heights._quality import quality

"""Refine complete C1 road plans and refit matching soil within bounded work (#169)."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from outshine_ref.transport.port_heights._contacts import contact_audit
from outshine_ref.transport.port_heights._layout import prepare_plane_field
from outshine_ref.transport.port_heights._system import continuous_matrix, fit_planes
from outshine_ref.transport.port_heights._world import (
    assemble_world,
)
from port_plane_fixture import REFERENCE, load_reference
from port_plane_refinement import refine

ROOT = Path(__file__).resolve().parents[3]


def resample_road(field, coefficients, datum, mesh):
    parts = np.full(len(mesh["xy"]), -1, int)
    road = mesh["parts"] >= 0
    np.maximum.at(parts, mesh["faces"][road].ravel(), np.repeat(mesh["parts"][road], 3))
    selected = np.flatnonzero(parts >= 0)
    values, _ = field.evaluate(mesh["xy"][selected], parts[selected], coefficients)
    mesh["heights"][selected] = values + datum


def refine_geometry(field, coefficients, datum, mesh, roles, *, levels=6):
    reports = []
    for level in range(levels + 1):
        marked, audit, errors = quality(field, coefficients, mesh, roles)
        print(
            json.dumps(
                dict(
                    stage="refinement",
                    level=level,
                    vertices=len(mesh["xy"]),
                    faces=len(mesh["faces"]),
                    **audit,
                )
            ),
            flush=True,
        )
        if not marked.any() or level == levels:
            return mesh, reports, audit, errors
        updated, report = refine(**mesh, marked=marked)
        reports.append(report)
        if updated is None:
            return mesh, reports, audit, errors
        mesh = updated
        resample_road(field, coefficients, datum, mesh)
    raise AssertionError("bounded refinement loop must return")


def initial_mesh(product, captured):
    mapping = captured["road_vertex_map"]
    count = int(captured["road_face_count"])
    faces = captured["ground_faces"]
    return {
        "xy": captured["ground_xy"],
        "faces": faces,
        "parts": np.r_[product["face_owners"], np.full(len(faces) - count, -1)],
        "frames": captured["ground_frames"],
        "targets": captured["ground_targets"],
        "roots": np.arange(len(faces)),
        "port_roots": {
            tuple(sorted(edge)): i
            for i, edge in enumerate(mapping[product["port_edges"]])
        },
    }


def solve_adaptive(field, product, captured, *, method="cone", seconds=8, rounds=3):
    mesh = initial_mesh(product, captured)
    certificates = continuous_matrix(
        field,
        product["vertices"],
        product["indices"],
        product["face_owners"],
        captured["road_frames"],
    )
    records, best = [], None
    for iteration in range(rounds):
        h, g, equal, _, system = assemble_world(
            field,
            product,
            mesh["xy"],
            mesh["faces"],
            mesh["parts"],
            mesh["frames"],
            certificates,
        )
        coefficients, report = fit_planes(
            h, g, equal, mesh["targets"], method=method, seconds=seconds
        )
        record = {"iteration": iteration, "system": system, "solver": report}
        records.append(record)
        print(json.dumps(dict(stage="coupled-fit", **record)), flush=True)
        if coefficients is None:
            break
        mesh["heights"] = h @ coefficients + report["datum_m"]
        planes = coefficients[: 3 * len(field.origins)]
        _, audit, errors = quality(field, planes, mesh, product["part_roles"])
        if not audit["whole_world_valid"]:
            raise ValueError("independent complete road/soil grade audit failed")
        best = mesh.copy(), coefficients, report, audit, errors
        refined, stages, after, refined_errors = refine_geometry(
            field, planes, report["datum_m"], mesh.copy(), product["part_roles"]
        )
        record.update(refinement=stages, final_audit=after)
        if after["whole_world_valid"] and after["geometry_complete"]:
            best = refined, coefficients, report, after, refined_errors
            break
        if len(refined["faces"]) == len(mesh["faces"]):
            break
        mesh = refined
    return best, records


def store_result(
    output, field, product, captured, best, records, layout, metadata, sources, began
):
    report = {
        "valid": False,
        "reason": "no_valid_joint_result",
        "rounds": records,
        "layout": layout,
        "generation_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    "lib",
                    "test/experiments/transport",
                ],
                cwd=ROOT,
                text=True,
            ).strip()
        ),
        "inputs": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in REFERENCE.iterdir()
            if p.is_file()
        },
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("port_plane_*.py")
        },
        "frame": metadata["frame"],
        "source_usage": sources["original_source_usage"],
    }
    output.mkdir(parents=True, exist_ok=True)
    if best is not None:
        mesh, coefficients, solver, audit, errors = best
        planes = coefficients[: 3 * len(field.origins)]
        report.update(
            valid=audit["geometry_complete"],
            reason="" if audit["geometry_complete"] else "refinement_budget_exhausted",
            audit=audit,
            vertex_count=len(mesh["xy"]),
            face_count=len(mesh["faces"]),
            maximum_original_target_error_m=float(
                abs(
                    mesh["heights"][: len(captured["ground_targets"])]
                    - captured["ground_targets"]
                ).max()
            ),
            maximum_all_prior_error_m=float(
                abs(mesh["heights"] - mesh["targets"]).max()
            ),
        )
        report["analytic_contacts"] = contact_audit(
            field, planes, product, mesh["heights"][captured["road_vertex_map"]]
        )
        path = output / ("plan.npz" if report["valid"] else "partial-plan.npz")
        port_edges, port_ids = zip(*mesh["port_roots"].items())
        np.savez_compressed(
            path,
            vertices=np.c_[mesh["xy"], mesh["heights"]],
            faces=mesh["faces"],
            face_parts=mesh["parts"],
            frames=mesh["frames"],
            targets=mesh["targets"],
            source_root_faces=mesh["roots"],
            current_port_edges=port_edges,
            current_port_ids=port_ids,
            plane_origins=field.origins,
            plane_ids=field.plane_ids,
            cut_centres=field.cut_centres,
            inward_normals=field.inward_normals,
            scales=field.scales,
            plane_coefficients=planes,
            all_coefficients=coefficients,
            datum_m=solver["datum_m"],
            height_chord_bounds_m=errors,
            road_vertex_map=captured["road_vertex_map"],
        )
        report.update(
            mesh_file=path.name,
            mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        )
    report.update(
        total_ms=(time.perf_counter() - began) * 1000,
        peak_rss_bytes=int(
            next(
                line
                for line in Path("/proc/self/status").read_text().splitlines()
                if line.startswith("VmHWM:")
            ).split()[1]
        )
        * 1024,
        scope="C1 analytic road patch plan with conforming C0 finite road/soil mesh. All original source XY/module/port identities remain. Valid only when every full current road face has <=0.01m height error, normalized grade<=1+1e-7, and every original port has <=0.005 finite gradient jump. No new native/full transportation/LOD or driving-dynamics proof.",
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in ("valid", "reason", "total_ms", "peak_rss_bytes", "audit")
                if k in report
            }
        ),
        flush=True,
    )
    return report["valid"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--method", choices=("cone", "lp"), default="cone")
    parser.add_argument("--seconds", type=float, default=8)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if (
        not 1 <= args.rounds <= 3
        or not np.isfinite(args.seconds)
        or not 0 < args.seconds <= 8
    ):
        raise ValueError(
            "at most three solves of at most eight requested seconds required"
        )
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    began = time.perf_counter()
    product, captured, metadata, sources = load_reference()
    field, layout = prepare_plane_field(product)
    best, records = solve_adaptive(
        field,
        product,
        captured,
        method=args.method,
        seconds=args.seconds,
        rounds=args.rounds,
    )
    if not store_result(
        args.output,
        field,
        product,
        captured,
        best,
        records,
        layout,
        metadata,
        sources,
        began,
    ):
        raise SystemExit(2)


if __name__ == "__main__":
    main()

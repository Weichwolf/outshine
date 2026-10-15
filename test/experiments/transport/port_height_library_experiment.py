"""Replay the immutable captured tile through the installed public port planner."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport import PortHeightBudget, PortHeightInput, plan_port_heights
from port_plane_fixture import REFERENCE, load_reference

ROOT = Path(__file__).resolve().parents[3]


def source_input(product, captured):
    faces = captured["ground_faces"]
    parts = np.r_[
        product["face_owners"], np.full(len(faces) - len(product["indices"]), -1)
    ]
    return PortHeightInput(
        Mesh(np.c_[captured["ground_xy"], captured["ground_targets"]], faces),
        parts,
        product["part_roles"],
        captured["road_vertex_map"][product["port_edges"]],
        product["port_parts"],
        captured["ground_frames"],
    )


def save_plan(path, plan):
    road = plan.face_parts >= 0
    referenced = np.unique(plan.mesh.triangles[road])
    soil = np.setdiff1d(np.arange(len(plan.mesh.vertices)), referenced)
    coefficients = np.r_[
        plan.plane_coefficients, plan.mesh.vertices[soil, 2] - plan.datum_m
    ]
    field = plan.field
    np.savez_compressed(
        path,
        xy=plan.mesh.vertices[:, :2],
        faces=plan.mesh.triangles,
        heights=plan.mesh.vertices[:, 2],
        parts=plan.face_parts,
        frames=plan.grade_frames,
        targets=plan.targets_m,
        roots=plan.source_faces,
        original_vertex_map=plan.original_vertex_map,
        intervals=plan.parameter_intervals,
        marked=np.zeros(len(plan.mesh.triangles), bool),
        height_errors=plan.height_error_bounds_m,
        coefficients=coefficients,
        datum_m=plan.datum_m,
        port_edges=plan.port_edges,
        port_ids=plan.port_ids,
        plane_origins=field.origins,
        plane_ids=field.plane_ids,
        cut_centres=field.cut_centres,
        inward_normals=field.inward_normals,
        scales=field.scales,
    )


def run(args):
    began = time.monotonic()
    product, captured, metadata, sources = load_reference()
    source = source_input(product, captured)
    fit = plan_port_heights(
        source,
        budget=PortHeightBudget(
            solver_time_limit_s=args.seconds,
            max_rounds=args.rounds,
            initial_segments=args.segments,
        ),
        method=args.method,
        progress=lambda row: print(json.dumps(row), flush=True),
    )
    report = dict(fit.report)
    valid = fit.plan is not None
    args.output.mkdir(parents=True, exist_ok=True)
    if valid:
        path = args.output / "plan.npz"
        save_plan(path, fit.plan)
        report.update(
            mesh_file=path.name,
            mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        )
    kernels = list((ROOT / "lib/src/outshine_ref/transport/port_heights").glob("*.py"))
    kernels += [
        ROOT / "lib/src/outshine_ref/contracts.py",
        ROOT / "lib/src/outshine_ref/transport/heights.py",
    ]
    report.update(
        valid_geometry=valid,
        approved=False,
        source_fit_valid=valid,
        generation_commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        working_tree_dirty=bool(
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
        implementation_sha256={
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("port_*py")
        },
        library_sha256={
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in kernels
        },
        input_sha256={
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in REFERENCE.iterdir()
            if p.is_file()
        },
        frame=metadata["frame"],
        original_source_usage=sources["original_source_usage"],
        source_usage=sources["original_source_usage"]
        if valid
        else [
            {
                **row,
                "status": "not_used",
                "reason": row["reason"]
                if row["status"] == "not_used"
                else report["reason"],
            }
            for row in sources["original_source_usage"]
        ],
        total_with_input_and_output_ms=(time.monotonic() - began) * 1000,
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        planner_module=str(Path(plan_port_heights.__code__.co_filename).resolve()),
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "valid_geometry",
                    "reason",
                    "total_ms",
                    "total_with_input_and_output_ms",
                    "peak_rss_bytes",
                )
            }
        ),
        flush=True,
    )
    return valid

"""Bounded whole-tile C1 plane planning on the retained Vienna XY/DEM products (#169)."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from collections import Counter
from pathlib import Path

import numpy as np
from outshine_ref.transport.port_heights._contacts import contact_audit
from outshine_ref.transport.port_heights._field import control_bounds
from outshine_ref.transport.port_heights._layout import prepare_plane_field
from outshine_ref.transport.port_heights._system import (
    assemble,
    fit_generated,
    fit_planes,
)
from port_plane_fixture import REFERENCE, load_reference
from surface_height_checks import normal_gradients

ROOT = Path(__file__).resolve().parents[3]


def validate_result(field, coefficients, xy, faces, owners, frames, vertex_parts):
    z, _ = field.evaluate(xy, vertex_parts, coefficients)
    gradients = normal_gradients(xy, faces, z)
    discrete = np.linalg.norm(np.einsum("nki,ni->nk", frames, gradients), axis=1)
    continuous, errors = [], []
    for start in range(0, len(faces), 256):
        selection = slice(start, start + 256)
        grade, error = control_bounds(
            field,
            xy[faces[selection]],
            owners[selection],
            coefficients,
            frames[selection],
        )
        continuous.extend(grade)
        errors.extend(error)
    continuous, errors = np.asarray(continuous), np.asarray(errors)
    if discrete.max() > 1.0000001 or continuous.max() > 1.0000001:
        raise ValueError("independent finite/continuous whole-face grade audit failed")
    return (
        z,
        {
            "maximum_finite_normalized_grade": float(discrete.max()),
            "maximum_continuous_normalized_grade": float(continuous.max()),
            "maximum_height_chord_bound_m": float(errors.max()),
            "p95_height_chord_bound_m": float(np.quantile(errors, 0.95)),
            "faces_requiring_1cm_refinement": int(np.count_nonzero(errors > 0.01)),
        },
        errors,
    )


def run(method, seconds, output):
    output.mkdir(parents=True, exist_ok=True)
    began = time.perf_counter()
    product, captured, metadata, sources = load_reference()
    targets, frames = captured["road_targets"], captured["road_frames"]
    field, layout = prepare_plane_field(product)
    xy, faces, owners = product["vertices"], product["indices"], product["face_owners"]
    geometry_ms = (time.perf_counter() - began) * 1000
    print(
        json.dumps(
            {
                "stage": "geometry",
                "plane_nodes": len(field.origins),
                "ports": layout["original_ports"],
                "ms": geometry_ms,
            }
        ),
        flush=True,
    )
    height, gradient, equality, vertex_parts, system = assemble(
        field, xy, faces, owners, frames
    )
    print(json.dumps(dict(stage="assembled", **system)), flush=True)
    if method == "generated-cone":
        coefficients, solver = fit_generated(
            height, gradient, equality, targets, system, seconds=seconds
        )
    else:
        coefficients, solver = fit_planes(
            height, gradient, equality, targets, method=method, seconds=seconds
        )
    print(json.dumps(dict(stage="solver", **solver)), flush=True)
    report = {
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
        "layout": layout,
        "system": system,
        "solver": solver,
        "geometry_prepare_ms": geometry_ms,
        "inputs": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                REFERENCE / "metadata.json",
                REFERENCE / "Vienna.npz",
                REFERENCE / "sources.json.gz",
            )
        },
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("port_plane_*.py")
        },
        "source_usage": sources["original_source_usage"],
        "frame": metadata["frame"],
        "scope": "Experimental whole road height field on original module/cut XY, not yet a published combined soil/traffic replacement. Analytic patch contacts C1 by shared parent planes. Finite original triangles C0; full error certificates explicitly identify refinement still needed. No new drivable-turn/grade-separated/native/LOD proof.",
    }
    if coefficients is not None:
        heights, audit, errors = validate_result(
            field, coefficients, xy, faces, owners, frames, vertex_parts
        )
        report["audit"] = audit
        report["contacts"] = contact_audit(field, coefficients, product, heights)
        np.savez_compressed(
            output / "plan.npz",
            plane_origins=field.origins,
            plane_ids=field.plane_ids,
            cut_centres=field.cut_centres,
            inward_normals=field.inward_normals,
            scales=field.scales,
            coefficients=coefficients,
            datum_m=solver["datum_m"],
            vertices=np.c_[xy, heights + solver["datum_m"]],
            faces=faces,
            face_parts=owners,
            frames=frames,
            height_chord_bounds_m=errors,
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
        local_plane_recipe_reasons=dict(
            Counter(r["reason"] for r in layout["explicit_local_plane_recipes"])
        ),
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: report[key]
                for key in ("total_ms", "peak_rss_bytes", "local_plane_recipe_reasons")
            }
        ),
        flush=True,
    )
    if coefficients is None:
        raise SystemExit(2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--method", choices=("cone", "generated-cone", "lp"), required=True
    )
    parser.add_argument("--seconds", type=float, default=8)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not np.isfinite(args.seconds) or not 0 < args.seconds <= 60:
        raise ValueError("solver budget must be in (0,60] seconds")
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    run(args.method, args.seconds, args.output)


if __name__ == "__main__":
    main()

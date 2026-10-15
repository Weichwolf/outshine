"""Joint road-plane and soil-height fitting on the existing conforming whole tile."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path

import numpy as np
from port_plane_contacts import contact_audit
from port_plane_field import control_bounds
from port_plane_fixture import REFERENCE, load_reference
from port_plane_layout import prepare_plane_field
from port_plane_system import (
    continuous_matrix,
    discrete_matrix,
    fit_planes,
    vertex_matrices,
)
from scipy import sparse
from surface_height_checks import normal_gradients


def assemble_world(field, product, xy, faces, parts, frames, certificates=None):
    began = time.perf_counter()
    height, equality, _, vertex_parts = vertex_matrices(field, xy, faces, parts)
    road = parts >= 0
    if certificates is None:
        continuous, report = continuous_matrix(
            field,
            xy[faces[road]].reshape(-1, 2),
            np.arange(3 * road.sum()).reshape(-1, 3),
            parts[road],
            frames[road],
        )
    else:
        continuous, report = certificates
        report = dict(report)
    extra = height.shape[1] - continuous.shape[1]
    continuous = sparse.hstack(
        [continuous, sparse.csc_matrix((continuous.shape[0], extra))]
    ).tocsc()
    finite = discrete_matrix(xy, faces, frames, height)
    gradient = sparse.vstack([continuous, finite]).tocsc()
    report.update(
        plane_coefficients=3 * len(field.origins),
        soil_height_unknowns=extra,
        unknowns=height.shape[1],
        total_cones=gradient.shape[0] // 2,
        point_contact_equalities=equality.shape[0],
        world_faces=len(faces),
        assembly_ms=(time.perf_counter() - began) * 1000,
        sparse_bytes=sum(
            a.data.nbytes + a.indices.nbytes + a.indptr.nbytes
            for a in (height, gradient, equality)
        ),
    )
    return height, gradient, equality, vertex_parts, report


def approximation_bounds(field, coefficients, xy, faces, parts, frames):
    errors = np.zeros(len(faces))
    maximum = 0.0
    road = np.flatnonzero(parts >= 0)
    for begin in range(0, len(road), 256):
        ids = road[begin : begin + 256]
        grade, error = control_bounds(
            field, xy[faces[ids]], parts[ids], coefficients, frames[ids]
        )
        maximum = max(maximum, float(grade.max(initial=0)))
        errors[ids] = error
    return errors, maximum


def finite_ports(xy, faces, parts, heights, port_roots, roles):
    selected = {edge: [] for edge in port_roots}
    for face, corners in enumerate(faces):
        for a, b in ((0, 1), (1, 2), (2, 0)):
            key = tuple(sorted((int(corners[a]), int(corners[b]))))
            if key in selected:
                selected[key].append(face)
    gradients = normal_gradients(xy, faces, heights)
    jumps, bad_faces = [], []
    by_port = np.zeros(max(port_roots.values(), default=-1) + 1)
    for edge, touching in selected.items():
        if len(touching) != 2 or any(parts[i] < 0 for i in touching):
            raise ValueError(
                "a whole port segment lost one of its two original road modules"
            )
        jump = float(np.linalg.norm(gradients[touching[0]] - gradients[touching[1]]))
        by_port[port_roots[edge]] = max(by_port[port_roots[edge]], jump)
        jumps.append(jump)
        if jump > 0.005:
            bad_faces.extend(i for i in touching if roles[parts[i]] == 0)
    return np.unique(np.asarray(bad_faces, dtype=int)), {
        "original_ports": len(by_port),
        "current_shared_segments": len(jumps),
        "maximum_finite_contact_grade_jump": float(by_port.max(initial=0)),
        "p95_finite_contact_grade_jump": float(np.quantile(by_port, 0.95))
        if len(by_port)
        else 0.0,
        "original_ports_exceeding_0_005": int(np.count_nonzero(by_port > 0.005)),
    }


def run(args):
    began = time.perf_counter()
    product, captured, metadata, sources = load_reference()
    field, layout = prepare_plane_field(product)
    xy, faces, frames, targets = (
        captured[k]
        for k in ("ground_xy", "ground_faces", "ground_frames", "ground_targets")
    )
    count = int(captured["road_face_count"])
    parts = np.r_[product["face_owners"], np.full(len(faces) - count, -1)]
    height, gradient, equal, _vertex_parts, system = assemble_world(
        field, product, xy, faces, parts, frames
    )
    print(json.dumps(dict(stage="world-assembled", **system)), flush=True)
    coefficients, report = fit_planes(
        height, gradient, equal, targets, method=args.method, seconds=args.seconds
    )
    print(json.dumps(dict(stage="world-solver", **report)), flush=True)
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    summary = {
        "layout": layout,
        "system": system,
        "solver": report,
        "frame": metadata["frame"],
        "inputs": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in REFERENCE.iterdir()
            if p.is_file()
        },
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("port_plane_*.py")
        },
        "source_usage": sources["original_source_usage"],
    }
    if coefficients is not None:
        relative = height @ coefficients
        heights = relative + report["datum_m"]
        plane_coefficients = coefficients[: 3 * len(field.origins)]
        mapping = captured["road_vertex_map"]
        summary["analytic_contacts"] = contact_audit(
            field, plane_coefficients, product, heights[mapping]
        )
        port_roots = {
            tuple(sorted(edge)): i
            for i, edge in enumerate(mapping[product["port_edges"]])
        }
        _, summary["finite_contacts"] = finite_ports(
            xy, faces, parts, heights, port_roots, product["part_roles"]
        )
        errors, maximum = approximation_bounds(
            field, plane_coefficients, xy, faces, parts, frames
        )
        finite = np.linalg.norm(
            np.einsum("nki,ni->nk", frames, normal_gradients(xy, faces, heights)),
            axis=1,
        )
        if finite.max() > 1.0000001 or maximum > 1.0000001:
            raise ValueError("independent road/soil field/mesh grade audit failed")
        summary["audit"] = {
            "maximum_normalized_finite_grade": float(finite.max()),
            "maximum_normalized_continuous_road_grade": maximum,
            "maximum_height_chord_bound_m": float(errors.max()),
            "faces_requiring_1cm_refinement": int(np.count_nonzero(errors > 0.01)),
            "maximum_soil_grade": float(finite[count:].max() * 2 / 3),
            "finite_whole_world_valid": True,
            "approximation_complete": bool(
                errors.max() <= 0.01
                and summary["finite_contacts"]["original_ports_exceeding_0_005"] == 0
            ),
        }
        np.savez_compressed(
            output / "plan.npz",
            plane_origins=field.origins,
            plane_ids=field.plane_ids,
            cut_centres=field.cut_centres,
            inward_normals=field.inward_normals,
            scales=field.scales,
            plane_coefficients=plane_coefficients,
            all_coefficients=coefficients,
            datum_m=report["datum_m"],
            vertices=np.c_[xy, heights],
            faces=faces,
            face_parts=parts,
            frames=frames,
            targets=targets,
            height_chord_bounds_m=errors,
            road_vertex_map=mapping,
        )
    summary.update(
        total_ms=(time.perf_counter() - began) * 1000,
        peak_rss_bytes=int(
            next(
                line
                for line in Path("/proc/self/status").read_text().splitlines()
                if line.startswith("VmHWM:")
            ).split()[1]
        )
        * 1024,
        scope="Experimental joint whole-tile road C1 fields and C0 soil. Source XY/ports/priors preserved. Finite mesh approximation and port-grade errors remain explicitly unresolved until refinement completes. Not a native/full transport/LOD/runtime proof.",
    )
    (output / "report.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: summary[k]
                for k in ("total_ms", "peak_rss_bytes", "audit")
                if k in summary
            }
        ),
        flush=True,
    )
    if coefficients is None:
        raise SystemExit(2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--method", choices=("cone", "lp"), default="cone")
    parser.add_argument("--seconds", type=float, default=8)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    run(args)


if __name__ == "__main__":
    main()

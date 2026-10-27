"""Actual finite-ground field, original DEM priors and bounded earthwork recipe."""

import argparse
import hashlib
import json
import time
from contextlib import nullcontext
from pathlib import Path

import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports._ground_scope_field import (
    derive_scoped_ground_field,
)
from replay_canonical_bridge_ports import allocations


def positive_volume(area, values):
    """Exact integral of positive linear height over one projected triangle."""
    a, b, c = sorted(map(float, values))
    if c <= 0:
        return 0.0
    if a >= 0:
        return area * (a + b + c) / 3
    if b <= 0:
        return area * c**3 / (3 * (c - a) * (c - b))
    negative = area * a**3 / (3 * (b - a) * (c - a))
    return area * (a + b + c) / 3 - negative


def diagnostics(prepared, report):
    mesh, prior = report["surface"], report["source_prior_surface"]
    if not np.array_equal(mesh.triangles, prior.triangles) or not np.array_equal(
        mesh.vertices[:, :2], prior.vertices[:, :2]
    ):
        raise ValueError("This affine diagnostic requires the original finite topology")
    faces = mesh.triangles
    tri = mesh.vertices[faces]
    sides = tri[:, 1:, :2] - tri[:, :1, :2]
    areas = abs(np.linalg.det(sides)) * 0.5
    gradients = np.linalg.solve(sides, tri[:, 1:, 2:] - tri[:, :1, 2:]).squeeze(-1)
    frames = report["source_grade_frames"][report["surface_source_faces"]]
    normalized = np.linalg.norm(np.einsum("fij,fj->fi", frames, gradients), axis=1)
    delta = mesh.vertices[:, 2] - prior.vertices[:, 2]
    fill = sum(
        positive_volume(area, d) for area, d in zip(areas, delta[faces], strict=True)
    )
    cut = sum(
        positive_volume(area, -d) for area, d in zip(areas, delta[faces], strict=True)
    )
    signed = float(np.sum(areas * np.mean(delta[faces], axis=1)))
    if not np.isclose(fill - cut, signed, rtol=1e-12, atol=1e-9):
        raise ValueError("independent signed-volume identity failed")
    return {
        "vertices": len(mesh.vertices),
        "faces": len(faces),
        "projected_area_m2": float(sum(areas)),
        "maximum_fill_m": float(max(0.0, max(delta))),
        "maximum_cut_m": float(max(0.0, max(-delta))),
        "fill_volume_m3": fill,
        "cut_volume_m3": cut,
        "signed_volume_identity_error_m3": abs(fill - cut - signed),
        "maximum_normalized_finite_gradient": float(max(normalized)),
        "maximum_original_prior_error_m": float(max(abs(delta))),
        "scope": "Whole finite affine mesh against piecewise-linear actual original DEM observations; no dense-DEM/terrain-contact/World claim",
    }, delta


def plot(output, prepared, report, delta):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    mesh, prior = report["surface"], report["source_prior_surface"]
    fig = plt.figure(figsize=(12, 5), layout="constrained")
    ax = fig.add_subplot(121, projection="3d")
    ax.plot_trisurf(
        *prior.vertices.T, triangles=prior.triangles, color="#777777", alpha=0.35
    )
    ax.plot_trisurf(
        *mesh.vertices.T, triangles=mesh.triangles, color="#176ba0", alpha=0.85
    )
    ax.set(
        xlabel="Source east (m)",
        ylabel="Source north (m)",
        zlabel="Source ENU z (m)",
        title="Actual DEM priors and qualified affine field",
    )
    ax = fig.add_subplot(122)
    artist = PolyCollection(
        mesh.vertices[mesh.triangles, :2],
        array=delta[mesh.triangles].mean(axis=1),
        cmap="coolwarm",
        edgecolors="#555555",
        linewidths=0.3,
    )
    maximum = max(abs(delta))
    artist.set_clim(-maximum, maximum)
    ax.add_collection(artist)
    ax.autoscale()
    ax.set_aspect("equal")
    ax.set(
        xlabel="Source east (m)",
        ylabel="Source north (m)",
        title="Fill (+) / cut (−), source triangle means",
    )
    fig.colorbar(artist, ax=ax, label="Field − original DEM-prior surface (m)")
    fig.suptitle("Two-span field only:3 m=width/4 recipe; original Unknown unchanged")
    fig.savefig(output / "ground-field-scope.png", dpi=150)
    plt.close(fig)


def run(args):
    args.output.mkdir(parents=True, exist_ok=True)
    source, groups, ways, binding, reader_work = load_bridge_source_facts(args.input)
    ref = json.loads((args.input / "manifest.json").read_bytes())["terrain_reference"]
    if (
        hashlib.sha256((FIXTURE / "dem.webp").read_bytes()).hexdigest()
        != ref["source_sha256"]
    ):
        raise ValueError("original DEM pin mismatch")
    terrain, terrain_binding = terrain_input(FIXTURE, {"terrain": ref})
    seed = (8409897393650800292, 0)
    width = next(p.width_m for p in source.spans if p.identifier == seed)
    tracking = nullcontext()
    if args.allocation_trace:
        import memray

        tracking = memray.Tracker(
            str(args.allocation_trace), trace_python_allocators=True
        )
    start = time.perf_counter()
    with tracking:
        prepared, reason, report = derive_scoped_ground_field(
            source,
            seed,
            groups,
            ways,
            terrain,
            source_binding=source.binding,
            candidate_binding=binding,
            terrain_binding=terrain_binding,
            maximum_prior_change_m=width / 4,
        )
    elapsed = time.perf_counter() - start
    if reason:
        raise ValueError(reason)
    measured, delta = diagnostics(prepared, report)
    summary = {
        "recipe": {
            "name": "road-earthwork-width-quarter/1",
            "source_width_m": width,
            "maximum_prior_change_m": width / 4,
        },
        "source_facts_digest": binding["source_facts_digest"],
        "selected_members": report["source_scope"]["members"],
        "typed_exits": report["source_scope"]["typed_exits"],
        "source_anchor": prepared[0],
        "unknown_nodes_unchanged": len(source.unknown_nodes),
        "terrain_samples": report["terrain_samples"],
        "source_work": report["source_work"],
        "reader_work": reader_work,
        "selected_method": report["field"]["selected_method"],
        "field_seconds": elapsed,
        "whole_surface": measured,
        "finite_quality": report.get("finite_quality"),
        "qualified_world_joins": 0,
        "allocation_tracing": bool(args.allocation_trace),
        "affine_vertex_copy_bytes": report["surface"].vertices.nbytes,
        "copy_scope": "Additional affine vertex copy only; total procedure copied bytes are not instrumented",
    }
    if args.allocation_trace:
        summary["allocations"] = allocations(args.allocation_trace)
    (args.output / "ground-field-scope.json").write_text(
        json.dumps(summary, indent=2) + "\n"
    )
    np.savez(
        args.output / "ground-field-scope.npz",
        vertices=report["surface"].vertices,
        faces=report["surface"].triangles,
        source_prior_vertices=report["source_prior_surface"].vertices,
        source_faces=report["surface_source_faces"],
        source_grade_frames=report["source_grade_frames"],
    )
    if args.plot:
        plot(args.output, prepared, report, delta)
    print(json.dumps(summary))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(__file__).parent / "fixtures/bridge-source-facts",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--plot", action="store_true")
    parser.add_argument("--allocation-trace", type=Path)
    run(parser.parse_args())

"""Bounded shared-facet owner versus the same canonical pair's reference fit (#963)."""

import argparse
import dataclasses
import hashlib
import json
import platform
import resource
from pathlib import Path

import numpy as np
from compare_physical_boundary import crease, measured
from compare_physical_river import audit
from outshine_ref.contracts import Mesh
from outshine_ref.water import fit_physical_surface
from outshine_ref.water.frame import normal_caps, tangent_directions
from physical_boundary import boundary_error, canonical_cut, local_piece
from water_contact_band import (
    common_level_shift,
    compose,
    level_moment,
    physical_audit,
    solve_band,
    solve_region,
)
from water_contact_inputs import original_pair

DEFAULT = Path(__file__).parent / "fixtures/feldkirch-ill-native-inputs.json"


def rms(mesh, error):
    triangle = mesh.vertices[mesh.triangles, :2]
    a, b = triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
    area = abs(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]) / 2
    e = error[mesh.triangles]
    return float(
        np.sqrt(
            np.sum(
                area
                / 6
                * (
                    (e * e).sum(1)
                    + e[:, 0] * e[:, 1]
                    + e[:, 0] * e[:, 2]
                    + e[:, 1] * e[:, 2]
                )
            )
            / area.sum()
        )
    )


def compare(path, output):
    (mesh, anchor, rows, bounds, csr, inputs), prepare_cost = measured(
        lambda: original_pair(path)
    )
    count = len(mesh.triangles)
    primary = np.c_[np.arange(count), count + np.arange(count)]
    reference, reference_cost = measured(
        lambda: fit_physical_surface(
            mesh, anchor, rows, up_bounds_m=bounds, primary_rows=primary
        )
    )
    if reference.mesh is None:
        raise ValueError(f"canonical reference absent: {reference.report}")
    (band, band_report), band_cost = measured(
        lambda: solve_band(
            mesh,
            anchor,
            rows,
            bounds,
            plane_x=0,
            half_width=20,
            recipe_revision="original-pair-soft-courses-v1:"
            + inputs["historical_native_sha256"],
        )
    )
    if band is None:
        raise ValueError(f"canonical contact owner absent: {band_report}")
    regions, reports, costs = {}, {}, {}
    # Arrival order does not choose/re-fit the contact owner.
    for side in (1, 0):
        (regional, domain, report), cost = measured(
            lambda side=side: solve_region(
                mesh, anchor, rows, bounds, band, plane_x=0, side=side
            )
        )
        if regional is None:
            raise ValueError(f"regional fit absent: {report}")
        regions[side] = (regional, domain)
        reports[str(side)] = report
        costs[str(side)] = cost
    (joined, joined_audit), compose_cost = measured(
        lambda: compose(
            mesh, anchor, rows, bounds, band, [regions[0], regions[1]], plane_x=0
        )
    )
    moments = {
        2: level_moment(
            band.domain.mesh, band.mesh, np.arange(len(band.domain.face_ids)), bounds
        )
    }
    for side, (regional, domain) in regions.items():
        own_faces = np.flatnonzero(~np.isin(domain.face_ids, band.domain.face_ids))
        moments[side] = level_moment(domain.mesh, regional, own_faces, bounds)
    (shift, level_report), level_cost = measured(
        lambda: common_level_shift(moments, free_level=True)
    )
    if level_report["source_faces_counted_once"] != count:
        raise ValueError("constant-mode summaries duplicated or omitted contact faces")
    unshifted = joined
    final_version = hashlib.sha256(
        band.version.encode() + np.asarray([shift], dtype="<f8").tobytes()
    ).hexdigest()
    band_vertices = band.mesh.vertices.copy()
    band_vertices[:, 2] += shift
    published_band = dataclasses.replace(
        band, mesh=Mesh(band_vertices, band.mesh.triangles), version=final_version
    )
    published_regions = []
    for side in (0, 1):
        regional, domain = regions[side]
        placed = regional.vertices.copy()
        placed[:, 2] += shift
        published_regions.append((Mesh(placed, regional.triangles), domain))
    joined, _published_audit = compose(
        mesh, anchor, rows, bounds, published_band, published_regions, plane_x=0
    )
    np.testing.assert_array_equal(
        joined.vertices[band.domain.vertex_ids], published_band.mesh.vertices
    )
    shifted_audit = physical_audit(joined, anchor, rows, bounds)
    if not shifted_audit["admitted"]:
        raise ValueError("shared constant-mode correction broke physical admission")
    caps = normal_caps(anchor, mesh.vertices[:, :2], mesh.triangles, bounds)
    tangent = tangent_directions(caps["up"][rows[:, 0].astype(int)], rows[:, 1:4])
    sampled, _ = audit(joined, anchor, rows, bounds, tangent)
    if sampled["sampled_violating_rows"]:
        raise ValueError("independent composed physical-grade reader found a violation")
    cut = canonical_cut(joined, 0)
    anchors = [anchor + [-0.001, 0], anchor + [0.001, 0]]
    pieces = [local_piece(cut, side, anchor, a) for side, a in enumerate(anchors)]
    boundary = boundary_error(
        pieces[0][0], anchors[0], pieces[0][2], pieces[1][0], anchors[1], pieces[1][2]
    )
    dihedral = crease(
        [p[0] for p in pieces], anchors, [p[2] for p in pieces], cut.parents
    )
    if (
        boundary["endpoint_max_m"] > 1e-8
        or dihedral["maximum_new_dihedral_degrees"] > 1e-7
    ):
        raise ValueError(
            "shared owner facet cut did not survive regional frame placement"
        )
    replay, domain, replay_report = solve_region(
        mesh, anchor, rows, bounds, band, plane_x=0, side=0, prior_offset_m=0.25
    )
    if replay is None:
        raise ValueError(f"changed soft-prior local job absent: {replay_report}")
    exact_ids = np.searchsorted(domain.vertex_ids, band.domain.vertex_ids)
    np.testing.assert_array_equal(replay.vertices[exact_ids], band.mesh.vertices)
    delta = joined.vertices[:, 2] - reference.mesh.vertices[:, 2]
    report = {
        "scope": "one bounded canonical pair/contact owner, no global corner/junction/lake/coast ownership or source-completeness proof",
        "inputs": inputs,
        "prepare_cost": prepare_cost,
        "reference_fit": dict(reference.report),
        "reference_cost": reference_cost,
        "band": band_report,
        "relative_band_version": band.version,
        "published_band_version": final_version,
        "band_cost": band_cost,
        "regions": reports,
        "region_costs": costs,
        "compose_cost": compose_cost,
        "composed_whole_face_audit": joined_audit,
        "independent_sampled_audit": sampled,
        "shared_constant_mode": dict(
            level_report,
            shift_m=shift,
            cost=level_cost,
            physical_audit=shifted_audit,
            independently_published_shared_bits_exact=True,
        ),
        "unshifted_difference_from_reference": {
            "maximum_vertex_enu_up_m": float(
                abs(unshifted.vertices[:, 2] - reference.mesh.vertices[:, 2]).max()
            ),
            "integrated_enu_up_rms_m": rms(
                mesh, unshifted.vertices[:, 2] - reference.mesh.vertices[:, 2]
            ),
        },
        "boundary": boundary,
        "new_cut_dihedral": dihedral,
        "changed_soft_prior_control": {
            "offset_m": 0.25,
            "same_owner_vertices_exact": True,
            "report": replay_report,
        },
        "difference_from_reference": {
            "maximum_vertex_enu_up_m": float(abs(delta).max()),
            "integrated_enu_up_rms_m": rms(mesh, delta),
        },
        "integrated_soft_target_rms_m": {
            "reference": rms(mesh, reference.mesh.vertices[:, 2] - mesh.vertices[:, 2]),
            "contact_owner": rms(mesh, joined.vertices[:, 2] - mesh.vertices[:, 2]),
        },
        "work": {
            "reference_fit_faces": count,
            "band_fit_faces": len(band.mesh.triangles),
            "regional_fit_faces": [len(regions[s][0].triangles) for s in (0, 1)],
            "candidate_fit_faces_including_contact_overlap": len(band.mesh.triangles)
            + sum(len(regions[s][0].triangles) for s in (0, 1)),
            "contact_geometry_index_payload_bytes": band_report[
                "shared_geometry_index_payload_bytes"
            ],
            "changed_prior_control_fit_faces": len(replay.triangles),
            "copied_bytes": None,
            "allocation_count": None,
        },
        "environment": {
            "python": platform.python_version(),
            "system": platform.system(),
            "machine": platform.machine(),
            "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            * 1024,
        },
        "native_device_admission": False,
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(
        output / "fields.npz",
        target_vertices=mesh.vertices,
        reference_vertices=reference.mesh.vertices,
        composed_vertices=joined.vertices,
        unshifted_vertices=unshifted.vertices,
        triangles=mesh.triangles,
        shared_vertices=band.mesh.vertices,
        shared_vertex_ids=band.domain.vertex_ids,
        shared_face_ids=band.domain.face_ids,
        source_offsets=csr.offsets,
        source_members=csr.members,
        original_native_surface_indices=inputs["original_native_surface_indices"],
        anchor=anchor,
    )
    plot(mesh, reference.mesh, joined, band, output / "comparison.png")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "scope",
                    "difference_from_reference",
                    "integrated_soft_target_rms_m",
                    "work",
                    "boundary",
                    "new_cut_dihedral",
                )
            },
            indent=2,
        )
    )
    return report


def plot(mesh, reference, joined, band, path):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    fig, axes = plt.subplots(1, 2, figsize=(11, 5))
    faces = mesh.triangles
    axes[0].triplot(*mesh.vertices[:, :2].T, faces, color=".7", linewidth=0.5)
    for face in faces[band.domain.face_ids]:
        axes[0].fill(*mesh.vertices[face, :2].T, color="tab:orange", alpha=0.4)
    axes[0].axvline(0, color="red")
    axes[0].set_title("Owned full contact facets (orange)\nFixed bounded source pair")
    color = axes[1].tripcolor(
        *mesh.vertices[:, :2].T,
        faces,
        joined.vertices[:, 2] - reference.vertices[:, 2],
        shading="gouraud",
        cmap="coolwarm",
    )
    fig.colorbar(color, ax=axes[1], label="ENU Up difference from reference (m)")
    axes[1].axvline(0, color="red")
    axes[1].set_title(
        "Independent local fits with shared facets\nSame whole-face physical constraints"
    )
    for axis in axes:
        axis.set_aspect("equal")
        axis.set_xlabel("Canonical east (m)")
        axis.set_ylabel("North (m)")
    fig.suptitle(
        "Conditional contact-owner experiment; no worldwide recipe or speedup claim"
    )
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    compare(args.fixture, args.output)

"""Source-boundary height owner before render refinement, against the same-source oracle."""

import argparse
import hashlib
import json
import resource
import sys
import time
from pathlib import Path

import numpy as np
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_physical_boundary import crease
from compare_physical_boundary import measured as traced_measurement
from compare_physical_river import audit
from outshine_ref.water import fit_physical_surface
from outshine_ref.water.frame import normal_caps, tangent_directions
from physical_boundary import (
    boundary_error,
    canonical_cut,
    child_rows,
    from_world,
    local_piece,
    to_world,
)
from physical_boundary_audit import inherited_envelope
from replay_canonical_water_fit import recipe
from water_contact_band import physical_audit
from water_contact_inputs import original_domain, original_pair
from water_field_difference import compare_fields


def measured(call):
    cpu = time.process_time()
    result, cost = traced_measurement(call)
    return result, dict(cost, process_cpu_seconds=time.process_time() - cpu)


def parent_cut_error(mesh, cut):
    maximum = 0.0
    for faces, parents in zip(cut.faces, cut.parents, strict=True):
        triangles = mesh.vertices[mesh.triangles[parents]]
        gradients = np.linalg.solve(
            triangles[:, 1:, :2] - triangles[:, :1, :2],
            (triangles[:, 1:, 2] - triangles[:, :1, 2])[..., None],
        )[..., 0]
        points = cut.vertices[faces]
        returned = triangles[:, 0, 2, None] + np.einsum(
            "fki,fi->fk", points[:, :, :2] - triangles[:, :1, :2], gradients
        )
        maximum = max(maximum, float(abs(returned - points[:, :, 2]).max()))
    if maximum > 1e-9:
        raise ValueError("draw cut lost the authoritative owner plane")
    left = np.unique(cut.faces[0])
    right = np.unique(cut.faces[1])
    if not np.isin(cut.seam, left).all() or not np.isin(cut.seam, right).all():
        raise ValueError("shared cut edge missing from a region")
    a = cut.vertices[left][np.searchsorted(left, cut.seam)]
    b = cut.vertices[right][np.searchsorted(right, cut.seam)]
    gap = float(np.linalg.norm(a - b, axis=2).max())
    return {
        "maximum_point_up_error_m": maximum,
        "shared_cut_edges": len(cut.seam),
        "owner_frame_endpoint_gap_m": gap,
        "height_queries_use_owner_facets": True,
        "draw_cuts_keep_owner_frame_and_parent_indices": True,
    }


def solve(mesh, anchor, rows, bounds, *, contact_points=None):
    count = len(mesh.triangles)
    result = fit_physical_surface(
        mesh,
        anchor,
        rows,
        up_bounds_m=bounds,
        primary_rows=np.c_[np.arange(count), count + np.arange(count)],
        contact_points=contact_points,
    )
    if result.mesh is None or not result.report.get("finite_qp_optimal"):
        raise ValueError(f"height owner absent: {result.report}")
    return result


def lake_case(fixture):
    from dataclasses import replace

    from canonical_water_domains import resolve

    data, snapshots, indices = observed_snapshots(fixture)
    complete = {t: replace(s, complete=True) for t, s in snapshots.items()}
    options = {
        "epoch": (data["sources"][0]["provider"], data["sources"][0]["revision"]),
        "support": 1 / 64,
        "model_revision": "lake-soft-native-median-grade-0.005-v1",
        "assume_ground": True,
    }
    # Generic first eligible closed multi-source lake; source originals are unchanged.
    for key in sorted(indices):
        native = indices[key]
        info = data["inputs"][int(data["surfaces"][native]["input"])]
        if info["kind"] != "lake":
            continue
        domain = resolve(key, complete, **options)
        if domain.state == "closed" and len(domain.sources) > 1:
            break
    else:
        raise ValueError("no conditional closed multi-source lake")

    def prepare(spacing):
        (
            mesh,
            anchor,
            rows,
            bounds,
            _primary,
            _polys,
            csr,
            topology,
            native_ids,
            target,
        ) = recipe(domain, data, indices, boundary_spacing_m=spacing)
        return (
            mesh,
            anchor,
            rows,
            bounds,
            csr,
            {
                "scope": "closed only under normalized admitted-fixture contract; no raw completeness proof",
                "canonical_sources": domain.sources,
                "source_revision": domain.version,
                "native_surfaces_decoded": len(data["surfaces"]),
                "native_courses_decoded": len(data["courses"]),
                "native_points_decoded": len(data["points_lat_lon"]),
                "native_input_bytes": data["source_bytes"],
                "native_surface_indices": native_ids,
                "model_height_target_m": target,
                "topology": topology,
            },
        )

    return prepare


def run_case(
    name,
    prepare,
    output,
    *,
    selection_cost=None,
    owner_spacing_m=None,
    common_bounds=None,
    publish_contacts=None,
):
    # The candidate owner is complete before even constructing the fine oracle.
    (coarse, anchor, rows, bounds, csr, inputs), prepare_cost = measured(
        lambda: prepare(owner_spacing_m)
    )
    if common_bounds is not None:
        bounds = common_bounds
    solved, owner_cost = measured(lambda: solve(coarse, anchor, rows, bounds))
    descriptor = {
        "recipe": "source-height-owner-v1",
        "source_revision": inputs.get(
            "source_revision", inputs.get("historical_native_sha256")
        ),
        "domain_bounds_enu_m": inputs.get("domain_bounds_enu_m"),
        "boundary_spacing_m": owner_spacing_m,
        "adaptive_tolerance_m": (inputs.get("source_boundary_refinement") or {}).get(
            "tolerance_m"
        ),
        "adaptive_split_round_limit": (
            inputs.get("source_boundary_refinement") or {}
        ).get("split_round_limit"),
    }
    digest = hashlib.sha256(json.dumps(descriptor, sort_keys=True).encode())
    for array in (
        coarse.vertices,
        coarse.triangles,
        anchor,
        rows,
        np.asarray(bounds),
        solved.mesh.vertices,
    ):
        digest.update(np.ascontiguousarray(array).tobytes())
    version = digest.hexdigest()
    owner_bytes = solved.mesh.vertices.tobytes()
    contact_product, contact_cost = (
        measured(lambda: publish_contacts(solved.mesh, anchor, version))
        if publish_contacts is not None
        else (None, None)
    )
    (
        (fine, fine_anchor, fine_rows, _fine_bounds, _fine_csr, fine_inputs),
        fine_prepare_cost,
    ) = measured(lambda: prepare(20.0))
    np.testing.assert_array_equal(anchor, fine_anchor)
    reference, reference_cost = measured(lambda: solve(fine, anchor, fine_rows, bounds))
    if np.any(csr.status) or np.any(_fine_csr.status):
        raise ValueError("unresolved original face/source membership")
    if owner_bytes != solved.mesh.vertices.tobytes():
        raise ValueError("oracle construction mutated the preexisting owner")
    comparison, difference_cost = measured(
        lambda: compare_fields(solved.mesh, reference.mesh)
    )
    difference = comparison.report
    caps = normal_caps(anchor, coarse.vertices[:, :2], coarse.triangles, bounds)
    tangent = tangent_directions(caps["up"][rows[:, 0].astype(int)], rows[:, 1:4])
    sampled, _ = audit(solved.mesh, anchor, rows, bounds, tangent)
    if sampled["sampled_violating_rows"]:
        raise ValueError("independent physical grade reader found violations")
    plane = float(np.median(solved.mesh.vertices[:, 0]))
    cut = canonical_cut(solved.mesh, plane)
    parent_preservation = parent_cut_error(solved.mesh, cut)
    anchors = [anchor + [-0.001, 0], anchor + [0.001, 0]]
    pieces = [local_piece(cut, side, anchor, a) for side, a in enumerate(anchors)]
    boundary = boundary_error(
        pieces[0][0], anchors[0], pieces[0][2], pieces[1][0], anchors[1], pieces[1][2]
    )
    dihedral = crease(
        [p[0] for p in pieces], anchors, [p[2] for p in pieces], cut.parents
    )
    envelopes = []
    for side, (piece, _ids, _seam) in enumerate(pieces):
        local_rows, _primary, gathered = child_rows(
            rows, cut.parents[side], anchor, anchor, len(coarse.triangles)
        )
        returned = from_world(anchor, to_world(anchors[side], piece.vertices))
        envelopes.append(
            inherited_envelope(
                returned,
                piece.triangles,
                cut.parents[side],
                caps,
                local_rows,
                tangent[gathered],
            )
        )
    report = {
        "case": name,
        "scope": "fixed admitted source domain, source-boundary mesh first; global ownership/datum and raw completeness remain open",
        "inputs": inputs,
        "oracle_inputs": fine_inputs,
        "owner_version": version,
        "owner_descriptor": descriptor,
        "owner_before_fine_preparation": True,
        "pre_fine_contacts": contact_product,
        "pre_fine_contacts_cost": contact_cost,
        "model_contract": "one owned functional field for height queries, bed and render LODs; native adoption unproved",
        "boundary_spacing_m": owner_spacing_m,
        "oracle_boundary_spacing_m": 20.0,
        "coarse_prepare_cost": prepare_cost,
        "source_snapshot_and_domain_selection_cost": selection_cost,
        "owner_fit_cost": owner_cost,
        "reference_prepare_cost": fine_prepare_cost,
        "reference_fit_cost": reference_cost,
        "owner_fit": dict(solved.report),
        "reference_fit": dict(reference.report),
        "whole_domain_difference": difference,
        "difference_cost": difference_cost,
        "independent_sampled_audit": sampled,
        "whole_face_audit": physical_audit(solved.mesh, anchor, rows, bounds),
        "independent_frame_reprojection_control": {
            "boundary": boundary,
            "dihedral": dihedral,
            "inherited_envelopes": envelopes,
            "admitted": all(e["certified"] for e in envelopes),
        },
        "authoritative_parent_cut": parent_preservation,
        "coarse_vertices": len(coarse.vertices),
        "coarse_faces": len(coarse.triangles),
        "reference_vertices": len(fine.vertices),
        "reference_faces": len(fine.triangles),
        "coarse_face_source_membership": csr.work,
        "common_bounds_enu_up_m": np.asarray(bounds).tolist(),
        "owner_geometry_index_bytes": solved.mesh.vertices.nbytes
        + solved.mesh.triangles.nbytes,
        "reference_geometry_index_bytes": reference.mesh.vertices.nbytes
        + reference.mesh.triangles.nbytes,
        "copied_bytes": None,
        "allocation_count": None,
        "native_device_admission": False,
        "process_peak_rss_bytes_including_diagnostics": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / f"{name}.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez_compressed(
        output / f"{name}.npz",
        owner_vertices=solved.mesh.vertices,
        owner_triangles=solved.mesh.triangles,
        reference_vertices=reference.mesh.vertices,
        reference_triangles=reference.mesh.triangles,
        anchor=anchor,
        source_offsets=csr.offsets,
        source_members=csr.members,
        cut_vertices=cut.vertices,
        cut_seam=cut.seam,
        left_draw_faces=cut.faces[0],
        right_draw_faces=cut.faces[1],
        left_parent_faces=cut.parents[0],
        right_parent_faces=cut.parents[1],
        overlay_xy=comparison.overlay_xy,
        overlay_error_m=comparison.overlay_error_m,
    )
    title = (
        "Source-height adaptive model"
        if inputs.get("source_boundary_refinement")
        else "Source-boundary owner"
        if owner_spacing_m is None
        else f"{owner_spacing_m:g} m model-height mesh"
    )
    plot(solved.mesh, reference.mesh, comparison, output / f"{name}.png", title)
    return report


def plot(owner, reference, comparison, path, owner_title):
    difference = comparison.report
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    fig, axes = plt.subplots(1, 3, figsize=(14, 5))
    for axis, mesh, title in (
        (axes[0], owner, owner_title),
        (axes[1], reference, "20 m boundary-refined reference"),
    ):
        axis.triplot(
            *mesh.vertices[:, :2].T, mesh.triangles, color="tab:blue", linewidth=0.6
        )
        axis.set_title(
            f"{title}\n{len(mesh.vertices)} vertices / {len(mesh.triangles)} faces"
        )
        axis.set_aspect("equal")
        axis.set_xlabel("Canonical east (m)")
        axis.set_ylabel("North (m)")
    xy = comparison.overlay_xy.reshape(-1, 2)
    faces = np.arange(len(xy)).reshape(-1, 3)
    color = axes[2].tripcolor(
        *xy.T,
        faces,
        comparison.overlay_error_m.ravel(),
        shading="gouraud",
        cmap="coolwarm",
    )
    axes[2].set_title("Signed ENU Up difference\non complete triangle overlap")
    axes[2].set_aspect("equal")
    axes[2].set_xlabel("Canonical east (m)")
    axes[2].set_ylabel("North (m)")
    axes[2].scatter(*difference["worst"]["xy_m"], marker="x", color="black", s=35)
    bar = fig.colorbar(color, ax=axes[2], label="ENU Up difference (m)")
    bar.formatter = matplotlib.ticker.FuncFormatter(
        lambda value, _position: f"{value:.2g}"
    )
    bar.update_ticks()
    fig.suptitle(
        f"Whole-overlap ENU Up error: max {difference['maximum_enu_up_difference_m']:.6g} m; RMS {difference['integrated_enu_up_rms_m']:.6g} m"
    )
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    river = run_case(
        "river-pair",
        lambda spacing: original_pair(args.fixture, boundary_spacing_m=spacing),
        args.output,
    )
    full_river = run_case(
        "full-original-river",
        lambda spacing: original_domain(args.fixture, None, boundary_spacing_m=spacing),
        args.output,
    )
    model40 = run_case(
        "full-river-model40",
        lambda spacing: original_domain(args.fixture, None, boundary_spacing_m=spacing),
        args.output,
        owner_spacing_m=40.0,
        common_bounds=full_river["common_bounds_enu_up_m"],
    )
    try:
        adaptive = run_case(
            "full-river-source-adaptive",
            lambda spacing: original_domain(
                args.fixture,
                None,
                boundary_spacing_m=spacing,
                boundary_tolerance_m=0.01 if spacing is None else None,
            ),
            args.output,
            common_bounds=full_river["common_bounds_enu_up_m"],
        )
    except ValueError as error:
        adaptive = {
            "case": "full-river-source-adaptive",
            "state": "not_admitted",
            "reason": str(error),
            "scope": "fixed bounded midpoint attempt; no partial geometry or relaxed limits",
            "parameters": {
                "indicator_tolerance_m": 0.01,
                "split_rounds": 8,
                "vertex_limit": 4096,
                "query_limit": 16384,
            },
        }
        (args.output / "full-river-source-adaptive.json").write_text(
            json.dumps(adaptive, indent=2) + "\n"
        )
    adaptive_work = run_case(
        "full-river-source-workbudget",
        lambda spacing: original_domain(
            args.fixture,
            None,
            boundary_spacing_m=spacing,
            boundary_tolerance_m=0.01 if spacing is None else None,
            boundary_rounds=None,
        ),
        args.output,
        common_bounds=full_river["common_bounds_enu_up_m"],
    )
    prepare_lake, selection_cost = measured(lambda: lake_case(args.fixture))
    lake = run_case(
        "closed-lake", prepare_lake, args.output, selection_cost=selection_cost
    )
    cases = [river, full_river, model40, adaptive_work, lake]
    if "whole_domain_difference" in adaptive:
        cases.append(adaptive)
    summary = [
        {
            "case": r["case"],
            "coarse_faces": r["coarse_faces"],
            "reference_faces": r["reference_faces"],
            "maximum_difference_m": r["whole_domain_difference"][
                "maximum_enu_up_difference_m"
            ],
            "rms_difference_m": r["whole_domain_difference"]["integrated_enu_up_rms_m"],
            "independent_reprojection_admitted": r[
                "independent_frame_reprojection_control"
            ]["admitted"],
        }
        for r in cases
    ]
    print(
        json.dumps(
            {
                "comparisons": summary,
                "source_adaptive_attempt": {
                    k: adaptive[k] for k in ("case", "state", "reason") if k in adaptive
                },
                "native_admission": False,
            },
            indent=2,
        )
    )

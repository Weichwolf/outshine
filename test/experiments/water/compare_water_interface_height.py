"""Fixed source-pair witness, independent local fits and actual shared meshes."""

import argparse
import dataclasses
import json
import resource
from pathlib import Path

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_coarse_water_owner import measured
from compare_water_grid_height import inputs
from compare_water_owner_inputs import (
    adjacent_counterexample,
    fixed_case,
    layer_receipts,
    plot_adjacent,
)
from compare_water_region_mesh import (
    independent_audit,
    plot_cases,
    plot_surface_patches,
)
from outshine_ref.contracts import Mesh
from outshine_ref.water import fit_physical_surface
from physical_boundary import to_world
from water_cell_height import body_geometry, project
from water_contact_band import physical_audit, subset
from water_edge_profiles import prepare_profile_owner
from water_field_difference import compare_fields
from water_fixture_cohort import prepare_cohort
from water_interface_height import (
    fit_interface_region,
    interface_region_domain,
    prepare_interface,
    restrict_interface_region,
)
from water_owner_inputs import fixed_interface_inputs
from water_region_mesh import cook_region
from water_shared_borders import shared_borders

RECIPE = "water-interface-r1:radius1:grid1024:support1024:coarse-feasibility-witness"


def build(edge, receipts, snapshots, data, courses, epoch):
    plan = fixed_interface_inputs(edge, receipts, epoch=epoch, model_revision=RECIPE)
    prepared, records, knots, sources = prepare_cohort(
        plan, snapshots, data, courses, cells=plan.cells
    )
    borders, border_work = shared_borders(records, knots, known_cells=plan.tiles)
    border = next(b for b in borders if b["edge"] == edge)
    lines = [
        project(line, edge[0], prepared.anchor)[:, :2] for line in border["polylines"]
    ]
    owner, report = prepare_interface(
        prepared.mesh,
        prepared.anchor,
        prepared.rows,
        prepared.bounds,
        prepared.mask,
        lines,
        edge=edge,
        cells=plan.cells,
        source_version=plan.version,
    )
    if owner is None:
        raise ValueError(f"bounded pair witness unavailable: {report}")
    anchor, polygons = body_geometry(records)
    np.testing.assert_array_equal(anchor, owner.anchor)
    masks, parts, keys = {}, {}, {}
    for cell in plan.cells:
        selected = [i for i, record in enumerate(records) if record[0] == cell]
        parts[cell] = [polygons[i] for i in selected]
        masks[cell] = shapely.union_all(parts[cell])
        keys[cell] = tuple(
            (records[i][0], records[i][1].feature, records[i][1].part, records[i][2])
            for i in selected
        )
    source_points = {
        str(cell): shapely.get_coordinates(
            [
                records[i][1].polygon
                for i in range(len(records))
                if records[i][0] == cell
            ]
        ).tolist()
        for cell in plan.cells
    }
    return (
        owner,
        masks,
        parts,
        keys,
        {
            "plan": dataclasses.asdict(plan),
            "sources": sources,
            "producer": report,
            "other_cohort_borders": border_work,
            "declared_cell_source_points": source_points,
        },
    )


def save_owner(owner, output):
    np.savez_compressed(
        output / "shared-interface.npz",
        initial_vertices=owner.priors.vertices,
        vertices=owner.witness.vertices,
        faces=owner.witness.triangles,
        rows=owner.rows,
        bounds=owner.bounds,
        anchor=owner.anchor,
        mask_wkb=np.frombuffer(owner.mask.wkb, np.uint8),
        band_vertices=owner.band.mesh.vertices,
        band_faces=owner.band.mesh.triangles,
        band_vertex_ids=owner.band.vertex_ids,
        band_face_ids=owner.band.face_ids,
        profile_points=owner.profile.points,
        profile_segments=owner.profile.segments,
        profile_parents=owner.profile.parent_faces,
        source_polyline_xy=np.concatenate(owner.polylines),
        source_polyline_offsets=np.cumsum(
            [0] + [len(line) for line in owner.polylines]
        ),
        edge=np.asarray(owner.edge),
        cells=np.asarray(owner.cells),
        profile_digest=np.asarray(owner.profile.report["profile_content_digest"]),
        source_version=np.asarray(owner.source_version),
        version=np.asarray(owner.version),
        profile_context=np.asarray(owner.profile.context),
    )


def local_case(owner, masks, parts, keys, refinement, order, output):
    output.mkdir(parents=True, exist_ok=True)
    products, fields, rows = {}, {}, []
    for side in order:
        cell = owner.cells[side]
        (regional, fit_report), fit_cost = measured(
            lambda cell=cell: restrict_interface_region(
                owner,
                masks[cell],
                cell=cell,
                expected_source_version=owner.source_version,
            )
        )
        if regional is None:
            raise ValueError(f"common-field restriction refused: {fit_report}")
        fine, fine_cost = measured(
            lambda cell=cell, regional=regional, side=side: cook_region(
                regional.field,
                parts[cell],
                cell=cell,
                source_keys=keys[cell],
                required_edges=(owner.edge,),
                profiles={owner.edge: regional.profile},
                refine_interior=refinement[side],
            )
        )
        audit = independent_audit(regional.field, fine, parts[cell])
        difference = compare_fields(
            owner.witness, regional.field.mesh, domain_mask=masks[cell]
        )
        np.savez_compressed(
            output / f"cell-{side}.npz",
            vertices=fine.mesh.vertices,
            faces=fine.mesh.triangles,
            parent_normals=fine.parent_normals,
            parents=fine.parent_faces,
            root_vertices=regional.field.mesh.vertices,
            root_faces=regional.field.mesh.triangles,
            root_face_ids=regional.domain.face_ids,
            source_offsets=fine.membership.offsets,
            source_members=fine.membership.members,
            source_keys_json=np.asarray(json.dumps(fine.source_keys)),
            contact_edges=fine.contacts[owner.edge],
            frame=owner.anchor,
            local_context=np.asarray(regional.field.context),
            interface_version=np.asarray(owner.version),
        )
        with np.load(output / f"cell-{side}.npz") as saved:
            np.testing.assert_array_equal(
                saved["vertices"][saved["contact_edges"]],
                owner.profile.points[owner.profile.segments],
            )
            np.testing.assert_array_equal(
                saved["source_members"], fine.membership.members
            )
        products[cell], fields[cell] = fine, regional
        rows.append(
            {
                "cell": cell,
                "field": fit_report,
                "field_cost": fit_cost,
                "fine": fine.report,
                "fine_cost": fine_cost,
                "fine_audit": audit,
                "whole_mask_difference_to_bounded_witness": difference.report,
            }
        )
    first, second = [products[cell] for cell in owner.cells]
    lines = [p.mesh.vertices[p.contacts[owner.edge]] for p in (first, second)]
    np.testing.assert_array_equal(lines[0], lines[1])
    world = [
        to_world(owner.anchor, line.reshape(-1, 3)).reshape(line.shape)
        for line in lines
    ]
    gap = float(np.linalg.norm(world[0] - world[1], axis=2).max())
    fractions = np.linspace(0, 1, 17)
    samples = [
        line[:, :1] + fractions[None, :, None] * (line[:, 1:] - line[:, :1])
        for line in world
    ]
    report = {
        "regions": rows,
        "shared_profile_segments": len(owner.profile.segments),
        "whole_declared_p1_edge_endpoint_world_gap_m": gap,
        "seventeen_samples_per_subsegment_world_gap_m": float(
            np.linalg.norm(samples[0] - samples[1], axis=2).max()
        ),
        "whole_edge_argument": "each corresponding piecewise-affine segment has identical endpoints in one frame; affine ECEF placement preserves equality",
        "scope": "this one interface qualified; other cell boundaries/corners, bed and native consumers remain unqualified",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return products, fields, report


def isolated_band_counter(owner, masks, output):
    band = subset(owner.priors, owner.rows, owner.band.face_ids)
    fit = fit_physical_surface(
        band.mesh,
        owner.anchor,
        band.rows,
        up_bounds_m=owner.bounds,
        primary_rows=band.primary,
    )
    if fit.mesh is None:
        return {"band_fit": dict(fit.report), "published": False}
    audit = physical_audit(fit.mesh, owner.anchor, band.rows, owner.bounds)
    reports = []
    np.savez_compressed(
        output / "isolated-band.npz",
        initial_vertices=owner.priors.vertices,
        faces=owner.priors.triangles,
        rows=owner.rows,
        bounds=owner.bounds,
        anchor=owner.anchor,
        band_vertices=fit.mesh.vertices,
        band_vertex_ids=band.vertex_ids,
        band_face_ids=band.face_ids,
    )
    for side, cell in enumerate(owner.cells):
        domain, _candidates, _own = interface_region_domain(owner, masks[cell])
        ids = np.searchsorted(domain.vertex_ids, band.vertex_ids)
        initial = domain.mesh.vertices.copy()
        initial[:, 2] += (-0.25, 0.25)[side]
        fixed = dict(zip(ids.tolist(), fit.mesh.vertices[:, 2].tolist(), strict=True))
        result = fit_physical_surface(
            Mesh(initial, domain.mesh.triangles),
            owner.anchor,
            domain.rows,
            up_bounds_m=owner.bounds,
            primary_rows=domain.primary,
            fixed=fixed,
        )
        reports.append(
            {
                "cell": cell,
                "domain_root_faces": domain.face_ids.tolist(),
                "fit": dict(result.report),
                "mesh_returned": result.mesh is not None,
            }
        )
    return {
        "band_fit": dict(fit.report),
        "band_physical_audit": audit,
        "local_fits": reports,
        "published": False,
        "scope": "reported solver refusal, not a general mathematical infeasibility claim or rejection of all band methods",
    }


def optional_local_qps(owner, masks, output):
    reports = []
    for variant, offsets in (("base", (0, 0)), ("offsets", (-0.25, 0.25))):
        for side, cell in enumerate(owner.cells):
            domain, _candidates, _own = interface_region_domain(owner, masks[cell])
            ids = np.searchsorted(domain.vertex_ids, owner.band.vertex_ids)
            target = domain.mesh.vertices.copy()
            target[:, 2] += offsets[side]
            np.savez_compressed(
                output / f"local-qp-input-{variant}-{side}.npz",
                vertices=target,
                faces=domain.mesh.triangles,
                rows=domain.rows,
                bounds=owner.bounds,
                fixed_ids=ids,
                fixed_heights=owner.band.mesh.vertices[:, 2],
                anchor=owner.anchor,
            )
            (result, report), cost = measured(
                lambda cell=cell, side=side, offsets=offsets: fit_interface_region(
                    owner,
                    masks[cell],
                    cell=cell,
                    expected_source_version=owner.source_version,
                    prior_offset_m=offsets[side],
                )
            )
            difference = (
                None
                if result is None
                else compare_fields(
                    owner.witness, result.field.mesh, domain_mask=masks[cell]
                ).report
            )
            reports.append(
                {
                    "variant": variant,
                    "cell": cell,
                    "mesh_admitted": result is not None,
                    "fit": report,
                    "cost": cost,
                    "difference_to_witness": difference,
                }
            )
    return {
        "regions": reports,
        "selected_for_loading_path": False,
        "scope": "extra local objectives lack an image-gain requirement; solver/fixed-equality refusals remain rejected, no tolerance change",
    }


def independent_baseline(owner, receipts, snapshots, data, courses, epoch, output):
    directory = output / "independent-stencils"
    directory.mkdir(exist_ok=True)
    cell, neighbour = owner.cells[1], owner.cells[0]
    field, product, prepared, report = fixed_case(
        cell, receipts, snapshots, data, courses, epoch
    )
    negative, world = adjacent_counterexample(
        cell,
        receipts,
        snapshots,
        data,
        courses,
        epoch,
        (field, product, prepared),
        report,
        directory,
        neighbour_cell=neighbour,
        shared_edge=owner.edge,
    )
    return negative, world, field.anchor, directory


def overlapping_interface_counter(
    owner, source, other_edge, receipts, snapshots, data, courses, epoch, output
):
    second, masks, _parts, _keys, _report = build(
        tuple(other_edge), receipts, snapshots, data, courses, epoch
    )
    common = sorted(set(owner.cells) & set(second.cells))
    if len(common) != 1:
        raise ValueError("peer interface must share exactly one cell")
    cell = common[0]
    points = np.unique(
        np.asarray(source["declared_cell_source_points"][str(cell)]), axis=0
    )
    xy = [project(points, cell[0], o.anchor)[:, :2] for o in (owner, second)]
    valid = shapely.covers(owner.mask, shapely.points(xy[0])) & shapely.covers(
        masks[cell], shapely.points(xy[1])
    )
    if not valid.any():
        raise ValueError("peer interfaces have no shared strictly covered source point")
    sampled = [
        o.common.sampler.sample(p[valid])
        for o, p in zip((owner, second), xy, strict=True)
    ]
    if any(any(s.reasons) for s in sampled):
        raise ValueError("peer common source point lacks a root field")
    world = [
        to_world(o.anchor, s.points_enu_m)
        for o, s in zip((owner, second), sampled, strict=True)
    ]
    difference = np.linalg.norm(world[0] - world[1], axis=1)
    np.savez_compressed(
        output / "overlapping-interface-counterexample.npz",
        source_points=points[valid],
        first_world=world[0],
        second_world=world[1],
        first_vertices=owner.witness.vertices,
        first_faces=owner.witness.triangles,
        first_anchor=owner.anchor,
        second_vertices=second.witness.vertices,
        second_faces=second.witness.triangles,
        second_anchor=second.anchor,
    )
    return {
        "first_edge": owner.edge,
        "second_edge": second.edge,
        "shared_cell": cell,
        "checked_source_points": int(valid.sum()),
        "maximum_matched_source_point_world_difference_m": float(difference.max()),
        "compatible_multi_interface_owner_proved": False,
        "scope": "two pair owners are not a canonical cell owner; sampled counterexample only, not a whole-cell bound",
    }


def plot_results(
    owner, base, changed, negative, world, baseline_anchor, baseline_dir, output
):
    render_owner = prepare_profile_owner(
        owner.witness, anchor=owner.anchor, mask=owner.mask, owner_version=owner.version
    )
    cases = {
        "shared functional field": (
            {"root_faces": len(owner.witness.triangles)},
            {c: base[c] for c in owner.cells},
            render_owner,
        ),
        "independent interior detail": (
            {"root_faces": len(owner.witness.triangles)},
            {c: changed[c] for c in owner.cells},
            render_owner,
        ),
    }
    plot_cases(cases, output)
    plot_surface_patches(cases, output)
    plot_adjacent(negative, world, baseline_anchor, baseline_dir)


def run(path, output, edge_override=None, peer_edge=None):
    output.mkdir(parents=True, exist_ok=True)
    data, raw, _indices = observed_snapshots(path)
    snapshots = {tile: dataclasses.replace(s, complete=True) for tile, s in raw.items()}
    _data, records, _knots, _origin = inputs(path)
    tiles = sorted({r[0] for r in records})
    bottom = tiles[len(tiles) // 2]
    edge = (
        tuple(edge_override)
        if edge_override is not None
        else (bottom[0], 1, bottom[1], bottom[2])
    )
    epoch = tuple(snapshots[bottom].source[k] for k in ("provider", "revision"))
    (receipts, courses), index_cost = measured(
        lambda: layer_receipts(data, snapshots, epoch)
    )
    (owner, masks, parts, keys, source), producer_cost = measured(
        lambda: build(edge, receipts, snapshots, data, courses, epoch)
    )
    plan = fixed_interface_inputs(edge, receipts, epoch=epoch, model_revision=RECIPE)
    selected = {k: receipts[k] for k in reversed(plan.required)}
    selected_snapshots = {t: snapshots[t] for t in reversed(plan.tiles)}
    replay, _m, _p, _k, replay_source = build(
        edge, selected, selected_snapshots, data, courses, epoch
    )
    if (
        owner.version != replay.version
        or owner.profile.context != replay.profile.context
    ):
        raise ValueError("residence/arrival changed the shared owner")
    np.testing.assert_array_equal(owner.witness.vertices, replay.witness.vertices)
    save_owner(owner, output)
    base, base_fields, base_report = local_case(
        owner, masks, parts, keys, (False, False), (0, 1), output / "base"
    )
    changed, _changed_fields, changed_report = local_case(
        replay,
        masks,
        parts,
        keys,
        (False, True),
        (1, 0),
        output / "varied-interior-detail",
    )
    for cell in owner.cells:
        again, report = restrict_interface_region(
            replay,
            masks[cell],
            cell=cell,
            expected_source_version=replay.source_version,
        )
        if again is None:
            raise ValueError(f"unchanged local replay refused: {report}")
        np.testing.assert_array_equal(
            again.field.mesh.vertices, base_fields[cell].field.mesh.vertices
        )
    isolated = isolated_band_counter(owner, masks, output)
    optional_qps = optional_local_qps(owner, masks, output)
    negative, world, baseline_anchor, baseline_dir = independent_baseline(
        owner, receipts, snapshots, data, courses, epoch, output
    )
    plot_results(
        owner, base, changed, negative, world, baseline_anchor, baseline_dir, output
    )
    peer = (
        None
        if peer_edge is None
        else overlapping_interface_counter(
            owner, source, peer_edge, receipts, snapshots, data, courses, epoch, output
        )
    )
    summary = {
        "source": source,
        "source_index_cost": index_cost,
        "producer_cost": producer_cost,
        "required_only_source": replay_source,
        "identical_owner_across_residence_windows": True,
        "independent_stencil_counterexample": negative,
        "isolated_band_counterexample": isolated,
        "optional_local_qp_comparison": optional_qps,
        "overlapping_interface_counterexample": peer,
        "base": base_report,
        "varied_interior_detail": changed_report,
        "whole_process_max_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "one source-owned bounded interface and local products; no multi-interface/corner/global hydrology/native/device/DB-IO admission",
    }
    (output / "report.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(
        json.dumps(
            {
                "source_cells": len(plan.tiles),
                "coarse_faces": len(owner.witness.triangles),
                "shared_faces": len(owner.band.face_ids),
                "base_world_gap_m": base_report[
                    "whole_declared_p1_edge_endpoint_world_gap_m"
                ],
                "varied_detail_world_gap_m": changed_report[
                    "whole_declared_p1_edge_endpoint_world_gap_m"
                ],
                "independent_stencil_world_gap_m": negative[
                    "maximum_matched_source_knot_world_gap_m"
                ],
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--edge", type=int, nargs=4, metavar=("Z", "AXIS", "X", "Y"))
    parser.add_argument(
        "--peer-edge", type=int, nargs=4, metavar=("Z", "AXIS", "X", "Y")
    )
    args = parser.parse_args()
    run(args.fixture, args.output, args.edge, args.peer_edge)


if __name__ == "__main__":
    main()

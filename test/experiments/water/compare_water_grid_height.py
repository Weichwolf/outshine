"""Fixed-support functional height models, compared over complete original water."""

import argparse
import dataclasses
import hashlib
import json
import resource
from pathlib import Path

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_coarse_water_owner import measured, plot, solve
from compare_physical_river import audit
from outshine_ref.water.frame import normal_caps, tangent_directions
from water_body_domains import assemble
from water_cell_edges import align_edges, edge_knots
from water_cell_handoff import route_cells
from water_cell_height import publish_contacts
from water_contact_band import physical_audit
from water_field_difference import compare_fields
from water_grid_height import prepare_grid, sample_water


def inputs(path, *, kind="river"):
    data, raw, _ = observed_snapshots(path)
    manifest = json.loads(path.read_text())["observed_v3"]
    snapshots = {t: dataclasses.replace(s, complete=True) for t, s in raw.items()}
    cells, work = route_cells(
        snapshots,
        source_tiles=snapshots,
        recipients=snapshots,
        scope="delivered-WATER3-grid:" + manifest["sha256"],
    )
    knots = edge_knots(cells)
    cells, edge_work = align_edges(cells, knots)
    records = [
        (t, p, c.parents[t + (p.feature, p.part)], c.snapshot.source)
        for t, c in cells.items()
        for p in c.snapshot.pieces
    ]
    bodies, rejected, _ = assemble(
        [r[1].polygon for r in records],
        [r[1].info for r in records],
        [r[3] for r in records],
        assume_ground=True,
    )
    if rejected:
        raise ValueError("delivered input cohort rejected")
    body = min(
        (b for b in bodies if records[b.sources[0]][1].info["kind"] == kind),
        key=lambda b: -len(b.sources),
    )
    selected = [records[i] for i in body.sources]
    return (
        data,
        selected,
        knots,
        {
            "input_sha256": manifest["sha256"],
            "decoded_source_bytes": data["source_bytes"],
            "source_work": work,
            "edge_work": edge_work,
            "declared_original_surfaces": len(data["surfaces"]),
            "selected_cells": sorted({r[0] for r in selected}),
            "selected_normalized_pieces": len(selected),
            "scope": "complete declared finite delivered fixture; no external raw completeness, global hydraulic owner or DB/IO claim",
        },
    )


def run_model(spacing, data, records, knots, origin, output):
    prepared, prepare_cost = measured(lambda: prepare_grid(records, data, spacing))
    fitted, fit_cost = measured(
        lambda: solve(prepared.mesh, prepared.anchor, prepared.rows, prepared.bounds)
    )
    physical = physical_audit(
        fitted.mesh, prepared.anchor, prepared.rows, prepared.bounds
    )
    if not physical["admitted"]:
        raise ValueError("independent whole-face physical audit failed")
    sampled_physical, sampled_cost = None, None
    if spacing >= 256:
        caps = normal_caps(
            prepared.anchor,
            prepared.mesh.vertices[:, :2],
            prepared.mesh.triangles,
            prepared.bounds,
        )
        tangent = tangent_directions(
            caps["up"][prepared.rows[:, 0].astype(int)], prepared.rows[:, 1:4]
        )
        (sampled_physical, _), sampled_cost = measured(
            lambda: audit(
                fitted.mesh, prepared.anchor, prepared.rows, prepared.bounds, tangent
            )
        )
        if sampled_physical["sampled_violating_rows"]:
            raise ValueError("independent physical source-frame samples failed")
    digest = hashlib.sha256(
        json.dumps(
            ["fixed-domain-water-grid-v1", origin, prepared.report], sort_keys=True
        ).encode()
    )
    for array in (
        prepared.mesh.vertices,
        prepared.mesh.triangles,
        prepared.rows,
        prepared.anchor,
        np.asarray(prepared.bounds),
        fitted.mesh.vertices,
    ):
        digest.update(np.ascontiguousarray(array).tobytes())
    digest.update(prepared.mask.wkb)
    version = digest.hexdigest()
    name = "grid-" + str(spacing)
    contacts, contact_cost = measured(
        lambda: publish_contacts(
            fitted.mesh,
            prepared.anchor,
            version,
            records,
            knots,
            output / (name + "-contacts.npz"),
            mask=prepared.mask,
        )
    )
    source_xy = np.unique(
        np.concatenate([shapely.get_coordinates(p) for p in prepared.source_polygons]),
        axis=0,
    )
    samples = sample_water(fitted.mesh, prepared.mask, source_xy)
    reasons = {r: samples.reasons.count(r) for r in set(samples.reasons) if r}
    np.savez_compressed(
        output / (name + ".npz"),
        vertices=fitted.mesh.vertices,
        faces=fitted.mesh.triangles,
        initial_vertices=prepared.mesh.vertices,
        rows=prepared.rows,
        bounds=prepared.bounds,
        anchor=prepared.anchor,
        source_xy=source_xy,
        source_height=samples.points_enu_m,
        mask_wkb=np.frombuffer(prepared.mask.wkb, np.uint8),
    )
    report = {
        "spacing_m": spacing,
        "version": version,
        "inputs": prepared.report,
        "prepare_cost": prepare_cost,
        "fit_cost": fit_cost,
        "contact_cost": contact_cost,
        "fit": dict(fitted.report),
        "physical_audit": physical,
        "sampled_physical_audit": sampled_physical,
        "sampled_physical_cost": sampled_cost,
        "contacts": contacts,
        "source_ring_queries": len(source_xy),
        "source_ring_unresolved_by_reason": reasons,
        "geometry_index_bytes": fitted.mesh.vertices.nbytes
        + fitted.mesh.triangles.nbytes,
        "precise_water_area_m2": float(prepared.mask.area),
        "source_mask_holes": sum(
            len(p.interiors) for p in shapely.get_parts(prepared.mask)
        ),
        "scope": "fixed physical generation recipe, not camera-dependent physical/render LOD; native consumers unintegrated",
    }
    (output / (name + ".json")).write_text(json.dumps(report, indent=2))
    return prepared, fitted, report


def run(path, output):
    output.mkdir(parents=True, exist_ok=True)
    (data, records, knots, origin), source_cost = measured(lambda: inputs(path))
    # These owners and shared vertex contacts exist before either fine reference.
    models = {
        s: run_model(s, data, records, knots, origin, output) for s in (1024, 512, 256)
    }
    protected = {s: m[1].mesh.vertices.tobytes() for s, m in models.items()}
    references = {}
    for spacing in (128, 64):
        try:
            references[spacing] = run_model(
                spacing, data, records, knots, origin, output
            )
        except ValueError as error:
            (output / f"reference-{spacing}-refused.json").write_text(
                json.dumps({"state": "not_admitted", "reason": str(error)}, indent=2)
            )
    if 128 not in references:
        raise ValueError("declared fine comparison absent; no quality result")
    reference_spacing = 64 if 64 in references else 128
    reference, fitted_reference, _ = references[reference_spacing]
    results = {}
    for spacing, (prepared, fitted, report) in models.items():
        if protected[spacing] != fitted.mesh.vertices.tobytes():
            raise ValueError("reference construction changed published owner")
        if (
            prepared.mask.wkb != reference.mask.wkb
            or prepared.bounds != reference.bounds
        ):
            raise ValueError("different declared water mask or source-height prism")
        if (
            prepared.report["physical_support_area_m2"]
            != reference.report["physical_support_area_m2"]
        ):
            raise ValueError("changed physical objective support")
        difference, cost = measured(
            lambda fitted=fitted, prepared=prepared: compare_fields(
                fitted.mesh, fitted_reference.mesh, domain_mask=prepared.mask
            )
        )
        results[spacing] = dict(difference.report, comparison_cost=cost)
        np.savez_compressed(
            output / f"difference-{spacing}.npz",
            xy=difference.overlay_xy,
            error=difference.overlay_error_m,
        )
        plot(
            fitted.mesh,
            fitted_reference.mesh,
            difference,
            output / f"difference-{spacing}.png",
            f"Fixed-domain model ({spacing} m)",
            reference_title=f"Same-domain model ({reference_spacing} m)",
            mask=prepared.mask,
        )
    convergence = None
    if 64 in references:
        convergence = compare_fields(
            references[128][1].mesh, references[64][1].mesh, domain_mask=reference.mask
        ).report
    summary = {
        "origin": origin,
        "source_prepare_cost": source_cost,
        "coarse_models_and_vertex_contacts_before_fine": True,
        "fixed_support_spacing_m": 1024,
        "reference_spacing_m": reference_spacing,
        "comparisons": results,
        "128_vs_64_reference_check": convergence,
        "process_peak_rss_bytes_including_all_diagnostics": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "all_copied_bytes": None,
        "all_allocations": None,
        "limits": "Differences between declared functional models, not source/Earth accuracy or a universal quality budget. Fixed dry-halo extension/objective is explicit; whole edge profiles, cross-cohort ownership, rendering/bed/native/device admission remain open. Root grid triangles must never render or answer water outside the precise mask.",
    }
    (output / "comparison.json").write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.fixture, args.output)

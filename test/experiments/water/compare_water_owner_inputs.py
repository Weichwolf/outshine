"""Original-data residence changes versus fixed, two-layer water-owner inputs."""

import argparse
import dataclasses
import hashlib
import json
import resource
from collections import defaultdict
from pathlib import Path

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_coarse_water_owner import measured, solve
from compare_water_grid_height import inputs
from compare_water_region_mesh import independent_audit
from native_water_observation import NO_INPUT
from physical_boundary import from_world, to_world
from water_cell_height import body_geometry, project
from water_contact_band import physical_audit
from water_edge_profiles import prepare_profile_owner, restrict_profile
from water_field_difference import compare_fields
from water_fixture_cohort import prepare_cohort
from water_grid_height import prepare_grid
from water_owner_inputs import LAYERS, LayerReceipt, fixed_owner_inputs
from water_region_mesh import cook_region
from water_shared_borders import adjacent_cells, shared_borders

RECIPE = "river-finite-inputs-r1:radius1:grid1024:support1024:canonical-course-order"


def course_contents(data, index):
    course = data["courses"][index]
    info = data["inputs"][int(course["input"])]
    source = data["sources"][info["source"]]
    count = int(course["point_count"])
    points = data["points_lat_lon"][
        course["first_point"] : course["first_point"] + count
    ]
    levels = data["levels"][course["first_level"] : course["first_level"] + count]
    digest = hashlib.sha256(
        json.dumps(
            {k: v for k, v in info.items() if k != "source"}, sort_keys=True
        ).encode()
    )
    for array in (points, levels, np.asarray(course["half_width_m"])):
        digest.update(np.ascontiguousarray(array).tobytes())
    identity = (
        info["feature"],
        info.get("provider_feature_id") or "",
        hashlib.sha256(points.tobytes()).hexdigest(),
    )
    return tuple(source["tile"]), identity, digest.hexdigest()


def layer_receipts(data, snapshots, epoch):
    """Bind the finite fixture's actual profile payloads, not only water outlines.

    This is an acquisition-time fixture adapter. Building the whole fixture index
    is measured separately; fixed_owner_inputs itself performs bounded lookups.
    """
    grouped = defaultdict(list)
    for index, course in enumerate(data["courses"]):
        if int(course["input"]) == NO_INPUT:
            raise ValueError("fixture course lacks source provenance")
        tile, identity, digest = course_contents(data, index)
        grouped[tile].append((identity, digest, index))
    receipts, course_order = {}, {}
    for tile, snapshot in sorted(snapshots.items()):
        rows = sorted(grouped[tile])
        digest = hashlib.sha256(
            json.dumps(
                [
                    "finite-water-lines-v1",
                    tile,
                    epoch,
                    snapshot.source["digest"],
                    [(r[0], r[1]) for r in rows],
                ],
                sort_keys=True,
            ).encode()
        ).hexdigest()
        receipts[(tile, LAYERS[0])] = LayerReceipt(
            tile, LAYERS[0], epoch, snapshot.digest, snapshot.complete
        )
        receipts[(tile, LAYERS[1])] = LayerReceipt(
            tile, LAYERS[1], epoch, digest, snapshot.complete
        )
        course_order[tile] = tuple(index for _identity, _digest, index in rows)
    return receipts, course_order


def fixed_case(centre, receipts, snapshots, data, course_order, epoch):
    plan = fixed_owner_inputs(centre, receipts, epoch=epoch, model_revision=RECIPE)
    prepared, records, knots, source_report = prepare_cohort(
        plan, snapshots, data, course_order, cells=(centre,)
    )
    fitted = solve(prepared.mesh, prepared.anchor, prepared.rows, prepared.bounds)
    audit = physical_audit(fitted.mesh, prepared.anchor, prepared.rows, prepared.bounds)
    if not audit["admitted"]:
        raise ValueError("fixed source owner failed unchanged physical audit")
    digest = hashlib.sha256(plan.version.encode())
    for array in (prepared.rows, np.asarray(prepared.bounds)):
        digest.update(np.ascontiguousarray(array).tobytes())
    owner = prepare_profile_owner(
        fitted.mesh,
        anchor=prepared.anchor,
        mask=prepared.mask,
        owner_version=digest.hexdigest(),
    )
    borders, border_status = shared_borders(records, knots, known_cells=plan.tiles)
    profiles = {
        b["edge"]: restrict_profile(
            owner,
            [project(line, centre[0], owner.anchor)[:, :2] for line in b["polylines"]],
        )
        for b in borders
    }
    anchor, polygons = body_geometry(records)
    np.testing.assert_array_equal(anchor, owner.anchor)
    selected = [i for i, r in enumerate(records) if r[0] == centre]
    contacts = {
        edge: p for edge, p in profiles.items() if centre in adjacent_cells(edge)
    }
    product = cook_region(
        owner,
        [polygons[i] for i in selected],
        cell=centre,
        source_keys=tuple(
            (records[i][0], records[i][1].feature, records[i][1].part, records[i][2])
            for i in selected
        ),
        required_edges=tuple(contacts),
        profiles=contacts,
    )
    render_audit = independent_audit(owner, product, [polygons[i] for i in selected])
    return (
        owner,
        product,
        prepared,
        {
            "plan": dataclasses.asdict(plan),
            **source_report,
            "fit": dict(fitted.report),
            "physical_audit": audit,
            "border_status": border_status,
            "border_curves": [
                {"edge": b["edge"], "polylines": [p.tolist() for p in b["polylines"]]}
                for b in borders
            ],
            "render": product.report,
            "render_audit": render_audit,
            "owner_context": owner.context,
        },
    )


def variable_windows(path, output):
    data, records, _knots, _origin = inputs(path)
    tiles = sorted({r[0] for r in records})
    models = []
    for count in (5, len(tiles)):
        selected = [r for r in records if r[0] in tiles[:count]]
        prepared = prepare_grid(selected, data, 1024)
        fitted = solve(prepared.mesh, prepared.anchor, prepared.rows, prepared.bounds)
        audit = physical_audit(
            fitted.mesh, prepared.anchor, prepared.rows, prepared.bounds
        )
        if not audit["admitted"]:
            raise ValueError("variable-window baseline failed unchanged physical audit")
        models.append((prepared, fitted))
        np.savez_compressed(
            output / f"variable-{count}.npz",
            vertices=fitted.mesh.vertices,
            faces=fitted.mesh.triangles,
            anchor=prepared.anchor,
            mask_wkb=np.frombuffer(prepared.mask.wkb, np.uint8),
        )
    np.testing.assert_array_equal(models[0][0].anchor, models[1][0].anchor)
    difference = compare_fields(
        models[0][1].mesh, models[1][1].mesh, domain_mask=models[0][0].mask
    )
    return difference, tiles[len(tiles) // 2]


def profile_changes(data, snapshots, epoch, centre, first, report, output):
    """Controlled copies retain the original fixture and change one soft profile."""
    inside_ids = set(report["finite_course_table_original_indices"])
    inside_levels = set()
    for i in inside_ids:
        row = data["courses"][i]
        inside_levels.update(
            range(int(row["first_level"]), int(row["first_level"] + row["point_count"]))
        )
    outside_id = next(
        i
        for i, row in enumerate(data["courses"])
        if i not in inside_ids
        and not inside_levels.intersection(
            range(int(row["first_level"]), int(row["first_level"] + row["point_count"]))
        )
    )
    inside_id = report["selected_original_course_table_indices"][0]
    results, changed_difference = {}, None
    for name, original in (
        ("outside-stencil", outside_id),
        ("inside-stencil", inside_id),
    ):
        changed = dict(data, levels=data["levels"].copy())
        row = data["courses"][original]
        changed["levels"][
            row["first_level"] : row["first_level"] + row["point_count"]
        ] += 1
        updated, order = layer_receipts(changed, snapshots, epoch)
        owner, product, prepared, candidate = fixed_case(
            centre, updated, snapshots, changed, order, epoch
        )
        difference = compare_fields(
            first[0].mesh, owner.mesh, domain_mask=first[0].mask
        )
        same = owner.context == first[0].context
        if name == "outside-stencil":
            if not same or difference.report["maximum_enu_up_difference_m"] != 0:
                raise ValueError("unconsumed profile changed the fixed owner")
            np.testing.assert_array_equal(product.mesh.vertices, first[1].mesh.vertices)
            np.testing.assert_array_equal(
                product.mesh.triangles, first[1].mesh.triangles
            )
        else:
            if same or candidate["plan"]["version"] == report["plan"]["version"]:
                raise ValueError("consumed profile failed to invalidate fixed inputs")
            changed_difference = difference
            np.savez_compressed(
                output / "controlled-profile-update.npz",
                vertices=owner.mesh.vertices,
                faces=owner.mesh.triangles,
                anchor=owner.anchor,
                initial_vertices=prepared.mesh.vertices,
            )
        info = data["inputs"][int(row["input"])]
        results[name] = {
            "original_course_table_index": original,
            "original_source_tile": data["sources"][info["source"]]["tile"],
            "original_source_identity": info,
            "same_context": same,
            "difference": difference.report,
            "physical_audit": candidate["physical_audit"],
            "scope": "controlled +1m soft native-profile mutation on a copy, not a newly observed Earth height",
        }
    if changed_difference is None:
        raise ValueError("controlled profile update missing")
    return results, changed_difference


def adjacent_counterexample(
    centre,
    receipts,
    snapshots,
    data,
    courses,
    epoch,
    first,
    report,
    output,
    *,
    neighbour_cell=None,
    shared_edge=None,
):
    neighbour = (
        neighbour_cell
        if neighbour_cell is not None
        else (centre[0], centre[1], centre[2] - 1)
    )
    owner, _product, prepared, second = fixed_case(
        neighbour, receipts, snapshots, data, courses, epoch
    )
    edge = (
        shared_edge if shared_edge is not None else (centre[0], 1, centre[1], centre[2])
    )
    if {centre, neighbour} != set(adjacent_cells(edge)):
        raise ValueError("counterexample cells do not share the declared edge")
    border = next(b for b in report["border_curves"] if tuple(b["edge"]) == edge)
    geographic = np.unique(np.concatenate(border["polylines"]), axis=0)
    projected = [project(geographic, centre[0], o.anchor) for o in (first[0], owner)]
    xy = [p[:, :2] for p in projected]
    valid = shapely.covers(first[0].mask, shapely.points(xy[0])) & shapely.covers(
        owner.mask, shapely.points(xy[1])
    )
    if not valid.any():
        raise ValueError(
            "adjacent-stencil fixture has no strictly covered common source knot"
        )
    samples = [
        o.sampler.sample(points[valid])
        for o, points in zip((first[0], owner), xy, strict=True)
    ]
    if any(any(s.reasons) for s in samples):
        raise ValueError("adjacent shared knot lacks functional root height")
    world = [
        to_world(o.anchor, sample.points_enu_m)
        for o, sample in zip((first[0], owner), samples, strict=True)
    ]
    gap = np.linalg.norm(world[0] - world[1], axis=1)
    zero_world = [
        to_world(o.anchor, p[valid])
        for o, p in zip((first[0], owner), projected, strict=True)
    ]
    delta_in_first_frame = (
        from_world(first[0].anchor, world[1]) - samples[0].points_enu_m
    )
    np.savez_compressed(
        output / "adjacent-stencil-counterexample.npz",
        mercator_source_knots=geographic[valid],
        first_anchor=first[0].anchor,
        second_anchor=owner.anchor,
        first_world=world[0],
        second_world=world[1],
        world_gap_m=gap,
        first_owner_vertices=first[0].mesh.vertices,
        first_owner_faces=first[0].mesh.triangles,
        first_rows=first[2].rows,
        first_bounds=first[2].bounds,
        second_owner_vertices=owner.mesh.vertices,
        second_owner_faces=owner.mesh.triangles,
        second_rows=prepared.rows,
        second_bounds=prepared.bounds,
    )
    (output / "adjacent-stencil.json").write_text(json.dumps(second, indent=2) + "\n")
    result = {
        "first_centre": centre,
        "second_centre": neighbour,
        "edge": edge,
        "common_source_knots": len(geographic),
        "both_masks_strictly_covered_knots": int(valid.sum()),
        "matched_source_knot_world_gap_m": gap.tolist(),
        "maximum_matched_source_knot_world_gap_m": float(gap.max()),
        "maximum_shared_source_zero_height_roundtrip_gap_m": float(
            np.linalg.norm(zero_world[0] - zero_world[1], axis=1).max()
        ),
        "maximum_gap_along_first_owner_up_m": float(
            abs(delta_in_first_frame[:, 2]).max()
        ),
        "maximum_gap_in_first_owner_horizontal_plane_m": float(
            np.linalg.norm(delta_in_first_frame[:, :2], axis=1).max()
        ),
        "both_owners_pass_unchanged_physical_audit": True,
        "compatible_adjacent_height_owners_proved": False,
        "scope": "point counterexample only, not a whole-edge maximum; independent stencil frames/heights lack a common interface owner",
    }
    return result, world


def run(path, output):
    output.mkdir(parents=True, exist_ok=True)
    variable, centre = variable_windows(path, output)
    data, raw, _indices = observed_snapshots(path)
    snapshots = {tile: dataclasses.replace(s, complete=True) for tile, s in raw.items()}
    epoch = tuple(snapshots[centre].source[k] for k in ("provider", "revision"))
    (receipts, courses), acquisition_cost = measured(
        lambda: layer_receipts(data, snapshots, epoch)
    )
    initial = fixed_owner_inputs(centre, receipts, epoch=epoch, model_revision=RECIPE)
    smallest = {key: receipts[key] for key in reversed(initial.required)}
    few_snapshots = {tile: snapshots[tile] for tile in reversed(initial.tiles)}
    cases, reports = [], []
    for name, metadata, resident in (
        ("required-only", smallest, few_snapshots),
        ("all-resident", receipts, snapshots),
    ):
        _plan, plan_cost = measured(
            lambda metadata=metadata: fixed_owner_inputs(
                centre, metadata, epoch=epoch, model_revision=RECIPE
            )
        )
        (owner, product, prepared, report), cost = measured(
            lambda metadata=metadata, resident=resident: fixed_case(
                centre, metadata, resident, data, courses, epoch
            )
        )
        np.savez_compressed(
            output / f"{name}.npz",
            owner_vertices=owner.mesh.vertices,
            owner_faces=owner.mesh.triangles,
            vertices=product.mesh.vertices,
            faces=product.mesh.triangles,
            anchor=owner.anchor,
            mask_wkb=np.frombuffer(owner.mask.wkb, np.uint8),
            context=np.asarray(owner.context),
            source_offsets=product.membership.offsets,
            source_members=product.membership.members,
        )
        cases.append((owner, product, prepared))
        reports.append(report)
        report["cost"] = cost
        report["input_plan_cost"] = plan_cost
        (output / f"{name}.json").write_text(json.dumps(report, indent=2) + "\n")
    first, second = cases
    if first[0].context != second[0].context:
        raise ValueError("irrelevant residence data changed owner context")
    for a, b in ((first[0].mesh, second[0].mesh), (first[1].mesh, second[1].mesh)):
        np.testing.assert_array_equal(a.vertices, b.vertices)
        np.testing.assert_array_equal(a.triangles, b.triangles)
    fixed_difference = compare_fields(
        first[0].mesh, second[0].mesh, domain_mask=first[0].mask
    )
    reordered = dict(data, courses=data["courses"][::-1])
    ordered_receipts, reversed_order = layer_receipts(reordered, snapshots, epoch)
    permuted_owner, permuted_product, _prepared, _report = fixed_case(
        centre, ordered_receipts, snapshots, reordered, reversed_order, epoch
    )
    if permuted_owner.context != first[0].context:
        raise ValueError("course table permutation changed the functional owner")
    np.testing.assert_array_equal(
        permuted_product.mesh.vertices, first[1].mesh.vertices
    )
    np.testing.assert_array_equal(
        permuted_product.mesh.triangles, first[1].mesh.triangles
    )
    mutations, mutation_difference = profile_changes(
        data, snapshots, epoch, centre, first, reports[0], output
    )
    adjacent, adjacent_world = adjacent_counterexample(
        centre, receipts, snapshots, data, courses, epoch, first, reports[0], output
    )
    key = (centre, "water_lines")
    missing = dict(receipts)
    missing.pop(key)
    pending = fixed_owner_inputs(centre, missing, epoch=epoch, model_revision=RECIPE)
    if pending.state != "pending_sources" or pending.version is not None:
        raise ValueError("missing profile layer incorrectly admitted an owner")
    summary = {
        "variable_windows": variable.report,
        "fixed_windows": fixed_difference.report,
        "centre": centre,
        "fixed_input_plan": dataclasses.asdict(initial),
        "acquisition_cost": acquisition_cost,
        "missing_profile": dataclasses.asdict(pending),
        "all_resident_source_cells": len(snapshots),
        "all_resident_course_rows": len(data["courses"]),
        "required_only_source_cells": len(initial.tiles),
        "root_faces": len(first[0].mesh.triangles),
        "requested_cell_faces": len(first[1].mesh.triangles),
        "profile_changes": mutations,
        "native_course_table_permutation_identical": True,
        "adjacent_stencil_counterexample": adjacent,
        "scope": "one fixed finite stencil, actual arrays already decoded/resident; no cross-stencil seam, global hydrology, DB-IO/native-device proof",
    }
    plot(variable, first, mutation_difference, output)
    plot_adjacent(adjacent, adjacent_world, first[0].anchor, output)
    summary["whole_process_max_rss_bytes_linux"] = (
        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    )
    (output / "report.json").write_text(json.dumps(summary, indent=2) + "\n")
    return summary


def plot(variable, first, mutation, output):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt
    from matplotlib.collections import PolyCollection

    fig, axes = plt.subplots(1, 3, figsize=(16, 5), constrained_layout=True)
    colours = variable.overlay_error_m.mean(axis=1) * 100
    artist = PolyCollection(
        variable.overlay_xy, array=colours, cmap="coolwarm", edgecolors="none"
    )
    axes[0].add_collection(artist)
    fig.colorbar(artist, ax=axes[0], label="prefix5 minus full9 owner Up [cm]")
    axes[0].set_title("Changed source window: same admitted water")
    owner, product, _prepared = first
    for polygon in shapely.get_parts(owner.mask):
        xy = np.asarray(polygon.exterior.coords)
        axes[1].plot(*xy.T, color="grey", linewidth=0.6)
    axes[1].add_collection(
        PolyCollection(
            product.mesh.vertices[product.mesh.triangles, :2],
            facecolors="#4d9fc8",
            edgecolors="black",
            linewidths=0.35,
        )
    )
    for segments in product.contacts.values():
        for segment in segments:
            axes[1].plot(
                *product.mesh.vertices[segment, :2].T, color="red", linewidth=2
            )
    axes[1].set_title("Fixed inputs: identical owner and requested cell")
    artist = PolyCollection(
        mutation.overlay_xy,
        array=mutation.overlay_error_m.mean(axis=1) * 100,
        cmap="coolwarm",
        edgecolors="none",
    )
    axes[2].add_collection(artist)
    fig.colorbar(artist, ax=axes[2], label="base minus changed owner Up [cm]")
    axes[2].set_title("Controlled +1m soft-profile update")
    for axis in axes:
        axis.autoscale()
        axis.set_aspect("equal")
        axis.set_xlabel("owner ENU east [m]")
        axis.set_ylabel("owner ENU north [m]")
    fig.savefig(output / "owner-windows.png", dpi=150)
    plt.close(fig)


def plot_adjacent(report, world, anchor, output):
    from matplotlib import pyplot as plt

    first, second = [from_world(anchor, points) for points in world]
    along = np.r_[0, np.cumsum(np.linalg.norm(np.diff(first[:, :2], axis=0), axis=1))]
    fig, axes = plt.subplots(1, 2, figsize=(10, 4), constrained_layout=True)
    for name, points in (
        ("first fixed stencil", first),
        ("neighbour fixed stencil", second),
    ):
        axes[0].plot(along, points[:, 2], marker="o", label=name)
    axes[0].set_ylabel("first-owner ENU up [m]")
    axes[0].legend()
    axes[1].plot(
        along,
        np.asarray(report["matched_source_knot_world_gap_m"]) * 100,
        marker="o",
        color="red",
    )
    axes[1].set_ylabel("matched source-knot world gap [cm]")
    for axis in axes:
        axis.set_xlabel("distance along tested source knots [m]")
        axis.grid(alpha=0.3)
    fig.suptitle(
        "Counterexample: bounded fixed inputs alone do not make compatible neighbours"
    )
    fig.savefig(output / "adjacent-stencil-gap.png", dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = run(args.fixture, args.output)
    print(
        json.dumps(
            {
                "variable_up_max_m": report["variable_windows"][
                    "maximum_enu_up_difference_m"
                ],
                "fixed_up_max_m": report["fixed_windows"][
                    "maximum_enu_up_difference_m"
                ],
                "fixed_source_cells": report["required_only_source_cells"],
                "root_faces": report["root_faces"],
                "requested_cell_faces": report["requested_cell_faces"],
            }
        )
    )


if __name__ == "__main__":
    main()

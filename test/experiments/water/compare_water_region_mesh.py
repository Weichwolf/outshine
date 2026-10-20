"""Cook/load original water cells independently from a common coarse owner."""

import argparse
import hashlib
import json
import resource
from pathlib import Path

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_coarse_water_owner import measured, solve
from compare_water_grid_height import inputs, run_model
from water_cell_height import body_geometry, prepare, project
from water_edge_profiles import prepare_profile_owner, restrict_profile
from water_region_mesh import cook_region
from water_shared_borders import adjacent_cells, shared_borders


def make_owner(kind, path, output):
    (data, records, knots, origin), source_cost = measured(
        lambda: inputs(path, kind=kind)
    )
    anchor, polygons = body_geometry(records)
    mask = shapely.union_all(polygons)
    if kind == "river":
        prepared, fitted, model = run_model(1024, data, records, knots, origin, output)
        anchor, mask, version = prepared.anchor, prepared.mask, model["version"]
        mesh = fitted.mesh
    else:
        _data, _snapshots, indices = observed_snapshots(path)
        mesh, anchor, rows, bounds, _csr, _meta = prepare(records, data, indices)
        fitted = solve(mesh, anchor, rows, bounds)
        mesh = fitted.mesh
        version = hashlib.sha256(
            ("lake-soft-native-source-v1:" + origin["input_sha256"]).encode()
        ).hexdigest()
        model = {"fit": dict(fitted.report), "version": version}
    owner, owner_cost = measured(
        lambda: prepare_profile_owner(
            mesh, anchor=anchor, mask=mask, owner_version=version
        )
    )
    borders, border_work = shared_borders(
        records, knots, known_cells=[tuple(s["tile"]) for s in data["sources"]]
    )
    profiles = {}
    for border in borders:
        lines = [
            project(line, border["edge"][0], anchor)[:, :2]
            for line in border["polylines"]
        ]
        profiles[border["edge"]] = restrict_profile(owner, lines)
    return (
        owner,
        records,
        polygons,
        profiles,
        {
            "source": origin,
            "source_cost": source_cost,
            "model": model,
            "owner_cost": owner_cost,
            "border_status": border_work,
            "profile_numeric_bytes": sum(
                p.report["numeric_payload_bytes"] for p in profiles.values()
            ),
        },
    )


def independent_audit(owner, product, source_polygons):
    mesh = product.mesh
    triangles = mesh.vertices[mesh.triangles]
    polygons = shapely.polygons(triangles[:, :, :2])
    union = shapely.union_all(polygons)
    source = shapely.union_all(source_polygons)
    missing_parent = max(
        p.difference(owner.polygons[int(parent)]).area
        for p, parent in zip(polygons, product.parent_faces, strict=True)
    )
    centres = triangles.mean(axis=1)
    samples = owner.sampler.sample(centres[:, :2], max_queries=16384)
    if any(samples.reasons):
        raise ValueError("independent child-interior query lacks root height")
    error = float(abs(samples.points_enu_m[:, 2] - centres[:, 2]).max())
    crosses = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    crosses /= np.linalg.norm(crosses, axis=1)[:, None]
    normal_difference = np.linalg.norm(crosses - product.parent_normals, axis=1)
    missing, outside = source.difference(union).area, union.difference(source).area
    tolerance = max(1e-8, source.area * 1e-12)
    if max(missing, outside, missing_parent) > tolerance or error > 1e-9:
        raise ValueError("independent parent/coverage/interior-height audit failed")
    return {
        "uncovered_area_m2": missing,
        "outside_area_m2": outside,
        "maximum_child_outside_parent_area_m2": missing_parent,
        "maximum_child_centroid_up_error_m": error,
        "maximum_recomputed_vs_inherited_normal_difference": float(
            normal_difference.max()
        ),
        "recomputed_normal_differences_above_1e_minus9": int(
            (normal_difference > 1e-9).sum()
        ),
        "physical_recomputed_child_normal_admission": False,
    }


def save_and_reload(path, owner, product, profiles):
    keys = sorted(product.contacts)
    np.savez_compressed(
        path,
        vertices=product.mesh.vertices,
        faces=product.mesh.triangles,
        parents=product.parent_faces,
        parent_normals=product.parent_normals,
        source_offsets=product.membership.offsets,
        source_members=product.membership.members,
        source_keys_json=np.asarray(json.dumps(product.source_keys)),
        anchor=owner.anchor,
        context=np.asarray(product.context),
        edges=np.asarray(keys, dtype=np.int64).reshape(-1, 4),
        **{f"contact_{i}": product.contacts[key] for i, key in enumerate(keys)},
    )
    with np.load(path) as data:
        if data["context"].item() != owner.context or not np.array_equal(
            data["anchor"], owner.anchor
        ):
            raise ValueError("saved cell owner/frame mismatch")
        vertices = data["vertices"].copy()
        for i, key in enumerate(keys):
            np.testing.assert_array_equal(
                vertices[data[f"contact_{i}"]],
                profiles[key].points[profiles[key].segments],
            )
        np.testing.assert_array_equal(
            data["source_members"], product.membership.members
        )
        np.testing.assert_array_equal(
            data["source_offsets"], product.membership.offsets
        )
        np.testing.assert_array_equal(data["parents"], product.parent_faces)
        np.testing.assert_array_equal(data["parent_normals"], product.parent_normals)
        np.testing.assert_array_equal(data["edges"], np.asarray(keys).reshape(-1, 4))
        if json.loads(data["source_keys_json"].item()) != json.loads(
            json.dumps(product.source_keys)
        ):
            raise ValueError("saved source identities differ from the local product")
    return {
        "compressed_file_bytes": path.stat().st_size,
        "exact_loaded_contact_segments": sum(len(a) for a in product.contacts.values()),
    }


def run_case(kind, path, output):
    output.mkdir(parents=True, exist_ok=True)
    owner, records, polygons, profiles, report = make_owner(kind, path, output)
    products, results = {}, []
    for index, cell in enumerate(sorted({r[0] for r in records})):
        selected = [i for i, record in enumerate(records) if record[0] == cell]
        sources = [polygons[i] for i in selected]
        keys = tuple(
            (records[i][0], records[i][1].feature, records[i][1].part, records[i][2])
            for i in selected
        )
        contacts = {
            edge: p for edge, p in profiles.items() if cell in adjacent_cells(edge)
        }
        options = {
            "cell": cell,
            "source_keys": keys,
            "required_edges": tuple(contacts),
            "profiles": contacts,
        }
        product, cost = measured(
            lambda sources=sources, options=options: cook_region(
                owner, sources, **options
            )
        )
        audit = independent_audit(owner, product, sources)
        saved = save_and_reload(output / f"cell-{index}.npz", owner, product, contacts)
        refined, refined_cost = measured(
            lambda sources=sources, options=options: cook_region(
                owner, sources, refine_interior=True, **options
            )
        )
        refined_audit = independent_audit(owner, refined, sources)
        for edge in contacts:
            np.testing.assert_array_equal(
                product.mesh.vertices[product.contacts[edge]],
                refined.mesh.vertices[refined.contacts[edge]],
            )
        products[cell] = product
        results.append(
            {
                "product": product.report,
                "cost": cost,
                "audit": audit,
                "saved": saved,
                "interior_refined": refined.report,
                "refined_cost": refined_cost,
                "refined_audit": refined_audit,
            }
        )
    # Reversed arrival order, separate invocation, no shared vertex/topology builder.
    for cell in sorted(products, reverse=True):
        selected = [
            i for i, record in reversed(list(enumerate(records))) if record[0] == cell
        ]
        contacts = {
            edge: p for edge, p in profiles.items() if cell in adjacent_cells(edge)
        }
        repeat = cook_region(
            owner,
            [polygons[i] for i in selected],
            cell=cell,
            source_keys=tuple(
                (
                    records[i][0],
                    records[i][1].feature,
                    records[i][1].part,
                    records[i][2],
                )
                for i in selected
            ),
            required_edges=tuple(contacts),
            profiles=contacts,
        )
        np.testing.assert_array_equal(
            repeat.mesh.vertices, products[cell].mesh.vertices
        )
        np.testing.assert_array_equal(
            repeat.mesh.triangles, products[cell].mesh.triangles
        )
    global_product, global_cost = measured(
        lambda: cook_region(
            owner,
            polygons,
            cell=min(products),
            source_keys=tuple(r[2] for r in records),
            required_edges=(),
            profiles={},
        )
    )
    report.update(
        {
            "whole_cohort_render_cut": {
                "product": global_product.report,
                "cost": global_cost,
            },
            "cells": results,
            "independent_cell_jobs": len(products),
            "root_faces": len(owner.mesh.triangles),
            "cohort_context": owner.context,
            "source_identity_rows": len(records),
            "scope": "one finite delivered common owner; no cross-cohort canonical owner, new physical fit, global lake/coast/bed, selective DB query or native budget proof",
        }
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report, products, owner


def plot_cases(cases, output):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt
    from matplotlib.collections import PolyCollection

    fig, axes = plt.subplots(1, 2, figsize=(13, 6), constrained_layout=True)
    for axis, (kind, (report, products, owner)) in zip(
        axes, cases.items(), strict=True
    ):
        for index, product in enumerate(products.values()):
            xy = product.mesh.vertices[product.mesh.triangles, :2]
            axis.add_collection(
                PolyCollection(
                    xy,
                    facecolors=f"C{index % 10}",
                    edgecolors="black",
                    linewidths=0.25,
                    alpha=0.65,
                )
            )
            for edges in product.contacts.values():
                for edge in edges:
                    line = product.mesh.vertices[edge]
                    axis.plot(line[:, 0], line[:, 1], color="red", linewidth=2)
        axis.autoscale()
        axis.set_aspect("equal")
        axis.set_title(
            f"{kind}: {len(products)} independent cells, {report['root_faces']} root facets"
        )
        axis.set_xlabel("owner ENU east [m]")
        axis.set_ylabel("owner ENU north [m]")
    fig.suptitle(
        "Precise water masks / local render facets / exact common contacts (red)"
    )
    fig.savefig(output / "regions.png", dpi=150)
    plt.close(fig)


def plot_surface_patches(cases, output):
    from matplotlib import pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    fig = plt.figure(figsize=(12, 5), layout="constrained")
    light = np.array([0.3, -0.4, 1.0])
    light /= np.linalg.norm(light)
    for index, (kind, (_report, products, _owner)) in enumerate(cases.items()):
        axis = fig.add_subplot(1, 2, index + 1, projection="3d", computed_zorder=False)
        lines = [
            p.mesh.vertices[edge]
            for p in products.values()
            for edges in p.contacts.values()
            for edge in edges
        ]
        seam = max(lines, key=lambda line: np.linalg.norm(line[1, :2] - line[0, :2]))
        centre = seam.mean(axis=0)
        radius = max(25, 2 * np.linalg.norm(seam[1, :2] - seam[0, :2]))
        selected = []
        for product in products.values():
            triangles = product.mesh.vertices[product.mesh.triangles]
            keep = (
                np.linalg.norm(triangles[:, :, :2].mean(axis=1) - centre[:2], axis=1)
                <= radius
            )
            triangles = triangles[keep] - centre
            if not len(triangles):
                continue
            selected.append(triangles)
            shade = np.clip(product.parent_normals[keep] @ light, 0.2, 1)
            rgba = np.c_[0.2 * shade, 0.65 * shade, 0.9 * shade, np.ones(len(shade))]
            axis.add_collection3d(
                Poly3DCollection(
                    triangles, facecolors=rgba, edgecolors="black", linewidths=0.3
                )
            )
        if not selected:
            raise ValueError("no surface triangles around actual shared contact")
        vertices = np.concatenate(selected).reshape(-1, 3)
        axis.auto_scale_xyz(vertices[:, 0], vertices[:, 1], vertices[:, 2])
        for line in lines:
            if np.linalg.norm(line.mean(axis=0)[:2] - centre[:2]) <= radius:
                axis.plot(*(line - centre).T, color="red", linewidth=2, zorder=10)
        axis.set_xlim(-radius, radius)
        axis.set_ylim(-radius, radius)
        axis.set_title(f"{kind}: local surface around shared edge")
        axis.set_xlabel("east offset [m]")
        axis.set_ylabel("north offset [m]")
        axis.set_zlabel("owner up offset [m]")
        axis.view_init(elev=25, azim=-60)
    fig.suptitle(
        "Geometry diagnostic; independent axis scaling; inherited root normals"
    )
    fig.savefig(output / "surface-patches.png", dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cases = {
        kind: run_case(kind, args.fixture, args.output / kind)
        for kind in ("river", "lake")
    }
    plot_cases(cases, args.output)
    plot_surface_patches(cases, args.output)
    summary = {
        kind: {
            "cells": result[0]["independent_cell_jobs"],
            "faces": sum(len(p.mesh.triangles) for p in result[1].values()),
            "numeric_payload_bytes": sum(
                p.report["numeric_payload_bytes"] for p in result[1].values()
            ),
        }
        for kind, result in cases.items()
    }
    summary["whole_process_max_rss_bytes_linux"] = (
        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    )
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary))


if __name__ == "__main__":
    main()

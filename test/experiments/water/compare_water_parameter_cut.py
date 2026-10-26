"""Reproduce strict local-frame water cuts; write private diagnostics only."""

import argparse
import json
import resource
from collections import defaultdict
from pathlib import Path

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT
from compare_coarse_water_owner import measured
from compare_water_grid_height import inputs
from outshine_ref.places.sources import tile_geography
from physical_boundary import from_world
from water_colored_reference import ReferenceUpdates
from water_envelope_fixture import acquire, prepare_parent
from water_nodal_height import cell_faces, parameter_triangle, sample_face
from water_parameter_cut import cut_cell
from water_region_mesh import boundary_edges


def profile(product, axis, side):
    xy = product.parameter.reshape(-1, 2)
    points, first, inverse = np.unique(
        xy, axis=0, return_index=True, return_inverse=True
    )
    world = product.world.reshape(-1, 3)[first]
    edges = boundary_edges(inverse.reshape(-1, 3))
    edges = edges[np.all(points[edges, axis] == side, axis=1)]
    segments = sorted(tuple(sorted(points[edge, 1 - axis])) for edge in edges)
    indices = np.unique(edges)
    indices = indices[np.argsort(points[indices, 1 - axis])]
    return segments, points[indices, 1 - axis], world[indices]


def verify_contacts(products, refined):
    contacts = []
    for cell, product in sorted(products.items()):
        z, x, y = cell
        for axis, neighbour in ((0, (z, x + 1, y)), (1, (z, x, y + 1))):
            if neighbour not in products:
                continue
            left = profile(product, axis, 1)
            right = profile(products[neighbour], axis, 0)
            if left[0] != right[0]:
                raise ValueError("neighbour boundary segments differ")
            np.testing.assert_array_equal(left[1], right[1])
            np.testing.assert_array_equal(left[2], right[2])
            for selected, side in ((cell, 1), (neighbour, 0)):
                coarse = profile(products[selected], axis, side)
                fine = profile(refined[selected], axis, side)
                if coarse[0] != fine[0]:
                    raise ValueError("interior refinement changed boundary segments")
                np.testing.assert_array_equal(coarse[1], fine[1])
                np.testing.assert_array_equal(coarse[2], fine[2])
            if left[0]:
                contacts.append(
                    {
                        "cells": [cell, neighbour],
                        "segments": len(left[0]),
                        "world_gap_m": 0.0,
                    }
                )
    return contacts


def save_product(output, product):
    arrays = {
        "parameter_tile_units": product.parameter,
        "world_ecef_m": product.world,
        "parent_faces": product.parents,
        "parent_normals": product.normals,
        "source_offsets": product.membership.offsets,
        "source_members": product.membership.members,
        "source_representative": product.membership.representative,
        "source_overlap_projected_m2": product.membership.member_area_m2,
        "source_uncovered_projected_m2": product.membership.uncovered_area_m2,
        "source_status": product.membership.status,
        "source_keys_json": np.asarray(json.dumps(product.source_keys)),
        "cell": np.asarray(product.cell),
        "context": np.asarray(product.context),
    }
    path = output / ("cell-" + "-".join(map(str, product.cell)) + ".npz")
    np.savez_compressed(path, **arrays)
    with np.load(path) as saved:
        for key, value in arrays.items():
            np.testing.assert_array_equal(saved[key], value)


def source_memberships(product):
    csr = product.membership
    return [
        sorted(
            json.dumps(product.source_keys[j], sort_keys=True) for j in csr.members[a:b]
        )
        for a, b in zip(csr.offsets[:-1], csr.offsets[1:], strict=True)
    ]


def welding_diagnostic(product):
    points, first, inverse = np.unique(
        product.parameter.reshape(-1, 2), axis=0, return_index=True, return_inverse=True
    )
    world = product.world.reshape(-1, 3)
    jitter = np.linalg.norm(world - world[first][inverse], axis=1)
    edges = boundary_edges(inverse.reshape(-1, 3))
    local = [shapely.Polygon(triangle) for triangle in product.parameter]
    boundary = shapely.union_all(local).boundary
    midpoint = points[edges].mean(axis=1)
    distances = shapely.distance(shapely.points(midpoint), boundary)
    return {
        "exact_parameter_vertices": len(points),
        "maximum_duplicate_parameter_world_difference_m": float(jitter.max()),
        "unpaired_edges": len(edges),
        "maximum_unpaired_midpoint_distance_to_mask_boundary_tile": float(
            distances.max()
        ),
        "scope": "diagnostic only; no welded topology or conforming interior-edge proof",
    }


def global_refusals(cell, polygons):
    mask = shapely.union_all(polygons)
    failed = total = 0
    for keys in cell_faces(cell):
        root = parameter_triangle(keys)
        cut = shapely.Polygon(root).intersection(mask)
        for part in shapely.get_parts(cut):
            if part.geom_type != "Polygon" or part.area <= 0:
                continue
            for triangle in shapely.get_parts(
                shapely.constrained_delaunay_triangles(part)
            ):
                total += 1
                try:
                    sample_face(
                        keys,
                        np.c_[root, np.zeros(3)],
                        np.asarray(triangle.exterior.coords)[:3],
                    )
                except ValueError as error:
                    if str(error) != "query outside canonical facet":
                        raise
                    failed += 1
    return {"triangles": total, "sampler_refusals": failed}


def plot(products, records, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    figure = plt.figure(figsize=(14, 7))
    top, space = figure.add_subplot(121), figure.add_subplot(122, projection="3d")
    centre = np.mean([r[1].polygon.centroid.coords[0] for r in records], axis=0)
    lat, lon = tile_geography([centre], 1, (records[0][0][0], 0, 0))
    anchor = np.array([lon[0], lat[0]])
    all_world = []
    detail = max(products, key=lambda cell: len(products[cell].world), default=None)
    for cell in sorted({r[0] for r in records}):
        selected = [r for r in records if r[0] == cell]
        for record in selected:
            polygon = record[1].polygon
            for part in shapely.get_parts(polygon):
                rings = [part.exterior, *part.interiors]
                for ring in rings:
                    xy = np.asarray(ring.coords) - centre
                    top.plot(xy[:, 0], xy[:, 1], color="0.7", linewidth=0.7)
        if cell not in products:
            continue
        product = products[cell]
        xy = product.parameter + cell[1:] - centre
        top.add_collection(
            PolyCollection(xy, facecolors="#157ba5", edgecolors="none", alpha=0.8)
        )
        if cell != detail:
            continue
        world = from_world(anchor, product.world.reshape(-1, 3)).reshape(-1, 3, 3)
        space.add_collection3d(
            Poly3DCollection(
                world, facecolors="#157ba5", edgecolors="0.25", linewidths=0.2
            )
        )
        all_world.append(world.reshape(-1, 3))
    if all_world:
        space.auto_scale_xyz(*np.concatenate(all_world).T)
    space.view_init(elev=45, azim=110)
    top.autoscale_view()
    top.set_aspect("equal")
    top.set_title("Actual source masks: blue diagnostic cuts; grey pending")
    top.set_xlabel("Mercator tiles relative to cohort centre")
    top.set_ylabel("Mercator tiles")
    space.set_xlabel("ENU east / m")
    space.set_ylabel("ENU north / m")
    space.set_zlabel("ENU up / m")
    space.set_title(f"Cell detail {detail}; vertical scale exaggerated")
    figure.suptitle(
        "Finite WATER3 snapshot; triangle soup, not a native asset or hydrology proof"
    )
    figure.tight_layout()
    figure.savefig(output / "masked-water.png", dpi=150)
    plt.close(figure)


def run(path, output):
    output.mkdir(parents=True, exist_ok=True)
    data, snapshots, epoch, receipts, order = acquire(path)
    parent, provenance = prepare_parent(data, snapshots, receipts)
    recipe = ReferenceUpdates(
        receipts,
        epoch,
        snapshots,
        data,
        order,
        parent=parent,
        parent_version=parent.version,
    )
    _, records, _, origin = inputs(path)
    context = f"water-param-cut-v1:{parent.version}:{origin['input_sha256']}"
    grouped = defaultdict(list)
    for record in records:
        grouped[record[0]].append(record)

    def facet(keys):
        nodes = [parent(key) for key in keys]
        if any(node.state != "ready" for node in nodes):
            return "pending_parent", None
        world = np.array([node.world for node in nodes])
        return recipe.admissible(keys, world), world

    products, refined, results = {}, {}, []
    for cell, selected in sorted(grouped.items()):
        polygons = [r[1].polygon for r in selected]
        keys = tuple((r[0], r[3], r[1].feature, r[1].part, r[2]) for r in selected)
        (product, report), cost = measured(
            lambda cell=cell, polygons=polygons, keys=keys: cut_cell(
                cell, polygons, keys, facet, context=context
            )
        )
        item = {
            "cell": cell,
            "cut": report,
            "cost": cost,
            "global_coordinates": global_refusals(cell, polygons),
        }
        if product is not None:
            products[cell] = product
            fine, _ = cut_cell(
                cell,
                polygons,
                keys,
                facet,
                context=context,
                refine_interior=True,
            )
            if fine is None:
                raise ValueError("interior refinement lost admitted parent")
            refined[cell] = fine
            reverse, _ = cut_cell(
                cell, polygons[::-1], keys[::-1], facet, context=context
            )
            np.testing.assert_array_equal(product.parameter, reverse.parameter)
            np.testing.assert_array_equal(product.world, reverse.world)
            if source_memberships(product) != source_memberships(reverse):
                raise ValueError("source order changed original source membership")
            item["welding_diagnostic"] = welding_diagnostic(product)
            save_product(output, product)
        results.append(item)
    report = {
        "source": origin,
        "parent_version": parent.version,
        "parent_provenance": provenance,
        "cells": results,
        "diagnostic_cells": len(products),
        "pending_cells": len(grouped) - len(products),
        "contacts": verify_contacts(products, refined),
        "facets": sum(len(p.world) for p in products.values()),
        "cut_numerical_array_bytes": sum(
            p.report["numerical_array_bytes"] for p in products.values()
        ),
        "root_adapter_work": recipe.work,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "resident finite source snapshot; no DB IO, welded mesh, native image, hydrology, global completeness or device claim",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    plot(products, records, output)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = run(args.input, args.output)
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "diagnostic_cells",
                    "pending_cells",
                    "facets",
                    "cut_numerical_array_bytes",
                    "contacts",
                )
            }
        )
    )

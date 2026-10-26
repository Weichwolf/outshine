"""Exhaustive finite-source pose bounds for demand-free shared phase catalogs."""

import argparse
import json
import resource
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_phase_catalog import CrownPhaseCatalog
from spatial_group_source import load_source


def local_pose(crown, node):
    if node not in crown._groups:
        crown._groups.clear()
    group = crown.group(node, 128)
    origin, frame = crown.stem(node)
    return {
        "origins": (group["origins"] - origin) @ frame,
        "frames": np.einsum("ij,njk->nik", frame.T, group["frames"]),
        "variants": group["variants"],
        "ids": group["ids"],
    }


def mesh_errors(actual, proxy, meshes):
    """Corresponding coarse vertices bound every point in matching triangles."""
    delta_frames = actual["frames"] - proxy["frames"]
    delta_origins = actual["origins"] - proxy["origins"]
    position, normal = 0.0, 0.0
    for variant in np.unique(actual["variants"]):
        mask = actual["variants"] == variant
        mesh = meshes[int(variant)]
        moved = np.einsum("vj,nkj->nvk", mesh["vertices"], delta_frames[mask])
        moved += delta_origins[mask, None]
        turned = np.einsum("vj,nkj->nvk", mesh["normals"], delta_frames[mask])
        position = max(position, float(np.linalg.norm(moved, axis=-1).max()))
        normal = max(normal, float(np.linalg.norm(turned, axis=-1).max()))
    return position, normal


def study(inputs, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    began = time.perf_counter()
    source = load_source(inputs)
    catalogs = []
    with patch(
        "outshine_ref.vegetation.Shoot.detail",
        side_effect=AssertionError("poses before catalog demand"),
    ):
        for bins in (8, 16, 32, 64):
            start = time.perf_counter()
            catalog = CrownPhaseCatalog(source, bins, max_templates=1024)
            catalogs.append(catalog)
            np.savez_compressed(
                output / f"catalog-{bins}.npz",
                mapping=catalog.mapping,
                representative_host_nodes=np.asarray(catalog.representatives, np.int32),
            )
            row = catalog.recipe() | {
                "summary": catalog.summary(),
                "plan_s": time.perf_counter() - start,
            }
            (output / f"catalog-{bins}.json").write_text(
                json.dumps(row, indent=2) + "\n"
            )
    assert not source.prototypes and not source.crown._groups
    planned = time.perf_counter()
    radii = np.array(
        [
            b.length + b.petiole + b.width / 2 * np.sqrt(1 + b.fold**2)
            for b in source.blades
        ]
    )
    meshes = [b.mesh(8, 1) for b in source.blades]
    results = [
        {
            "phase_bins": c.bins,
            "maximum_leaf_surface_position_bound_m": 0.0,
            "maximum_unit_normal_difference_bound": 0.0,
            "checked_leaf_rows": 0,
            "maximum_coarse_vertex_displacement_m": 0.0,
            "maximum_coarse_corner_normal_difference": 0.0,
        }
        for c in catalogs
    ]
    caches = [{} for _ in catalogs]
    for i, node in enumerate(map(int, source.crown.hosts)):
        actual = local_pose(source.crown, node)
        for c, cache, row in zip(catalogs, caches, results, strict=True):
            slot = int(c.mapping[i])
            representative = c.representatives[slot]
            if slot not in cache:
                cache[slot] = local_pose(source.crown, representative)
            proxy = cache[slot]
            np.testing.assert_array_equal(actual["variants"], proxy["variants"])
            # Spectral norm bounds every point within the declared blade root sphere.
            rotation = np.linalg.norm(
                actual["frames"] - proxy["frames"], ord=2, axis=(1, 2)
            )
            displacement = np.linalg.norm(actual["origins"] - proxy["origins"], axis=1)
            bound = displacement + radii[actual["variants"]] * rotation
            row["maximum_leaf_surface_position_bound_m"] = max(
                row["maximum_leaf_surface_position_bound_m"], float(bound.max())
            )
            row["maximum_unit_normal_difference_bound"] = max(
                row["maximum_unit_normal_difference_bound"], float(rotation.max())
            )
            distance, normal = mesh_errors(actual, proxy, meshes)
            assert distance <= float(bound.max()) + 1e-12
            if distance > row["maximum_coarse_vertex_displacement_m"]:
                row["maximum_coarse_vertex_displacement_m"] = distance
                row["worst_host_node"] = node
                row["worst_representative_node"] = representative
            row["maximum_coarse_corner_normal_difference"] = max(
                row["maximum_coarse_corner_normal_difference"], normal
            )
            row["checked_leaf_rows"] += len(bound)
        assert len(source.crown._groups) == 1
    result = {
        "source_identity": source.crown.identity,
        "catalog_identity": source.identity,
        "source_hosts": len(source.crown.hosts),
        "source_leaves": len(source.crown.variants),
        "plans": [c.summary() for c in catalogs],
        "finite_source_bounds": results,
        "all_catalog_planning_s": planned - began,
        "separate_exhaustive_pose_check_s": time.perf_counter() - planned,
        "reference_cache_array_payload_bytes": sum(
            a.nbytes for cache in caches for p in cache.values() for a in p.values()
        ),
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Planning generates no poses/meshes; separate verifier expands original host poses and eight coarse prototypes. Vertex maxima bound corresponding matching coarse triangle interiors only; sphere bounds cover declared blades. Array payload sums are not unique allocations or RSS. No screen coverage, lighting, arbitrary source or native admission.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("inputs", "output"):
        p.add_argument("--" + key, type=Path, required=True)
    a = p.parse_args()
    study(a.inputs, a.output)

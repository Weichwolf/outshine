"""Compare complete collar normal products and oriented multi-hit ray queries."""

import hashlib
import json
import subprocess
import time
from pathlib import Path

import manifold3d as manifold
import numpy as np
from branch_collar import collar_products
from collar_shading import blend_collar_normals, tube_reference
from plant_experiment import sapling
from plant_surface import axis_products
from spine_frames import unit
from surface_rays import SurfaceRays


def ray_checks():
    cube = manifold.Manifold.cube((1, 1, 1), center=True)
    separated = cube + cube.translate((2, 0, 0))
    points = np.random.default_rng(2353).uniform(-3, 3, (256, 3))
    points = np.vstack((points, [[0, 0, 0], [1, 0, 0], [4, 0, 0]]))
    normals = unit(np.random.default_rng(2354).normal(size=points.shape))
    normals[-3:] = (1, 0, 0)
    maximum, probes = 0.0, 0
    for scale, translation in ((1.0, 0.0), (1e-3, 1e3), (1e3, 1e6)):
        solid = (
            separated.scale((scale,) * 3).translate((translation,) * 3).as_original()
        )
        raw = solid.to_mesh64()
        rays = SurfaceRays(
            {
                "vertices": np.asarray(raw.vert_properties)[:, :3],
                "faces": np.asarray(raw.tri_verts),
            },
            solid,
        )
        query = points * scale + translation
        selected, positions, faces = rays.nearest(query, normals, 10 * scale)
        tri = rays.triangles[faces]
        plane = unit(rays.normals[faces])
        assert (
            np.max(abs(np.sum((positions - tri[:, 0]) * plane, axis=1))) < scale * 1e-7
        )
        actual = dict(zip(selected, positions, strict=True))
        for i, (p, n) in enumerate(zip(query, normals, strict=True)):
            hits = [
                h
                for h in solid.ray_cast(p + 10 * scale * n, p - 10 * scale * n)
                if np.dot(h.normal, n) > 0
            ]
            assert bool(hits) == (i in actual)
            if hits:
                expected = min(hits, key=lambda h: abs(h.distance - 0.5)).position
                error = np.linalg.norm(actual[i] - expected) / scale
                maximum = max(maximum, error)
                assert error < 1e-7
            probes += 1
    return {"oriented_ray_probes": probes, "maximum_relative_position_error": maximum}


def shade(plant, meshes, plans, references):
    indices = {path: i for i, path in enumerate(plant.paths())}
    products, counts = [], []
    for mesh, plan in zip(meshes, plans, strict=True):
        path = plan["path"]
        state = plant.axis(path)
        parent_base, parent_tip = plant.radii(path[:-1])
        radius = parent_base + state.parent_parameter * (parent_tip - parent_base)
        product, work = blend_collar_normals(
            mesh,
            [references[indices[path[:-1]]], references[indices[path]]],
            radius,
            plant.radii(path)[0],
        )
        products.append(product)
        counts.append(work)
    return products, counts


def compare(seed):
    plant = sapling(seed)
    axes = axis_products(plant, sides=16)
    meshes, plans = collar_products(plant)
    references, build_ms, times = {}, {}, {"manifold": [], "embree": []}
    for method in times:
        began = time.perf_counter()
        references[method] = [tube_reference(m, method) for m in axes]
        build_ms[method] = (time.perf_counter() - began) * 1000
    maximum = 0.0
    for repetition in range(3):
        products, counts = {}, {}
        order = (
            ("manifold", "embree") if repetition % 2 == 0 else ("embree", "manifold")
        )
        for method in order:
            began = time.perf_counter()
            products[method], counts[method] = shade(
                plant, meshes, plans, references[method]
            )
            times[method].append((time.perf_counter() - began) * 1000)
        assert counts["manifold"] == counts["embree"]
        for a, b in zip(products["manifold"], products["embree"], strict=True):
            for name in ("vertices", "faces"):
                np.testing.assert_array_equal(a[name], b[name])
            error = float(np.max(abs(a["normals"] - b["normals"])))
            maximum = max(maximum, error)
            assert error < 1e-5
    row = {
        "seed": seed,
        "reference_build_ms": build_ms,
        "samples_ms": times,
        "median_ms": {k: float(np.median(v)) for k, v in times.items()},
        "maximum_normal_component_error": maximum,
        "collars": len(meshes),
        "identical_geometry_and_reference_coverage": True,
        "fallback_queries_per_repetition": sum(
            r[0].fallback_queries for r in references["embree"]
        )
        // 3,
    }
    print(row, flush=True)
    return row


def main():
    checks = ray_checks()
    rows = [compare(seed) for seed in (17, 23, 41)]
    report = {
        "checks": checks,
        "cases": rows,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(["git", "status", "--porcelain"])
        ),
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("*.py")
        },
        "scope": "Three alternating local repetitions; reference construction once separately. Local float32 BVH queries, double triangle-plane hit refinement and scalar fallback for possible misses, edge and grazing hits; at most sixteen crossings per half-ray with 2e-6 reference-extent advance. Observed error/coverage on calibration plants, no universal ray precision or native device claim.",
    }
    Path("build/experiments/vegetation/collars/ray-cost.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )


if __name__ == "__main__":
    main()

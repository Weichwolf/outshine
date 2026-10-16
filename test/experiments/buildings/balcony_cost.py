"""Same actual deck/guard solid and visible role surfaces, three bounded constructions."""

import gc
import time
import tracemalloc

import manifold3d as manifold
import numpy as np
import shapely
from entrance_clearance import native
from outshine_ref.buildings.balconies import BalconyPlan, balcony_surface, volumes


def role_surfaces(product, role):
    triangles = product["vertices"][product["faces"]]
    groups, all_faces = {}, {}
    for t, material in zip(triangles, product["roles"], strict=True):
        n = np.cross(t[1] - t[0], t[2] - t[0])
        n /= np.linalg.norm(n)
        fixed = int(np.argmax(abs(n)))
        axes = [i for i in range(3) if i != fixed]
        key = fixed, round(float(n[fixed])), round(float(t[0, fixed]), 10)
        polygon = shapely.Polygon(t[:, axes])
        all_faces.setdefault(key, []).append(polygon)
        if material == role:
            groups.setdefault(key, []).append(polygon)
    result = {}
    for key, polygons in groups.items():
        opposite = key[0], -key[1], key[2]
        exterior = shapely.union_all(polygons).difference(
            shapely.union_all(all_faces.get(opposite, []))
        )
        if exterior.area > 1e-10:
            result[key] = exterior
    return result


def benchmark():
    results = []
    for width in (1.8, 2.8, 3.2):
        plan = BalconyPlan(width)
        oracle = balcony_surface(plan, method="union")
        expected = native(oracle)
        for method in ("boundary", "union", "parts"):
            product = balcony_surface(plan, method=method)
            if method == "parts":
                from outshine_ref.buildings.modelling.prism_union import primitive_mesh

                solids = []
                for source in volumes(plan):
                    vertices, faces = primitive_mesh(source)
                    solids.append(native({"vertices": vertices, "faces": faces}))
                actual = manifold.Manifold.batch_boolean(solids, manifold.OpType.Add)
            else:
                actual = native(product)
            for role in (0, 1):
                a, b = role_surfaces(oracle, role), role_surfaces(product, role)
                if a.keys() != b.keys() or any(
                    a[k].symmetric_difference(b[k]).area > 1e-8 for k in a
                ):
                    raise ValueError(
                        "Balcony methods changed visible material-role surfaces"
                    )
            difference = (actual - expected).volume() + (expected - actual).volume()
            if difference > 1e-9:
                raise ValueError("Balcony methods changed the solid")
            times = []
            for _ in range(21):
                started = time.perf_counter()
                product = balcony_surface(plan, method=method)
                times.append((time.perf_counter() - started) * 1000)
            gc.collect()
            tracemalloc.start()
            product = balcony_surface(plan, method=method)
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            results.append(
                {
                    "width_m": width,
                    "method": method,
                    "triangles": len(product["faces"]),
                    "array_bytes": sum(v.nbytes for v in product.values()),
                    "median_ms": float(np.median(times)),
                    "runs_ms": times,
                    "traced_peak_bytes": peak,
                    "solid_difference_m3": difference,
                    "visible_role_surfaces_match": True,
                    "work": "15 explicit exterior polygons, no CSG; 40 rendered triangles"
                    if method == "boundary"
                    else "4 prisms and 1 boolean union"
                    if method == "union"
                    else "4 separate closed prism meshes with internal contacts",
                    "known_expanded_render_position_bytes": len(product["faces"])
                    * 3
                    * 3
                    * 8,
                    "backend_allocations_copies": None,
                }
            )
    return results

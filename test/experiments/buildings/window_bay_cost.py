"""Equal physical prism/union controls; skin costs are separate, unequal render products."""

import gc
import time
import tracemalloc

import manifold3d as manifold
import numpy as np
from entrance_clearance import native
from outshine_ref.buildings.window_bays import (
    WindowBay,
    bay_flat,
    bay_skin,
    bay_surface,
    volumes,
)


def physical(product):
    if "parts" not in product:
        return native(product)
    solids = [
        native(
            {
                "vertices": product["vertices"],
                "faces": product["faces"][product["parts"] == part],
            }
        )
        for part in np.unique(product["parts"])
    ]
    result = manifold.Manifold.batch_boolean(solids, manifold.OpType.Add)
    if result.status() != manifold.Error.NoError:
        raise ValueError("Bay parts have no valid physical union")
    return result


def benchmark():
    results = []
    for fraction in (0.65, 1.0):
        for width in (1.8, 2.8, 3.2):
            plan = WindowBay(width, 2.9, front_fraction=fraction)
            oracle = physical(bay_surface(plan, method="parts"))
            expected = sum(v.polygon.area * (v.high_m - v.low_m) for v in volumes(plan))
            if abs(oracle.volume() - expected) > 1e-9:
                raise ValueError("Physical bay changed its analytic component volume")
            for method in ("parts", "union", "skin", "flat"):
                recipe = (
                    (lambda plan=plan: bay_skin(plan))
                    if method == "skin"
                    else (
                        lambda method=method, plan=plan: bay_surface(
                            plan, method=method
                        )
                    )
                )
                if method == "flat":
                    recipe = lambda plan=plan: bay_flat(plan, back_covered=True)
                row = {"front_fraction": fraction, "width_m": width, "method": method}
                try:
                    product = recipe()
                    difference = None
                    if method in ("parts", "union"):
                        actual = physical(product)
                        difference = (actual - oracle).volume() + (
                            oracle - actual
                        ).volume()
                        if abs(difference) > 1e-9:
                            raise ValueError("Bay candidate changed the physical solid")
                except ValueError as error:
                    results.append(row | {"status": "rejected", "reason": str(error)})
                    continue
                times = []
                for _ in range(21):
                    started = time.perf_counter()
                    product = recipe()
                    times.append((time.perf_counter() - started) * 1000)
                gc.collect()
                tracemalloc.start()
                product = recipe()
                _, peak = tracemalloc.get_traced_memory()
                tracemalloc.stop()
                results.append(
                    row
                    | {
                        "status": "pass",
                        "triangles": len(product["faces"]),
                        "array_bytes": sum(v.nbytes for v in product.values()),
                        "runs_ms": times,
                        "median_ms": float(np.median(times)),
                        "traced_peak_bytes": peak,
                        "solid_difference_m3": difference,
                        "known_position_result_arrays": 9 if method == "parts" else 1,
                        "known_position_materialized_bytes": product["vertices"].nbytes
                        * (2 if method == "parts" else 1),
                        "known_position_copy_bytes": 0
                        if method in ("skin", "flat")
                        else product["vertices"].nbytes,
                        "allocation_scope": "Parts:8 primitive position results and1 final concatenation. Skin:only final array; Python point lists/geometry intermediates not enumerated. Union:only final owned copy; backend and roundtrip allocations not enumerated.",
                        "scope": "Skin/flat are unequal external-only render products, never equal physical candidates. Flat requires opaque back cover and separate projection admission; no native/frame claim.",
                    }
                )
    return results

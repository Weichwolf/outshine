"""Equivalent closed headhouse assembly alternatives and measured fresh work."""

import gc
import time

import manifold3d as manifold
from allocation_measure import allocations
from outshine_ref.buildings.modelling import union_prisms
from structural_shell_geometry import native
from terraced_crown_cost import payload


def measured(call, repeats, trace):
    times = []
    for _ in range(repeats):
        gc.collect()
        cpu, wall = time.process_time(), time.perf_counter()
        value = call()
        times.append(
            {"cpu_s": time.process_time() - cpu, "wall_s": time.perf_counter() - wall}
        )
        del value
    gc.collect()
    result, memory = allocations(trace, call)
    return result, {"runs": times, "allocations": memory, "held": payload(result)}


def headhouse(plan, output, repeats):
    sources = tuple(p.volume for p in plan.crown.parts if p.role.startswith("access-"))
    calls = {
        "separate": lambda: tuple(union_prisms((p,)) for p in sources),
        "union": lambda: (union_prisms(sources),),
    }
    products, costs = {}, {}
    for name, call in calls.items():
        products[name], costs[name] = measured(
            call, repeats, output / (name + "-headhouse.bin")
        )
        costs[name]["triangles"] = sum(len(p.mesh["faces"]) for p in products[name])
        costs[name]["components"] = len(products[name])
    separate = manifold.Manifold.batch_boolean(
        [native(p.mesh, p.vertex_roots) for p in products["separate"]],
        manifold.OpType.Add,
    )
    united = native(products["union"][0].mesh, products["union"][0].vertex_roots)
    if separate.status() != manifold.Error.NoError:
        raise ValueError("Independent headhouse assembly union failed")
    difference = (separate - united).volume() + (united - separate).volume()
    expected = sum(p.polygon.area * (p.high_m - p.low_m) for p in sources)
    if (
        max(
            abs(difference),
            abs(expected - separate.volume()),
            abs(expected - united.volume()),
        )
        > 1e-7
    ):
        raise ValueError("Equivalent headhouse alternatives disagree on actual volume")
    return {
        "variants": costs,
        "source_prisms": len(sources),
        "independent_volume_m3": expected,
        "symmetric_volume_difference_m3": difference,
        "source_materials": sorted({p.material for p in sources}),
        "scope": "Same three source/material prisms; shared interfaces are internal. Compare separate closed products with their Boolean union, not source-height or clearance alternatives.",
    }

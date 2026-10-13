"""Compare texel integrals and conservative support culling on cached leaf nets."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from blade_surface import Blade
from leaf_texture_filter import filtered_veins, physical_points


def culling_checks():
    rng = np.random.default_rng(839)
    cases = 0
    for blade in (Blade(lobes=0), Blade(), Blade(lobes=32, widest=0.1, lobe_depth=0.9)):
        a = physical_points(blade, rng.uniform(0, 1, (24, 2)))
        b = physical_points(blade, rng.uniform(0, 1, (24, 2)))
        radii = rng.uniform(0.00001, 0.0003, 24)
        full, _ = filtered_veins(blade, a, b, radii, size=64, order=6, cull=False)
        sparse, _ = filtered_veins(blade, a, b, radii, size=64, order=6)
        assert np.array_equal(full, sparse)
        cases += 1
    empty = np.empty((0, 2))
    zero, _ = filtered_veins(Blade(), empty, empty, np.empty(0), size=32)
    assert np.array_equal(zero, np.zeros(1024))
    for radius in (0, -1e-9, np.nan):
        try:
            filtered_veins(Blade(), a[:1], b[:1], [radius], size=32)
        except ValueError:
            pass
        else:
            raise AssertionError("support inflation masked an invalid radius")
    return {"full_maps": cases, "empty_graph": True, "invalid_radius": True}


def midpoint_reference(blade, a, b, radius, texels, size=512, subdivisions=64):
    offsets = (np.arange(subdivisions) + 0.5) / subdivisions - 0.5
    u, v = np.meshgrid(offsets, offsets)
    offsets = np.column_stack((u.ravel(), v.ravel())) / size
    tangent = b - a
    squared = np.sum(tangent**2, axis=1)
    result = []
    for texel in texels:
        row, column = divmod(int(texel), size)
        uv = np.array([column + 0.5, row + 0.5]) / size + offsets
        points = physical_points(blade, uv)
        delta = points[:, None] - a
        t = np.clip(np.sum(delta * tangent, axis=2) / squared, 0, 1)
        distance2 = np.sum((delta - t[..., None] * tangent) ** 2, axis=2)
        height = np.maximum(1 - distance2 / (9 * radius**2), 0) ** 3
        result.append(height.max(axis=1, initial=0).mean())
    return np.array(result)


def check_case(folder, index):
    paths = [
        folder / f"leaf-{index}-{suffix}" for suffix in ("material.json", "network.npz")
    ]
    material = json.loads(paths[0].read_text())
    blade = Blade(**material["blade"])
    with np.load(paths[1]) as graph:
        edges = graph["raw_edges"][int(graph["base_edges"]) :]
        a, b = (graph["raw_positions"][edges[:, i]] for i in (0, 1))
    radius = material["minor_radius_m"]
    radii = np.full(len(a), radius)
    times = {"all_texels": [], "support_only": []}
    for repeat in range(3):
        order = (False, True) if repeat % 2 == 0 else (True, False)
        values = {}
        for cull in order:
            start = time.perf_counter()
            values[cull], _work = filtered_veins(blade, a, b, radii, order=6, cull=cull)
            times["support_only" if cull else "all_texels"].append(
                time.perf_counter() - start
            )
        assert np.array_equal(values[False], values[True])
    point, _ = filtered_veins(blade, a, b, radii, order=1)
    active = np.flatnonzero(values[True] > 0)
    probes = active[np.linspace(0, len(active) - 1, min(1024, len(active)), dtype=int)]
    reference, _ = filtered_veins(
        blade, a, b, radii, order=16, texels=probes, cull=False
    )
    errors = {}
    for name, value in (("point", point), ("integrated", values[True])):
        difference = value[probes] - reference
        errors[name] = {
            "max": float(np.max(abs(difference))),
            "rms": float(np.sqrt(np.mean(difference**2))),
        }
    assert errors["integrated"]["rms"] < 0.001
    worst = np.argsort(abs(values[True][probes] - reference))[-12:]
    independent = midpoint_reference(blade, a, b, radius, probes[worst])
    reference_error = float(np.max(abs(independent - reference[worst])))
    assert reference_error < 0.002
    return {
        "seconds": times,
        "identical_culled_integrals": True,
        "probe_texels": len(probes),
        "errors": errors,
        "independent_midpoint_max_error": reference_error,
        "midpoint_probes": len(worst),
    }, paths


def main():
    folder = Path("build/experiments/vegetation/closed-leaves")
    cases, paths = [], []
    for index in range(3):
        case, inputs = check_case(folder, index)
        cases.append(case)
        paths.extend(inputs)
        print(index, case, flush=True)
    names = (
        "leaf_filter_experiment.py",
        "leaf_texture_filter.py",
        "leaf_domain.py",
        "leaf_vein_field.py",
        "blade_surface.py",
    )
    report = {
        "cases": cases,
        "checks": culling_checks(),
        "order": 6,
        "size": 512,
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "implementation_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in names
        },
        "process_peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
    }
    (folder / "filtering.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()

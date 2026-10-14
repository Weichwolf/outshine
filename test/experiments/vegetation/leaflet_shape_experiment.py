"""Local leaflet profiles/variation, same-input prototype comparison and 3D controls."""

import hashlib
import importlib.util
import json
import resource
import shutil
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from organ_experiment import UPRIGHT, write_product
from outshine_ref.vegetation import FrondGuide, LeafletShape, PinnatePattern, Profile
from outshine_ref.vegetation.axis_surface import axis_mesh

ROOT = Path("build/experiments/vegetation/leaflet-shape")


def patterns():
    fixed = LeafletShape(
        Profile(np.full(4, 0.4)), Profile(np.full(4, 0.2)), Profile(np.full(4, 0.35))
    )
    variable = LeafletShape(
        Profile(np.linspace(-0.3, 0.65, 4)),
        Profile(np.linspace(0.04, 0.2, 4)),
        Profile(np.linspace(-0.3, 0.4, 4)),
        (0.10, 0.03, 0.11),
    )
    return (
        PinnatePattern(),
        PinnatePattern(leaflet_shape=fixed),
        PinnatePattern(leaflet_shape=variable),
    )


def legacy_check():
    path = ROOT / "before/__init__.py"
    name = "leaflet_before"
    spec = importlib.util.spec_from_file_location(
        name, path, submodule_search_locations=[str(path.parent)]
    )
    before = importlib.util.module_from_spec(spec)
    sys.modules[name] = before
    spec.loader.exec_module(before)
    count = 0
    for seed in (17, 0, 2**64 - 1):
        old, new = before.PinnatePattern(seed=seed), PinnatePattern(seed=seed)
        assert old.recipe() == new.recipe() and old.identity == new.identity
        for n, across in ((12, 1), (48, 4)):
            a, b = old.mesh(n, across), new.mesh(n, across)
            for key, value in a.items():
                np.testing.assert_array_equal(value, b[key])
            count += 1
    return count


def compare(plan, name):
    samples = {"shared": [], "individual": []}
    expected = None
    for repeat in range(5):
        for method in (
            ("shared", "individual") if repeat % 2 == 0 else ("individual", "shared")
        ):
            began = time.perf_counter()
            mesh = plan.mesh(48, 4, method=method)
            samples[method].append(time.perf_counter() - began)
            if expected is None:
                expected = mesh
            else:
                for key, value in expected.items():
                    np.testing.assert_array_equal(value, mesh[key])
    np.savez_compressed(ROOT / f"{name}.npz", **expected)
    with np.load(ROOT / f"{name}.npz") as saved:
        for key, value in expected.items():
            np.testing.assert_array_equal(value, saved[key])
    loaded = PinnatePattern.from_recipe(json.loads(json.dumps(plan.recipe())))
    restored = loaded.mesh(48, 4)
    for key, value in expected.items():
        np.testing.assert_array_equal(value, restored[key])
    return {
        "name": name,
        "identity": plan.identity,
        "samples_s": samples,
        "faces": len(expected["faces"]),
        "original_and_reload_arrays_exact": True,
        "shape_variants": len({o.get("shape") for o in plan.organs()}),
    }


def source_products(plan, index, detail):
    near = detail == "near"
    n, across = (48, 4) if near else (12, 1)
    guide = FrondGuide(bend=-0.35, fold_per_m=0.10)
    source = plan.mesh(n, across)
    mesh = guide.deform(source)
    leaf = write_product(
        ROOT,
        f"section-{index}-{detail}",
        mesh,
        np.array([[(index - 1) * 0.75, 0, 0]]),
        UPRIGHT[None],
        "leaf",
    )
    rib = guide.deform(
        axis_mesh(
            guide.length,
            0,
            0.004,
            0.0007,
            96 if near else 24,
            8 if near else 6,
            bark=True,
        )
    )
    wood = write_product(
        ROOT,
        f"section-rib-{index}-{detail}",
        rib,
        np.array([[(index - 1) * 0.75, 0, 0]]),
        UPRIGHT[None],
        "wood",
    )
    wood["bark_material"] = "bark-material.json"
    return [leaf, wood]


def scenes(plans):
    shutil.copy2(
        "build/experiments/vegetation/placements/phyllotaxy/bark-material.json",
        ROOT / "bark-material.json",
    )
    near = [p for i, plan in enumerate(plans) for p in source_products(plan, i, "near")]
    coarse = source_products(plans[2], 2, "coarse")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    identity = hashlib.sha256(
        json.dumps([p.recipe() for p in plans], sort_keys=True).encode()
    ).hexdigest()
    for name, data, camera, target, fov in (
        ("leaflet-shapes", near, (2.1, -4.2, 2.2), (0, 0, 0.6), 35),
        ("leaflet-detail", near[4:6], (1.8, -1.4, 1.45), (0.75, 0.03, 0.65), 32),
        ("leaflet-coarse", coarse, (1.8, -1.4, 1.45), (0.75, 0.03, 0.65), 32),
    ):
        r = {
            "name": name,
            "products": data,
            "camera": camera,
            "target": target,
            "fov_y": fov,
            "ground_size_m": 8,
            "resolution": [2560, 1440],
            "samples": 32,
            "source_commit": commit,
            "source_modified": True,
            "plan_identity": identity,
        }
        (ROOT / f"{name}.json").write_text(json.dumps(r, indent=2) + "\n")


def main():
    plans = patterns()
    legacy = legacy_check()
    cases = [
        compare(p, name)
        for p, name in zip(plans, ("flat", "fixed", "profiled"), strict=True)
    ]
    scenes(plans)
    files = [
        Path(__file__),
        *[
            Path("lib/src/outshine_ref/vegetation") / f"{name}.py"
            for name in ("leaflet_shape", "frond_pattern")
        ],
    ]
    report = {
        "cases": cases,
        "old_meshes_exact": legacy,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "process_peak_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "botanical_original_or_device_equivalence_proven": False,
    }
    (ROOT / "leaflet-shape-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

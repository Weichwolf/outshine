"""Same-input leaf/grass library comparison and a rejected-twist render case."""

import hashlib
import importlib.util
import json
import resource
import subprocess
import sys
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np
from blade_surface import Blade
from botanical_profiles import PROFILES
from organ_experiment import GRASS, UPRIGHT, check_blades, write_product


def load_before(path):
    spec = importlib.util.spec_from_file_location("blade_before", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def compare(root, name, blade, before_type):
    before = before_type(**asdict(blade))
    samples = {"before": [], "after": []}
    for repeat in range(5):
        for key in ("before", "after") if repeat % 2 == 0 else ("after", "before"):
            started = time.perf_counter()
            mesh = (before if key == "before" else blade).mesh(64, 4)
            samples[key].append(time.perf_counter() - started)
            with np.load(root / f"{name}-near-before.npz") as saved:
                assert all(np.array_equal(mesh[k], saved[k]) for k in mesh)
    coarse = blade.mesh(16, 2)
    with np.load(root / f"{name}-coarse-before.npz") as saved:
        assert all(np.array_equal(coarse[k], saved[k]) for k in coarse)
    area_before, area_after = before.area(256), blade.area(256)
    assert area_before == area_after
    return {
        "name": name,
        "mesh_seconds": samples,
        "mesh_arrays_exact": True,
        "area_m2": area_after,
        "area_exact": True,
    }


def twist_scene(root, before_type):
    shape = Blade(twist=100, lobes=0, bend=0)
    invalid = before_type(**asdict(shape)).mesh(16, 4)
    valid = shape.mesh(512, 8)
    points = invalid["vertices"][invalid["faces"]]
    cross = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    bad = int(
        np.count_nonzero(
            np.sum(cross[:, None] * invalid["normals"].reshape(-1, 3, 3), axis=2) <= 0
        )
    )
    assert bad > 0
    try:
        shape.mesh(16, 4)
    except ValueError:
        pass
    else:
        raise AssertionError("underresolved twist accepted")
    products = []
    for name, mesh, x in (("underresolved", invalid, -0.06), ("resolved", valid, 0.06)):
        products.append(
            write_product(
                root, name, mesh, np.array([[x, 0, 0]]), UPRIGHT[None], "leaf"
            )
        )
    record = {
        "name": "blade-twist",
        "products": products,
        "camera": [0.25, -0.45, 0.22],
        "target": [0, 0, 0.06],
        "fov_y": 29,
        "ground_size_m": 2,
        "resolution": [2560, 1440],
        "samples": 32,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    "test/experiments/vegetation",
                    "lib/src/outshine_ref/vegetation",
                ],
                text=True,
            ).strip()
        ),
    }
    (root / "blade-twist.json").write_text(json.dumps(record, indent=2) + "\n")
    return {
        "coarse_bad_corner_orientations": bad,
        "rejected": True,
        "accepted_triangles": len(valid["faces"]),
        "coarse_triangles": len(invalid["faces"]),
    }


def main():
    root = Path("build/experiments/vegetation/blade-library")
    before = load_before(root / "blade_surface.before.py")
    cases = {
        "default": Blade(),
        "grass": GRASS,
        **{name: Blade(**data["blade"]) for name, data in PROFILES.items()},
    }
    report = {
        "cases": [
            compare(root, name, blade, before.Blade) for name, blade in cases.items()
        ],
        "independent_organ_cases": check_blades(),
        "twist": twist_scene(root, before.Blade),
        "peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "implementation_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__),
                Path("lib/src/outshine_ref/vegetation/blade_surface.py"),
                Path("lib/src/outshine_ref/vegetation/curve_frame.py"),
            )
        },
    }
    (root / "blades.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

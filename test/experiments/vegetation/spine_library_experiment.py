"""Same-input spatial curve/sweep migration, ownership and underresolved-roll control."""

import hashlib
import importlib.util
import json
import resource
import subprocess
import sys
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from organ_experiment import UPRIGHT, write_product
from outshine_ref.vegetation import CubicSpine, Profile, SpatialAxis
from spatial_examples import spatial_axis
from spine_checks import frame_checks, native_frame_check
from spine_experiment import fixtures
from spine_surface import spine_mesh

ROOT = Path("build/experiments/vegetation/spine-library")


def load_before():
    path = ROOT / "before/__init__.py"
    spec = importlib.util.spec_from_file_location(
        "spine_before", path, submodule_search_locations=[str(path.parent)]
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def factory(template, provider):
    return provider.SpatialAxis(
        provider.CubicSpine(template.spine.controls),
        template.radius,
        template.roll,
        template.ellipse,
        template.intervals_per_segment,
    )


def compare(kind, before, after):
    template = spatial_axis(kind)
    axes = {
        name: factory(template, provider)
        for name, provider in (("before", before), ("after", after))
    }
    u = np.linspace(0, 1, 513)
    for axis in axes.values():
        axis.field.sample(u)
    samples = {
        phase: {name: [] for name in axes}
        for phase in ("cold_field", "warm_field", "cold_mesh")
    }
    for repeat in range(5):
        for name in ("before", "after") if repeat % 2 == 0 else ("after", "before"):
            provider = before if name == "before" else after
            for phase in samples:
                started = time.perf_counter()
                axis = (
                    axes[name] if phase == "warm_field" else factory(template, provider)
                )
                if phase == "cold_mesh":
                    output = axis.mesh(48, 12)
                    saved_path = ROOT / f"axis-{kind}-near-before.npz"
                else:
                    output = axis.field.sample(u)
                    saved_path = ROOT / f"field-{kind}-before.npz"
                samples[phase][name].append(time.perf_counter() - started)
                with np.load(saved_path) as saved:
                    for key in output:
                        np.testing.assert_array_equal(output[key], saved[key])
    return {"name": kind, "samples_s": samples, "outputs_exact": True}


def mesh_regression():
    count = 0
    for kind in ("straight", "inflection", "space", "roll"):
        mesh = spatial_axis(kind).mesh(12, 6)
        with np.load(ROOT / f"axis-{kind}-coarse-before.npz") as saved:
            for key in mesh:
                np.testing.assert_array_equal(mesh[key], saved[key])
        count += 1
    for kind, (spine, roll) in fixtures().items():
        for n, sides in ((16, 8), (64, 16), (128, 32)):
            mesh = spine_mesh(
                spine,
                Profile(np.linspace(1, 0.2, 4)),
                0.013,
                n,
                sides,
                ellipse=(1, 0.6),
                roll=roll,
            )
            with np.load(ROOT / f"spine-{kind}-{n}-{sides}-before.npz") as saved:
                for key in mesh:
                    np.testing.assert_array_equal(mesh[key], saved[key])
            count += 1
    return {"mesh_arrays_exact": count + 4, "field_arrays_exact": 4}


def roll_scene(before):
    positions, derivatives = [[0, 0, 0], [0, 1, 0]], [[0, 1, 0], [0, 1, 0]]
    axes = [
        provider.SpatialAxis(
            provider.CubicSpine.from_hermite(positions, derivatives),
            Profile(np.full(4, 0.04)),
            Profile(np.linspace(0, 100, 4)),
            (1, 0.3),
        )
        for provider in (
            before,
            SimpleNamespace(SpatialAxis=SpatialAxis, CubicSpine=CubicSpine),
        )
    ]
    coarse = axes[0].mesh(4, 8)
    unresolved = axes[0].mesh(64, 8)
    points = unresolved["vertices"][unresolved["faces"]]
    cross = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    bad = int(
        np.count_nonzero(
            np.sum(cross[:, None] * unresolved["normals"].reshape(-1, 3, 3), axis=2)
            <= 0
        )
    )
    fine = axes[1].mesh(512, 16)
    for segments in (4, 64):
        try:
            axes[1].mesh(segments, 8)
        except ValueError:
            pass
        else:
            raise AssertionError("underresolved roll accepted")
    products = []
    for label, mesh, x in (
        ("roll-alias-control", coarse, -0.10),
        ("roll-resolved", fine, 0.10),
    ):
        product = write_product(
            ROOT, label, mesh, np.array([[x, 0, 0]]), UPRIGHT[None], "wood"
        )
        product["bark_material"] = "bark-material.json"
        products.append(product)
    material = Path("build/experiments/vegetation/spatial-graph/bark-material.json")
    if not material.exists():
        material = Path(
            "build/experiments/vegetation/spatial-graph/space/bark-material.json"
        )
    if not material.exists():
        material = Path(
            "build/experiments/vegetation/placements/phyllotaxy/bark-material.json"
        )
    (ROOT / "bark-material.json").write_bytes(material.read_bytes())
    scene = {
        "name": "branch-roll",
        "products": products,
        "camera": [0.55, -2.9, 1.25],
        "target": [0, 0, 0.50],
        "fov_y": 26,
        "ground_size_m": 8,
        "resolution": [2560, 1440],
        "samples": 32,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "plan_identity": None,
    }
    (ROOT / "branch-roll.json").write_text(json.dumps(scene, indent=2) + "\n")
    return {
        "total_roll_rad": 100,
        "alias_control_triangles": len(coarse["faces"]),
        "unresolved_64_bad_corners": bad,
        "resolved_triangles": len(fine["faces"]),
    }


def main():
    before = load_before()
    after = SimpleNamespace(SpatialAxis=SpatialAxis, CubicSpine=CubicSpine)
    cases = [
        compare(kind, before, after)
        for kind in ("straight", "inflection", "space", "roll")
    ]
    checks = (
        frame_checks()
        | mesh_regression()
        | {
            "native_frame": native_frame_check(
                Path(
                    "/home/cosmo/Downloads/outshine-speedtree-analysis/frame_oracle.py"
                )
            )
        }
    )
    paths = [
        Path(__file__),
        *Path("lib/src/outshine_ref/vegetation").glob("spine_*.py"),
        Path("lib/src/outshine_ref/vegetation/spatial_axis.py"),
        Path("lib/src/outshine_ref/vegetation/tube_topology.py"),
        Path("lib/src/outshine_ref/vegetation/prepared_polynomial.py"),
        Path("lib/src/outshine_ref/vegetation/quadrature.py"),
    ]
    report = {
        "cases": cases,
        "checks": checks,
        "roll_counterexample": roll_scene(before),
        "process_peak_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "commit": subprocess.check_output(
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
        "native_full_spine_or_device_proven": False,
    }
    (ROOT / "spine-library.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

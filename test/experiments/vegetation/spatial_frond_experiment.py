"""Free-spine frond derivatives, complete equal-output costs and saved product replay."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from frond_checks import guide_cases
from generator_graph import shape_bound
from generator_specs import FrondShape, Generator
from graph_frames import parent_frame
from graph_sources import source_parts
from outshine_ref.vegetation import PinnatePattern, SpatialFrondGuide
from spatial_frond_scenes import ROOT, guides, pattern, scenes


def unchanged():
    count = 0
    for i, g in enumerate(guide_cases()):
        for n, across in ((12, 1), (24, 2), (48, 4)):
            actual = g.deform(PinnatePattern().mesh(n, across))
            with np.load(ROOT / "legacy" / f"{i}-{n}.npz") as saved:
                for key, value in actual.items():
                    np.testing.assert_array_equal(value, saved[key])
            count += 1
    return count


def checks(guide):
    shape = FrondShape(pattern(guide), guide)
    node = Generator(1, "spatial frond", "frond", (0,), shape)
    restored = Generator.from_recipe(json.loads(json.dumps(node.recipe())))
    assert restored.recipe() == node.recipe()
    for detail in ("near", "coarse"):
        for mesh in source_parts(shape, detail).values():
            assert np.linalg.norm(mesh["vertices"], axis=1).max() <= shape_bound(shape)
    pose = SimpleNamespace(scale=1.7, length=guide.length * 1.7, bend=0)
    sample = guide.field.sample(0.37)
    actual = parent_frame(node, pose, 0.37)
    for a, key in zip(actual, ("centre", "tangent", "lateral", "normal"), strict=True):
        np.testing.assert_array_equal(a, sample[key] * (1.7 if key == "centre" else 1))
    rng = np.random.default_rng(41)
    points = rng.uniform(
        (-0.06, 0.02, -0.02), (0.06, guide.length - 0.02, 0.02), (97, 3)
    )
    _, jacobian = guide.evaluate(points)
    h = 1e-7
    numerical = np.stack(
        [
            (guide.evaluate(points + h * row)[0] - guide.evaluate(points - h * row)[0])
            / (2 * h)
            for row in np.eye(3)
        ],
        axis=-1,
    )
    error = float(abs(numerical - jacobian).max())
    assert error < 1e-8
    return error


def compare(guide, index):
    source = pattern(guide).mesh(24, 2)
    samples = {"batch": [], "scalar": []}
    expected = None
    for repeat in range(5):
        for method in ("batch", "scalar") if repeat % 2 == 0 else ("scalar", "batch"):
            began = time.perf_counter()
            actual = guide.deform(source, method)
            samples[method].append(time.perf_counter() - began)
            if expected is None:
                expected = actual
            else:
                for key, value in expected.items():
                    np.testing.assert_array_equal(value, actual[key])
    flat = source["faces"].ravel()
    j = guide.evaluate(source["vertices"])[1][flat]
    normals = np.linalg.solve(j.transpose(0, 2, 1), source["normals"][..., None])[
        ..., 0
    ]
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    normal_error = float(abs(normals - expected["normals"]).max())
    assert normal_error < 1e-14
    np.savez_compressed(ROOT / f"mesh-{index}.npz", **expected)
    loaded = SpatialFrondGuide.from_recipe(json.loads(json.dumps(guide.recipe())))
    reformed = loaded.deform(source)
    with np.load(ROOT / f"mesh-{index}.npz") as saved:
        for key, value in expected.items():
            np.testing.assert_array_equal(value, saved[key])
            np.testing.assert_array_equal(value, reformed[key])
    return {
        "index": index,
        "samples_s": samples,
        "faces": len(source["faces"]),
        "jacobian_error": checks(guide),
        "normal_error": normal_error,
        "arrays_reload_exact": True,
        "guide_identity": guide.identity,
    }


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    report = {
        "legacy_circle_meshes_exact": unchanged(),
        "cases": [compare(g, i) for i, g in enumerate(guides())],
    }
    scenes()
    paths = [Path(__file__), Path(__file__).with_name("spatial_frond_scenes.py")]
    paths += [
        Path("lib/src/outshine_ref/vegetation") / name
        for name in (
            "spatial_frond.py",
            "frond_deform.py",
            "frond_surface.py",
            "spine_measure.py",
            "spine_field.py",
        )
    ]
    report |= {
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "process_peak_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "botanical_original_or_device_equivalence_proven": False,
    }
    (ROOT / "spatial-frond-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

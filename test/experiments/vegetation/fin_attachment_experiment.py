"""Own procedural fin inputs, matched full normal methods and cached render products."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from organ_experiment import UPRIGHT, write_product
from outshine_ref.vegetation import (
    Blade,
    CubicSpine,
    FinAttachment,
    Profile,
    SpatialAxis,
)

ROOT = Path("build/experiments/vegetation/fins")


def axis():
    return SpatialAxis(
        CubicSpine.from_hermite(
            [[0, 0, 0], [0, 1.6, 0.1]], [[0, 1.6, 0], [0, 1.6, 0.2]]
        ),
        Profile(np.linspace(0.11, 0.08, 4)),
        ellipse=(1.1, 0.9),
    )


def source_mesh(bend, segments=96, across=6):
    return Blade(
        width=0.08,
        length=0.25,
        base_fill=1,
        fold=0,
        lobes=0,
        petiole=0,
        bend=bend,
        twist=0,
    ).mesh(segments, across)


def same_product(left, right):
    assert left.keys() == right.keys()
    error = 0.0
    for name in left:
        if name == "normals":
            error = max(error, float(np.max(np.abs(left[name] - right[name]))))
            np.testing.assert_allclose(left[name], right[name], atol=7e-16, rtol=0)
        else:
            np.testing.assert_array_equal(left[name], right[name])
    return error


def benchmark(parent):
    cases = []
    for style, angle, bend in (
        ("protrusion", 0, 0.8),
        ("peel-up", np.pi / 2, -0.8),
        ("peel-down", -np.pi / 2, 0.8),
    ):
        fin = FinAttachment(parent, 0.52, 0.7, angle)
        source = source_mesh(bend, 128, 8)
        fin.positions(source["vertices"])
        saved = {k: v.copy() for k, v in source.items()}
        samples = {"cofactor": [], "solve": []}
        error = 0.0
        for repeat in range(5):
            products = {}
            for method in (
                ("cofactor", "solve") if repeat % 2 == 0 else ("solve", "cofactor")
            ):
                began = time.perf_counter()
                products[method] = fin.deform(source, method=method)
                samples[method].append(time.perf_counter() - began)
            error = max(error, same_product(products["cofactor"], products["solve"]))
        for key, value in saved.items():
            np.testing.assert_array_equal(value, source[key])
        path = ROOT / f"benchmark-{style}.npz"
        np.savez_compressed(path, **products["cofactor"])
        with np.load(path) as loaded:
            for key, value in products["cofactor"].items():
                np.testing.assert_array_equal(value, loaded[key])
        cases.append(
            {
                "style": style,
                "vertices": len(source["vertices"]),
                "faces": len(source["faces"]),
                "samples_s": samples,
                "maximum_normal_error": error,
                "other_arrays_exact": True,
                "reload_arrays_exact": True,
                "source_preserved": True,
                "prepared_parent_and_attachment": True,
            }
        )
    return cases


def anchor_check(source, transformed, fin):
    mask = (source["vertices"][:, 1:] == 0).all(axis=1)
    points = source["vertices"][mask]
    expected = fin.positions(points)
    np.testing.assert_array_equal(transformed["vertices"][mask], expected)
    sample = fin.parent.field.sample(fin.fraction)
    delta = expected - sample["centre"]
    q = delta @ sample["lateral"] / fin.parent.ellipse[0]
    d = delta @ sample["normal"] / fin.parent.ellipse[1]
    radius = fin.parent.radius.evaluate(fin.fraction)
    error = float(np.max(np.abs(np.hypot(q, d) - radius)))
    assert error < 2e-15
    return {"anchors": int(mask.sum()), "ring_residual_m": error}


def products(parent, detail):
    n, sides, segments, across = (
        (96, 32, 96, 6) if detail == "near" else (24, 12, 24, 2)
    )
    entries = []
    attachments = []
    checks = []
    for style, fraction, azimuth, angle, bend in (
        ("up", 0.52, 0.8, np.pi / 2, -0.8),
        ("down", 0.75, 0.6, -np.pi / 2, 0.8),
        ("protrusion", 0.28, 1.1, 0, 0.8),
    ):
        fin = FinAttachment(parent, fraction, azimuth, angle)
        source = source_mesh(bend, segments, across)
        mesh = fin.deform(source)
        checks.append({"style": style} | anchor_check(source, mesh, fin))
        attachments.append((fin, source, mesh))
        entries.append(
            write_product(
                ROOT,
                f"fin-{style}-{detail}",
                mesh,
                np.zeros((1, 3)),
                UPRIGHT[None],
                "wood",
            )
        )
    angles = list(np.arange(sides) * 2 * np.pi / sides)
    fractions = list(np.linspace(0, 1, n + 1))
    for fin, source, _ in attachments:
        mask = (source["vertices"][:, 1:] == 0).all(axis=1)
        fractions.append(fin.fraction)
        angles.extend(fin.azimuth + source["vertices"][mask, 0] / fin._slice[2])
    fractions, angles = np.unique(fractions), np.unique(angles)
    parent_mesh = parent.mesh_at(fractions, angles)
    snapped = 0
    for fin, source, mesh in attachments:
        mask = (source["vertices"][:, 1:] == 0).all(axis=1)
        a = fin.azimuth + source["vertices"][mask, 0] / fin._slice[2]
        rows = np.searchsorted(fractions, fin.fraction)
        indices = rows * len(angles) + np.searchsorted(angles, a)
        exact = parent_mesh["vertices"][indices]
        np.testing.assert_allclose(mesh["vertices"][mask], exact, atol=2e-15, rtol=0)
        mesh["vertices"][mask] = exact
        snapped += int(mask.sum())
    for entry, (_, _, mesh) in zip(entries, attachments, strict=True):
        np.savez_compressed(ROOT / entry["mesh"], **mesh)
    entries.insert(
        0,
        write_product(
            ROOT,
            f"fin-parent-{detail}",
            parent_mesh,
            np.zeros((1, 3)),
            UPRIGHT[None],
            "wood",
        ),
    )
    checks.append(
        {
            "shared_parent_positions_exact": snapped,
            "parent_stations": len(fractions),
            "parent_angles": len(angles),
        }
    )
    for p in entries:
        p["bark_material"] = "bark-material.json"
    return entries, checks


def scenes(parent):
    material = Path(
        "build/experiments/vegetation/placements/phyllotaxy/bark-material.json"
    )
    (ROOT / "bark-material.json").write_bytes(material.read_bytes())
    near, near_checks = products(parent, "near")
    coarse, coarse_checks = products(parent, "coarse")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    for name, data, camera, target, fov in (
        ("fin-stem", near, (1.5, -3, 2), (0, 0, 0.8), 30),
        ("fin-stem-coarse", coarse, (1.5, -3, 2), (0, 0, 0.8), 30),
        ("fin-peel-detail", near, (0.50, -0.83, 1.20), (0.08, -0.10, 0.95), 31),
    ):
        record = {
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
            "plan_identity": hashlib.sha256(
                json.dumps(parent.recipe(), sort_keys=True).encode()
            ).hexdigest(),
        }
        (ROOT / f"{name}.json").write_text(json.dumps(record, indent=2) + "\n")
    return {"near": near_checks, "coarse": coarse_checks}


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    parent = axis()
    cases = benchmark(parent)
    checks = scenes(parent)
    paths = [
        Path(__file__),
        Path("lib/src/outshine_ref/vegetation/fin_attachment.py"),
        Path("lib/src/outshine_ref/vegetation/spatial_axis.py"),
    ]
    report = {
        "cases": cases,
        "checks": checks,
        "process_peak_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "original_full_fin_weld_lod_or_device_proven": False,
    }
    (ROOT / "fins.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

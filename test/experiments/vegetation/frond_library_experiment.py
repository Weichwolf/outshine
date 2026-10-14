"""Unchanged frond primitives plus own transverse curl, normals and cached views."""

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
from frond_checks import check_differentials, guide_cases
from organ_experiment import UPRIGHT, write_product
from outshine_ref.vegetation import FrondCurl, FrondGuide, PinnatePattern
from outshine_ref.vegetation.axis_surface import axis_mesh

ROOT = Path("build/experiments/vegetation/frond-library")


def before_module():
    p = ROOT / "before/__init__.py"
    name = "frond_before"
    spec = importlib.util.spec_from_file_location(
        name, p, submodule_search_locations=[str(p.parent)]
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def unchanged(before):
    meshes = 0
    for segments, across in ((12, 1), (24, 2), (48, 4)):
        old = before.PinnatePattern().mesh(segments, across)
        new = PinnatePattern().mesh(segments, across)
        assert old.keys() == new.keys()
        for key in old:
            np.testing.assert_array_equal(old[key], new[key])
        for guide in guide_cases():
            previous = before.FrondGuide.from_recipe(guide.recipe())
            a, b = previous.deform(old), guide.deform(new)
            for key in a:
                np.testing.assert_array_equal(a[key], b[key])
            assert (
                previous.recipe() == guide.recipe()
                and previous.identity == guide.identity
            )
            meshes += 1
    return {
        "original_guide_meshes_exact": meshes,
        "original_sources_exact": 3,
        "differential_errors": [check_differentials(g) for g in guide_cases()],
    }


def curl_cases(source):
    cases = []
    for k in (0.0, 3.5, -3.5):
        curl = FrondCurl(k)
        samples = {"cofactor": [], "solve": []}
        error = 0.0
        for repeat in range(5):
            results = {}
            for method in (
                ("cofactor", "solve") if repeat % 2 == 0 else ("solve", "cofactor")
            ):
                began = time.perf_counter()
                results[method] = curl.deform(source, method)
                samples[method].append(time.perf_counter() - began)
            for key in results["cofactor"]:
                a, b = results["cofactor"][key], results["solve"][key]
                if key == "normals":
                    error = max(error, float(np.max(abs(a - b))))
                    np.testing.assert_allclose(a, b, atol=8e-16, rtol=0)
                else:
                    np.testing.assert_array_equal(a, b)
        np.savez_compressed(ROOT / f"curl-{k:g}.npz", **results["cofactor"])
        with np.load(ROOT / f"curl-{k:g}.npz") as saved:
            for key, value in results["cofactor"].items():
                np.testing.assert_array_equal(value, saved[key])
        cases.append(
            {
                "curvature_per_m": k,
                "samples_s": samples,
                "faces": len(source["faces"]),
                "vertices": len(source["vertices"]),
                "maximum_normal_error": error,
                "other_arrays_exact": True,
                "reload_exact": True,
            }
        )
    return cases


def guide_costs():
    source = PinnatePattern().mesh()
    guide = FrondGuide()
    expected = guide.deform(source)
    samples = {"batch": [], "scalar": []}
    for repeat in range(3):
        for method in ("batch", "scalar") if repeat % 2 == 0 else ("scalar", "batch"):
            began = time.perf_counter()
            output = guide.deform(source, method)
            samples[method].append(time.perf_counter() - began)
            for key, value in expected.items():
                np.testing.assert_array_equal(value, output[key])
    return {"samples_s": samples, "outputs_exact": True, "faces": len(source["faces"])}


def scenes(source):
    guide = FrondGuide(bend=-0.55, fold_per_m=0.25)
    shutil.copy2(
        "build/experiments/vegetation/placements/phyllotaxy/bark-material.json",
        ROOT / "bark-material.json",
    )
    products = []
    for index, k in enumerate((0.0, 3.5, -3.5)):
        mesh = guide.deform(FrondCurl(k).deform(source))
        products.append(
            write_product(
                ROOT,
                f"curled-frond-{index}",
                mesh,
                np.array([[(index - 1) * 0.75, 0, 0]]),
                UPRIGHT[None],
                "leaf",
            )
        )
        rib = guide.deform(
            FrondCurl(k).deform(
                axis_mesh(guide.length, 0, 0.004, 0.0007, 96, 8, bark=True)
            )
        )
        products.append(
            write_product(
                ROOT,
                f"curled-rib-{index}",
                rib,
                np.array([[(index - 1) * 0.75, 0, 0]]),
                UPRIGHT[None],
                "wood",
            )
        )
        products[-1]["bark_material"] = "bark-material.json"
    # The same full source and guide, only transverse curl differs.
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    for name, data, camera, target, fov in (
        ("frond-curl-functions", products, (2.1, -4.2, 2.3), (0, 0, 0.55), 35),
        ("frond-curl-detail", products[2:4], (1.1, -1.25, 1.3), (0, 0.02, 0.65), 32),
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
                json.dumps(
                    {
                        "pattern": PinnatePattern().recipe(),
                        "guide": guide.recipe(),
                        "curls": [FrondCurl(k).recipe() for k in (0.0, 3.5, -3.5)],
                    },
                    sort_keys=True,
                ).encode()
            ).hexdigest(),
        }
        (ROOT / f"{name}.json").write_text(json.dumps(record, indent=2) + "\n")
    coarse = PinnatePattern().mesh(12, 1)
    mesh = guide.deform(FrondCurl(3.5).deform(coarse))
    entry = write_product(
        ROOT, "curled-frond-coarse", mesh, np.zeros((1, 3)), UPRIGHT[None], "leaf"
    )
    rib = guide.deform(
        FrondCurl(3.5).deform(
            axis_mesh(guide.length, 0, 0.004, 0.0007, 24, 6, bark=True)
        )
    )
    rib_entry = write_product(
        ROOT, "curled-rib-coarse", rib, np.zeros((1, 3)), UPRIGHT[None], "wood"
    )
    rib_entry["bark_material"] = "bark-material.json"
    record = record | {
        "name": "frond-curl-coarse",
        "products": [entry, rib_entry],
        "camera": (1.1, -1.25, 1.3),
    }
    (ROOT / "frond-curl-coarse.json").write_text(json.dumps(record, indent=2) + "\n")


def main():
    before = before_module()
    checks = unchanged(before)
    source = PinnatePattern().mesh(48, 4)
    cases = curl_cases(source)
    scenes(source)
    paths = [
        Path(__file__),
        *[
            Path("lib/src/outshine_ref/vegetation") / f"{name}.py"
            for name in ("frond_curl", "frond_surface", "frond_pattern")
        ],
    ]
    report = {
        "checks": checks,
        "cases": cases,
        "guide_normal_methods": guide_costs(),
        "process_peak_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "original_property_or_device_equivalence_proven": False,
    }
    (ROOT / "frond-library.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

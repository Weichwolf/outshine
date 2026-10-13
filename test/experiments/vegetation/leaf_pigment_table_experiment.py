"""Compare pigment table sizes and mapping order against direct external optics."""

import hashlib
import json
import time
from pathlib import Path

import colour
import numpy as np
import prosail
from leaf_pigment_checks import pigment_checks
from leaf_pigment_experiment import STATES
from leaf_pigment_optics import (
    FIXED,
    LIMITS,
    build_table,
    direct_colour,
    gamut_map,
    lookup,
)


def reference(points):
    return gamut_map(np.array([direct_colour(*point) for point in points]))[0]


def main():
    output = Path("build/experiments/vegetation/pigment-leaves")
    output.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(77)
    random = np.expm1(rng.uniform([0, 0], np.log1p(LIMITS), (512, 2)))
    random = np.vstack((random, [[0, 0], [80, 0], [0, 10], [80, 10]]))
    exact = reference(random)
    candidates, tables = [], {}
    for size in (17, 33, 65):
        start = time.perf_counter()
        values = build_table(size)
        elapsed = time.perf_counter() - start
        middle = (np.arange(size - 1) + 0.5) / (size - 1)
        x, y = np.meshgrid(middle, middle)
        points = np.expm1(np.column_stack((x.ravel(), y.ravel())) * np.log1p(LIMITS))
        expected = reference(points)
        current = gamut_map(lookup(values, random))[0]
        centers = gamut_map(lookup(values, points))[0]
        mapped = gamut_map(values.reshape(-1, 6))[0].reshape(values.shape)
        wrong_order = lookup(mapped, random)
        error = max(
            float(np.max(abs(current - exact))), float(np.max(abs(centers - expected)))
        )
        case = {
            "size": size,
            "build_seconds": elapsed,
            "probes": len(random) + len(points),
            "max_error": error,
            "gamut_before_interpolation_error": float(np.max(abs(wrong_order - exact))),
            "bytes": values.nbytes,
        }
        candidates.append(case)
        tables[size] = values
        print(case, flush=True)
    selected = next(c["size"] for c in candidates if c["max_error"] <= 1 / 1024)
    values = tables[selected]
    times = {"direct": [], "table": []}
    for repeat in range(3):
        for method in ("direct", "table") if repeat % 2 == 0 else ("table", "direct"):
            start = time.perf_counter()
            actual = (
                reference(random)
                if method == "direct"
                else gamut_map(lookup(values, random))[0]
            )
            times[method].append(time.perf_counter() - start)
            assert np.max(abs(actual - exact)) <= 1 / 1024
    path = output / "pigment-table.npz"
    np.savez_compressed(path, values=values)
    package = Path(prosail.__file__).parent
    spectra = []
    for chl, ant in STATES:
        wavelength, refl, trans = prosail.run_prospect(
            cab=chl, ant=ant, prospect_version="D", **FIXED
        )
        spectra.append(
            {
                "state": [chl, ant],
                "nm": wavelength[:381:5].tolist(),
                "reflection": refl[:381:5].tolist(),
                "transmission": trans[:381:5].tolist(),
            }
        )
    report = {
        "spectra": spectra,
        "candidates": candidates,
        "selected_size": selected,
        "error_limit": 1 / 1024,
        "seconds": times,
        "cost_probes": len(random),
        "fixed": FIXED,
        "checks": pigment_checks(),
        "versions": {
            "prosail": prosail.__version__,
            "colour-science": colour.__version__,
            "numpy": np.__version__,
        },
        "reference_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in package.glob("prospect*.*")
        },
        "table_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in Path(__file__).parent.glob("leaf_pigment*.py")
        },
    }
    (output / "table.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()

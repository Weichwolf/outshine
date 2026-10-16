"""Save the same root/shape stand, independent gap checks and directional products."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from grass_projection import contacts, footprint_mips
from grass_projection_reference import verify
from grass_projection_stand import (
    DIRECTIONS,
    LOWER,
    UPPER,
    build_stand,
    require_support,
)


def run(output, size=256):
    if output.exists():
        raise ValueError("explicit fresh output path required")
    began = time.perf_counter()
    recipe, identity, stand = build_stand()
    output.mkdir(parents=True)
    np.savez_compressed(output / "stand.npz", **stand)
    (output / "stand.json").write_text(
        json.dumps({"recipe": recipe, "identity": identity}, indent=2)
    )
    prepared = time.perf_counter()
    reports = []
    for name, direction in DIRECTIONS.items():
        require_support(recipe, LOWER, UPPER, direction)
        samples = {m: [] for m in ("sort", "scatter")}
        outputs = {}
        for trial in range(3):
            for method in (
                ("sort", "scatter") if trial % 2 == 0 else ("scatter", "sort")
            ):
                start = time.perf_counter()
                product = contacts(
                    stand["vertices"],
                    stand["faces"],
                    LOWER,
                    UPPER,
                    direction,
                    size,
                    method=method,
                )
                samples[method].append(time.perf_counter() - start)
                if method in outputs:
                    for key in ("face", "height", "normal", "coverage"):
                        np.testing.assert_array_equal(
                            product[key], outputs[method][key]
                        )
                outputs[method] = product
        for key in ("face", "height", "normal", "coverage"):
            np.testing.assert_array_equal(outputs["sort"][key], outputs["scatter"][key])
        product = outputs["scatter"]
        accuracy = verify(
            stand["vertices"], stand["faces"], LOWER, UPPER, direction, product
        )
        levels = footprint_mips(
            product["coverage"], product["height"], product["normal"]
        )
        for level in levels:
            np.testing.assert_allclose(
                level.mean((0, 1)), levels[0].mean((0, 1)), rtol=0, atol=1e-13
            )
        encoded = {
            key: product[key] for key in ("face", "height", "normal", "coverage")
        }
        encoded.update({f"moments_{i}": v for i, v in enumerate(levels)})
        np.savez_compressed(output / f"{name}.npz", **encoded)
        row = {
            "name": name,
            "direction": direction.tolist(),
            "cpu_s": samples,
            "accuracy": accuracy,
            "work": {m: outputs[m]["work"] for m in outputs},
            "mip_bytes": sum(v.nbytes for v in levels),
        }
        reports.append(row)
        print(json.dumps(row), flush=True)
    report = {
        "format": "ground-anchored-grass-contacts-1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "stand_identity": identity,
        "size": size,
        "footprint_m": [LOWER.tolist(), UPPER.tolist()],
        "roots": len(stand["site_ids"]),
        "triangles": len(stand["faces"]),
        "stand_array_bytes": sum(a.nbytes for a in stand.values()),
        "source_prepare_s": prepared - began,
        "total_s": time.perf_counter() - began,
        "views": reports,
        "hashes": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.glob("*")
        },
        "scope": "Fixed-direction opaque contacts, geometric normals and ground gaps; not leaf R/T, canopy light, angular interpolation or native LOD admission",
    }
    (output / "result.json").write_text(json.dumps(report, indent=2))
    print(
        json.dumps({k: report[k] for k in ("commit", "roots", "triangles", "total_s")}),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--size", type=int, default=256)
    args = parser.parse_args()
    run(args.output, args.size)

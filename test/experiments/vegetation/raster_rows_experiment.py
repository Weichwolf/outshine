"""Equal-output row/box timing on the saved canonical grass stand from #463."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from grass_projection import footprint_mips
from grass_projection_instances import instanced_contacts
from grass_projection_reader import read_product
from grass_projection_reference import verify


def compare(recipe, compact, full, cached, lower, upper, direction, size):
    samples = {name: [] for name in ("boxes", "rows")}
    outputs = {}
    for trial in range(3):
        for name in ("boxes", "rows") if trial % 2 == 0 else ("rows", "boxes"):
            began = time.perf_counter()
            product = instanced_contacts(
                recipe,
                compact,
                lower,
                upper,
                direction,
                size,
                max(262144, size * size),
                sampler=name,
            )
            samples[name].append(time.perf_counter() - began)
            if name in outputs:
                for key in ("face", "height", "normal", "coverage"):
                    np.testing.assert_array_equal(product[key], outputs[name][key])
            outputs[name] = product
    for key in ("face", "height", "normal", "coverage"):
        np.testing.assert_array_equal(outputs["boxes"][key], outputs["rows"][key])
        if size == len(cached["coverage"]):
            np.testing.assert_array_equal(outputs["rows"][key], cached[key])
    accuracy = (
        verify(
            full["vertices"], full["faces"], lower, upper, direction, outputs["rows"]
        )
        if size == 512
        else {"scope": "Exact original independently checked256² products"}
    )
    return outputs["rows"], {
        "size": size,
        "wall_s": samples,
        "work": {name: p["work"] for name, p in outputs.items()},
        "accuracy": accuracy,
    }


def run(inputs, output):
    if output.exists():
        raise ValueError("fresh output path required")
    report, stand, cached = read_product(inputs)
    recipe = json.loads((inputs / "stand.json").read_text())["recipe"]
    compact = {k: v for k, v in stand.items() if k.startswith(("pose_", "form_"))}
    lower, upper = np.asarray(report["footprint_m"])
    output.mkdir(parents=True)
    for name in ("stand.json", "stand.npz"):
        (output / name).symlink_to((inputs / name).resolve())
    views, results = [], []
    for view in report["views"]:
        direction = np.asarray(view["direction"])
        for size in (256, 512):
            product, cost = compare(
                recipe,
                compact,
                stand,
                cached[view["name"]],
                lower,
                upper,
                direction,
                size,
            )
            cost.update(name=view["name"], direction=direction.tolist())
            results.append(cost)
            print(json.dumps(cost), flush=True)
            if size == 512:
                levels = footprint_mips(
                    product["coverage"], product["height"], product["normal"]
                )
                for level in levels:
                    np.testing.assert_allclose(
                        level.mean((0, 1)), levels[0].mean((0, 1)), atol=1e-13, rtol=0
                    )
                encoded = {
                    k: product[k] for k in ("face", "height", "normal", "coverage")
                }
                encoded.update({f"moments_{i}": a for i, a in enumerate(levels)})
                np.savez_compressed(output / f"{view['name']}.npz", **encoded)
                views.append(
                    {
                        "name": view["name"],
                        "direction": direction.tolist(),
                        "accuracy": cost["accuracy"],
                    }
                )
    current = {
        **{
            k: v
            for k, v in report.items()
            if k not in ("views", "hashes", "source_prepare_s", "total_s")
        },
        "size": 512,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "views": views,
        "comparison": results,
        "hashes": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.glob("*")
        },
        "scope": "Same canonical source and contacts, bounded row sampling;512² finite angle study, not new lighting/parallax/native admission",
    }
    (output / "result.json").write_text(json.dumps(current, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)

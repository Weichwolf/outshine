"""Actual contact-conditioned light versus an independent ground-visibility mean."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from grass_light_model import contact_points, lighting, require_light_support
from grass_projection_reader import read_product
from grass_shadow import GrassShadowIndex, query_origins
from grass_shadow_reference import direct_visibility


def light_states(control):
    sky = [
        np.array(d, float)
        for d in ((0.6, 0, 0.8), (-0.6, 0, 0.8), (0, 0.6, 0.8), (0, -0.6, 0.8))
    ]
    states = {}
    for name, sign in (("front", 1), ("back", -1)):
        sun = np.array((sign * np.sqrt(3) / 2, 0, 0.5))
        lights = [
            {
                "name": "sun",
                "direction": sun.tolist(),
                "irradiance_rgb": control["sun_irradiance_rgb"],
            }
        ]
        lights += [
            {
                "name": f"sky-{i}",
                "direction": d.tolist(),
                "irradiance_rgb": control["sky_beam_irradiance_rgb"],
            }
            for i, d in enumerate(sky)
        ]
        states[name] = {
            "lights": lights,
            "leaf_rt": control["leaf_rt"],
            "ground_albedo": control["ground_albedo"],
        }
    return states


def check_rays(index, stand, points, normals, direction, bias):
    origins = query_origins(points, normals, direction, bias)
    lengths = np.maximum(
        (index.maximum_height + 2 * bias - origins[:, 2]) / direction[2], 0
    )
    exact, work = direct_visibility(
        stand["vertices"], stand["faces"], origins, direction, lengths
    )
    packet, counters = index.visibility(points, normals, direction, bias)
    np.testing.assert_array_equal(packet, exact)
    scalar = np.concatenate(
        [
            index.visibility(p[None], n[None], direction, bias, batch=1)[0]
            for p, n in zip(points, normals, strict=True)
        ]
    )
    np.testing.assert_array_equal(packet, scalar)
    timings = {}
    for method in ("scalar", "packet"):
        samples = []
        for _ in range(3):
            began = time.perf_counter()
            if method == "scalar":
                answer = np.concatenate(
                    [
                        index.visibility(p[None], n[None], direction, bias, batch=1)[0]
                        for p, n in zip(points, normals, strict=True)
                    ]
                )
            else:
                answer, _ = index.visibility(points, normals, direction, bias)
            samples.append(time.perf_counter() - began)
            np.testing.assert_array_equal(answer, exact)
        timings[method] = samples
    return {"exact": True, "reference": work, "packet": counters, "wall_s": timings}


def run(inputs, scenario, output):
    if output.exists():
        raise ValueError("fresh diagnostic output required")
    control = json.loads(scenario.read_text())
    report, stand, products = read_product(inputs)
    recipe = json.loads((inputs / "stand.json").read_text())["recipe"]
    view = np.asarray(
        next(v["direction"] for v in report["views"] if v["name"] == control["view"])
    )
    product = products[control["view"]]
    footprint = np.asarray(report["footprint_m"])
    states = light_states(control)
    for state in states.values():
        require_light_support(
            recipe,
            *footprint,
            view,
            [np.asarray(l["direction"]) for l in state["lights"]],
            control["bias_m"],
        )
    index = GrassShadowIndex(stand["vertices"], stand["faces"])
    _, points, normals, _ = contact_points(view, *footprint, product)
    chosen = np.random.Generator(np.random.PCG64(468)).choice(
        len(points), 128, replace=False
    )
    records = []
    output.mkdir(parents=True)
    with (
        patch(
            "grass_projection_stand.build_stand",
            side_effect=AssertionError("regrew light state"),
        ),
        patch(
            "grass_shadow.GrassShadowIndex.__init__",
            side_effect=AssertionError("rebuilt shared BVH"),
        ),
    ):
        for name, state in states.items():
            began = time.perf_counter()
            result = lighting(
                index, recipe, product, footprint, view, state, control["bias_m"]
            )
            wall = time.perf_counter() - began
            np.testing.assert_array_less(
                result["reflection_only"] - 1e-14, result["radiance"]
            )
            sun = np.asarray(state["lights"][0]["direction"])
            checks = check_rays(
                index, stand, points[chosen], normals[chosen], sun, control["bias_m"]
            )
            sky_checks = []
            for lamp in state["lights"][1:]:
                d = np.asarray(lamp["direction"])
                subset = chosen[:16]
                origins = query_origins(
                    points[subset], normals[subset], d, control["bias_m"]
                )
                lengths = np.maximum(
                    (index.maximum_height + 2 * control["bias_m"] - origins[:, 2])
                    / d[2],
                    0,
                )
                exact, counters = direct_visibility(
                    stand["vertices"], stand["faces"], origins, d, lengths
                )
                actual, _ = index.visibility(
                    points[subset], normals[subset], d, control["bias_m"]
                )
                np.testing.assert_array_equal(actual, exact)
                sky_checks.append({"name": lamp["name"], **counters, "exact": True})
            first, _ = index.visibility(
                points, normals, sun, bias=control["bias_m"] / 2
            )
            second, _ = index.visibility(
                points, normals, sun, bias=control["bias_m"] * 2
            )
            changed = int(np.count_nonzero(first != second))
            if changed / len(points) > 0.001:
                raise ValueError(
                    f"preset one-per-thousand offset sensitivity criterion failed: {changed}/{len(points)}"
                )
            np.savez_compressed(
                output / f"{name}.npz",
                **{k: v for k, v in result.items() if isinstance(v, np.ndarray)},
            )
            row = {
                "state": name,
                "wall_s": wall,
                "lights": result["lights"],
                "ray_checks": checks,
                "sky_checks": sky_checks,
                "bias_changed_pixels": changed,
                "pixels": len(points),
                "mean_rgb": {
                    k: result[k].mean((0, 1)).tolist()
                    for k in ("radiance", "reflection_only", "independent_mean")
                },
            }
            records.append(row)
            print(json.dumps(row), flush=True)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "stand_identity": report["stand_identity"],
        "source_inputs": str(inputs.resolve()),
        "source_report_sha256": hashlib.sha256(
            (inputs / "result.json").read_bytes()
        ).hexdigest(),
        "scenario": control,
        "states": states,
        "view": view.tolist(),
        "footprint_m": footprint.tolist(),
        "setup": index.setup,
        "records": records,
        "scope": control["scope"],
        "cached_geometry_rebuilt_for_light": False,
    }
    (output / "result.json").write_text(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument(
        "--scenario",
        type=Path,
        default=Path(__file__).with_name("grass_light_control.json"),
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.scenario, args.output)

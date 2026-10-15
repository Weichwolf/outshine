"""Compare spatial ground/litter composition with the shipped constant mixture."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
from PIL import Image, ImageDraw

from ground_detail import filtered, noise2
from ground_litter_filter import field


ROOT = Path(__file__).resolve().parents[3]


def offset(at, footprint, scale, coverage, method):
    amplitude = 2.0 * min(coverage, 1.0 - coverage)
    if amplitude == 0.0:
        return np.zeros(at.shape[:-1])
    if method == "mean":
        return np.zeros(at.shape[:-1])
    if method == "point":
        return amplitude * (noise2(at / scale) - 0.5)
    if method == "fade":
        return amplitude * filtered(at, footprint, scale)
    if method == "integral":
        return amplitude * field(at, footprint, scale)
    total = np.zeros(at.shape[:-1])
    count = 3 if method == "box9" else 32
    for y in range(count):
        for x in range(count):
            shift = (np.array([x, y]) + 0.5) / count - 0.5
            total += noise2((at + footprint * shift) / scale) - 0.5
    return amplitude * total / (count * count)


def optical(material, model):
    wet = 1.0
    if material["name"] not in model.get("exclusions", []):
        wet -= model["kWet"] * material["moisture"]
    colour = np.array(material["albedo"]) * wet * material["visibleBroadbandRatio"]
    return np.append(colour, material["roughness"])


def constituents(template, catalog):
    materials = {row["name"]: row for row in catalog["classes"]}
    ground = template["ground"]
    base = materials[ground["class"]]
    litter = materials[ground.get("litterClass", base["litter"]["class"])]
    coverage = ground.get("litterCoverage", base["litter"]["coverage"])
    return base, litter, coverage


def colour_picture(optics):
    linear = optics[..., :3] * 0.7
    srgb = np.where(linear <= 0.0031308, linear * 12.92,
                    1.055 * linear ** (1.0 / 2.4) - 0.055)
    return Image.fromarray(np.uint8(np.clip(srgb, 0.0, 1.0) * 255.0))


def compare(template, catalog, output):
    base, litter, coverage = constituents(template, catalog)
    scale = base["detailScaleM"][0]
    first, second = [optical(m, catalog["moistureModel"]) for m in (base, litter)]
    mean = first * (1.0 - coverage) + second * coverage
    delta = second - first
    rng = np.random.default_rng(196)
    probes = rng.uniform(-128.0, 128.0, (512, 2)) * scale
    errors = []
    timings = {}
    for relative in (0.05, 0.25, 0.5, 0.8, 1.0, 1.5, 2.0, 8.0):
        footprint = scale * relative
        reference = offset(probes, footprint, scale, coverage, "reference")
        row = {"footprint_over_scale": relative}
        for method in ("mean", "point", "fade", "box9", "integral"):
            started = time.perf_counter()
            actual = offset(probes, footprint, scale, coverage, method)
            timings[method] = timings.get(method, 0.0) + time.perf_counter() - started
            difference = (actual - reference)[..., None] * delta
            row[method] = {"linear_rgb_rms": float(np.sqrt(np.mean(difference[..., :3] ** 2))),
                           "roughness_rms": float(np.sqrt(np.mean(difference[..., 3] ** 2)))}
            assert np.all(coverage + actual >= -1e-12) and np.all(coverage + actual <= 1.0 + 1e-12)
            if method == "integral" and relative <= 1.0:
                assert np.max(np.abs(actual - reference)) < 0.00015
        errors.append(row)
    x, y = np.meshgrid(np.arange(320) * scale / 40.0,
                        np.arange(240) * scale / 40.0)
    at = np.stack([x + scale * 8.13, y - scale * 5.37], axis=-1)
    canvas = Image.new("RGB", (1280, 520), (24, 24, 24))
    draw = ImageDraw.Draw(canvas)
    for row, relative in enumerate((0.025, 0.8)):
        for column, method in enumerate(("mean", "point", "fade", "integral")):
            value = offset(at, relative * scale, scale, coverage, method)
            picture = colour_picture(mean + value[..., None] * delta)
            canvas.paste(picture, (320 * column, 260 * row + 20))
            draw.text((320 * column + 8, 260 * row + 4),
                      f"{method} / footprint {relative:g} x {scale:g} m", fill="white")
    canvas.save(output / (template["name"] + ".png"))
    near = coverage + offset(probes, 0.0, scale, coverage, "fade")
    far = offset(probes, scale * 100.0, scale, coverage, "integral")
    assert np.array_equal(far, np.zeros(len(probes)))
    for endpoint in (0.0, 1.0):
        assert np.array_equal(offset(probes, 0.0, scale, endpoint, "integral"), far)
    return {"template": template["name"], "base": base["name"], "litter": litter["name"],
            "coverage": coverage, "coarse_m": scale, "mean_optics": mean.tolist(),
            "near_weight_range": [float(near.min()), float(near.max())],
            "near_spatial_mean": float(near.mean()), "errors": errors,
            "python_seconds_for_4096_probes": timings}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    paths = [ROOT / "src/assets/world/ground-materials.json",
             ROOT / "src/assets/world/vegetation.json"]
    catalog, vegetation = [json.loads(path.read_text()) for path in paths]
    names = {"mixed_broadleaf", "conifer_forest", "alpine_grassland"}
    results = [compare(template, catalog, args.output)
               for template in vegetation["templates"] if template["name"] in names]
    report = {"scope": "Shipped optical recipes; flat material response, no runtime/GPU proof",
              "inputs": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in paths}, "results": results,
              "decision": "integral: exact axis-aligned square box through one cell; fade between one and two cells",
              "hash_work": {"mean": 0, "point": 4, "fade": "0 additional if coarse field reused",
                            "box9": 36, "integral": "4-16; zero for footprint >= 2 cells",
                            "reference": 4096}}
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"templates": len(results), "output": str(args.output)}))


if __name__ == "__main__":
    main()

"""Independent mature wood architectures with unchanged physical needle forms."""

import hashlib
import json
from pathlib import Path

from conifer_whole_products import BASE
from outshine_ref.vegetation import NeedledPlant

ROOT = Path(__file__).resolve().parents[3]
SPECIES = (("picea-abies", "spruce"), ("pinus-sylvestris", "pine"))


def mature_plan(species, profile, variant):
    if type(variant) is not int or not 0 <= variant < 4:
        raise ValueError("one of the four existing stand-size variants required")
    recipe = json.loads((BASE / species / "whole-plan.json").read_text())
    scale = 0.85 + variant * 0.1
    height, spread = profile["height_m"] * scale, profile["spread_m"] * scale
    spruce = species == "picea-abies"
    levels = recipe["wood"]["levels"]
    levels[0].update(
        length=height, base_radius=profile["dbh_cm"] / 200 * scale, tip_radius=0.0045
    )
    levels[1].update(
        branches=63 if spruce else 24,
        whorl_size=3,
        first=profile["bole_frac"],
        last=0.96 if spruce else 0.94,
        length=spread * (0.31 if spruce else 0.34),
        base_radius=0.025 * scale if spruce else 0.045 * scale,
        tip_radius=0.0008,
        tip_scale=0.09 if spruce else 0.48,
        length_shape="taper" if spruce else "rounded",
        angle=1.64 if spruce else 0.86,
        angle_tip=0.61 if spruce else 1.24,
        angle_variation=0.17,
        bend=0.25 if spruce else 0.12,
        attachment_jitter=0.38 if spruce else 0.25,
        azimuth_variation=0.42,
    )
    levels[2].update(
        branches=24 if spruce else 20,
        length=spread * (0.15 if spruce else 0.12),
        base_radius=0.0035 * scale if spruce else 0.006 * scale,
        tip_radius=0.0006,
        tip_scale=0.5,
        bend=-0.65 if spruce else 0.27,
        angle=1.14,
        angle_variation=0.3,
        azimuth_variation=0.6,
    )
    levels[3].update(
        branches=11 if spruce else 12,
        length=spread * (0.07 if spruce else 0.05),
        base_radius=0.0007 if spruce else 0.001,
        tip_radius=0.0002,
        tip_scale=0.55,
        angle=1.2 if spruce else 0.75,
        angle_variation=0.3,
        azimuth_variation=0.65,
    )
    recipe["patterns"][0] = None
    recipe["wood"]["version"] = "branching-plant-4"
    for index, level in enumerate(levels):
        level["radius_parent_ratio"] = None if index == 0 else 0.68
    recipe["wood"]["seed"] = 718 + 97 * variant + (0 if spruce else 113)
    return NeedledPlant.from_recipe(recipe)


def prepare(output):
    output.mkdir(parents=True, exist_ok=True)
    cases = []
    for kind, (species, profile_name) in enumerate(SPECIES):
        profile_path = ROOT / "src/assets/world/species" / f"{profile_name}.json"
        profile = json.loads(profile_path.read_text())
        for variant in range(4):
            plan = mature_plan(species, profile, variant)
            key = f"{species}-{variant}"
            path = output / f"{key}.json"
            path.write_text(json.dumps(plan.recipe(), indent=2) + "\n")
            cases.append(
                {
                    "key": key,
                    "species": species,
                    "latin": profile["botanical"],
                    "kind": kind,
                    "variant": variant,
                    "recipe": str(path),
                    "identity": plan.identity,
                    "summary": plan.window_summary(),
                    "profile_sha256": hashlib.sha256(
                        profile_path.read_bytes()
                    ).hexdigest(),
                    "scope": "own structural mature form interpretation; conserved physical needles, size profiles and near plan; age/population/LAI calibration open",
                }
            )
    (output / "recipes.json").write_text(json.dumps(cases, indent=2) + "\n")
    return cases


if __name__ == "__main__":
    for case in prepare(Path("build/experiments/vegetation/mature-conifers/recipes")):
        print(case["key"], case["summary"])

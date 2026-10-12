"""Prepare named calibration fixtures at organ, plant and foliage-group detail."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from botanical_profiles import PROFILES, plant_recipe
from cloud_product import pack_atlas
from foliage_views import bake_views, select_view
from group_cloud import group_geometry
from organ_experiment import write_product
from plant_experiment import make_scenes
from preview_lighting import FILL_POSITION, SUN_ROTATION
from scipy.spatial.transform import Rotation


def group_scene(plant, output, recipe):
    camera = np.asarray(recipe["camera"])
    requests = {
        "camera": camera,
        "sun": Rotation.from_euler("xyz", SUN_ROTATION).apply((0, 0, 1)),
        "fill": np.asarray(FILL_POSITION),
    }
    charts, images, uvs, records = [], [], [], []
    for path in plant.children(()):
        source = group_geometry(plant, path)
        views = bake_views(source, count=32, size=32)
        records.append(
            {"path": path, "ids": source["ids"].tolist(), "views": len(views)}
        )
        for role, target in requests.items():
            direction = target if role == "sun" else target - source["anchor_origin"]
            index, chart = select_view(views, direction @ source["anchor_frame"])
            mesh = dict(chart.mesh)
            mesh["vertices"] = (
                mesh["vertices"] @ source["anchor_frame"].T + source["anchor_origin"]
            )
            mesh["normals"] = mesh["normals"] @ source["anchor_frame"].T
            charts.append((role, mesh, source["ids"], index))
            uvs.append(mesh["uv"].copy())
            images.append(
                tuple(
                    np.uint8(np.rint(np.clip(a, 0, 1) * 255))
                    for a in (chart.colour, chart.normal)
                )
            )
    atlas = pack_atlas(output, images, uvs, 32)
    products = [p for p in recipe["products"] if p["surface"] == "wood"]
    for role in requests:
        selected = [(i, mesh) for i, (r, mesh, _, _) in enumerate(charts) if r == role]
        mesh = {
            "vertices": np.concatenate([m["vertices"] for _, m in selected]),
            "faces": np.concatenate(
                [m["faces"] + 4 * j for j, (_, m) in enumerate(selected)]
            ),
            "normals": np.concatenate([m["normals"] for _, m in selected]),
            "uv": np.concatenate([uvs[i] for i, _ in selected]),
        }
        product = write_product(
            output, f"groups-{role}", mesh, np.zeros((1, 3)), np.eye(3)[None], "leaf"
        )
        product.update(
            card=True,
            textures={"colour": "cloud-colour.png", "normal": "cloud-normal.png"},
            pass_role="camera" if role == "camera" else "shadow",
        )
        if role != "camera":
            product["shadow_light"] = role
        products.append(product)
    result = recipe | {"name": "groups", "products": products}
    (output / "groups.json").write_text(json.dumps(result, indent=2) + "\n")
    (output / "groups-plan.json").write_text(
        json.dumps(
            {
                "plan_identity": plant.identity,
                "groups": records,
                "atlas": atlas,
                "scope": "all 32 views evaluated per group; only requested camera/light snapshot packed in this form probe, not a complete persistent LOD cache",
            },
            indent=2,
        )
        + "\n"
    )


def check_plan(name, plant):
    summary = plant.summary()
    assert not plant._axes and not plant._leaves
    detail = plant.expand(summary["axes"], summary["leaves"])
    ids = np.concatenate([p["ids"] for p in detail["groups"]])
    np.testing.assert_array_equal(ids, np.arange(summary["leaves"]))
    fresh = plant_recipe(name)
    paths = tuple(plant.paths())
    for path in reversed(paths):
        a, b = (
            plant.leaves(path, summary["leaves"]),
            fresh.leaves(path, summary["leaves"]),
        )
        if a is not None:
            for key in a:
                np.testing.assert_array_equal(a[key], b[key])
    return {
        "plan_identity": plant.identity,
        "axes": summary["axes"],
        "leaves": summary["leaves"],
        "reversed_detail_requests_match": True,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/botanical")
    )
    args = parser.parse_args()
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    modified = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
            text=True,
        ).strip()
    )
    report = {}
    for name, profile in PROFILES.items():
        plant = plant_recipe(name)
        report[name] = check_plan(name, plant)
        output = args.output / name
        output.mkdir(parents=True, exist_ok=True)
        make_scenes(output, plant, commit, modified)
        recipe = json.loads((output / "sapling.json").read_text())
        for filename in ("sapling.json", "sapling-branch.json"):
            item = json.loads((output / filename).read_text())
            item.update(
                genus=profile["genus"],
                target_species=profile["species"],
                form_scope="own authored juvenile form study; mature architecture, diagnostic botanical details and species-quality validation remain open",
            )
            (output / filename).write_text(json.dumps(item, indent=2) + "\n")
        group_scene(plant, output, recipe)
        print(name, report[name], flush=True)
    (args.output / "botanical.json").write_text(
        json.dumps(
            {"commit": commit, "source_modified": modified, "cases": report}, indent=2
        )
        + "\n"
    )


if __name__ == "__main__":
    main()

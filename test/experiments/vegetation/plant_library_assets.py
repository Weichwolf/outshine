"""Complete cached plant rows and render assets with procedural material provenance."""

import hashlib
import json
import shutil
import subprocess
from dataclasses import asdict
from pathlib import Path

import numpy as np
from leaf_normal_experiment import export_level
from leaf_normal_moments import mip_pyramid, normal_slopes, slope_moments
from organ_experiment import write_product
from outshine_ref.vegetation.axis_surface import axis_mesh
from PIL import Image
from plant_experiment import sapling

ROOT = Path("build/experiments/vegetation/plant-library/assets")


def leaf_material(blade, index):
    source = Path("build/experiments/vegetation/normal-leaves")
    recipe = json.loads((source / "developed-1-material.json").read_text())
    height = (
        np.asarray(Image.open(source / recipe["maps"]["height"]))[::-1]
        / 255
        * recipe["height_m"]
    )
    roughness = np.asarray(Image.open(source / recipe["maps"]["roughness"]))[::-1] / 255
    maps = {}
    for key, name in recipe["maps"].items():
        if key in ("normal", "roughness"):
            continue
        path = source / name
        shutil.copy2(path, ROOT / path.name)
        maps[key] = path.name
    for key, value in tuple(recipe.items()):
        if (
            isinstance(value, str)
            and value.endswith(".npz")
            and key != "normal_moments"
        ):
            path = source / value
            shutil.copy2(path, ROOT / path.name)
            recipe[key] = path.name
    moments = slope_moments(normal_slopes(blade, height), roughness)
    levels = []
    checks = []
    for level, values in enumerate(mip_pyramid(moments)):
        files, check = export_level(ROOT, f"leaf-{index}-mip-{level}", values)
        levels.append(files)
        checks.append(check)
    path = ROOT / f"leaf-{index}-moments.npz"
    np.savez_compressed(path, moments=moments)
    recipe.update(
        blade=asdict(blade),
        normal_levels=levels,
        maps=maps | {key: levels[0][key] for key in ("normal", "roughness")},
        normal_moments=path.name,
        normal_moments_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        metallic=0,
        pattern_semantics="shared normalized UV field; normals use this blade metric",
        botanical_calibration=False,
    )
    recipe["normal_level_sha256"] = {
        name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
        for files in levels
        for name in files.values()
    }
    name = f"leaf-{index}-material.json"
    (ROOT / name).write_text(json.dumps(recipe, indent=2) + "\n")
    return name, checks


def products(plant, tables, detail, materials):
    near = detail == "near"
    result = []
    for state in plant.expand(100000, 100000)["axes"]:
        base_radius, tip_radius = plant.radii(state.path)
        mesh = axis_mesh(
            state.length,
            state.bend,
            base_radius,
            tip_radius,
            24 if near else 12,
            8 if near else 6,
            bark=True,
        )
        entry = write_product(
            ROOT,
            f"axis-{state.id}-{detail}",
            mesh,
            state.origin[None],
            state.frame[None],
            "wood",
        )
        entry.update(bark_material="bark-material.json", axis_id=state.id)
        np.savez_compressed(
            ROOT / entry["placements"],
            origins=state.origin[None],
            frames=state.frame[None],
            organ_ids=np.array([state.id], dtype=np.uint64),
        )
        result.append(entry)
    for variant, blade in enumerate(plant.blades):
        selected = tables["leaf_variants"] == variant
        entry = write_product(
            ROOT,
            f"leaf-{variant}-{detail}",
            blade.mesh(64 if near else 16, 2),
            tables["leaf_origins"][selected],
            tables["leaf_frames"][selected],
            "leaf",
        )
        entry.update(
            leaf_material=materials[variant],
            normal_level=0 if near else 2,
            variant=variant,
        )
        np.savez_compressed(
            ROOT / entry["placements"],
            origins=tables["leaf_origins"][selected],
            frames=tables["leaf_frames"][selected],
            organ_ids=tables["leaf_ids"][selected],
            parent_axis_ids=tables["leaf_axis_ids"][selected],
        )
        result.append(entry)
    return result


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    plant = sapling()
    tables = plant.tables(100000, 100000)
    np.savez_compressed(ROOT / "canonical-plant.npz", **tables)
    with np.load(ROOT / "canonical-plant.npz") as saved:
        for key in tables:
            np.testing.assert_array_equal(saved[key], tables[key])
    bark = Path("build/experiments/vegetation/placements/phyllotaxy/bark-material.json")
    shutil.copy2(bark, ROOT / "bark-material.json")
    materials = []
    material_checks = []
    for index, blade in enumerate(plant.blades):
        name, checks = leaf_material(blade, index)
        materials.append(name)
        material_checks.extend(checks)
    near = products(plant, tables, "near", materials)
    coarse = products(plant, tables, "coarse", materials)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    for name, data, camera, target, fov in (
        ("plant-library-near", near, (3, -4, 2.7), (0, 0, 0.85), 35),
        ("plant-library-coarse", coarse, (3, -4, 2.7), (0, 0, 0.85), 35),
        ("plant-library-detail", near, (0.95, -1.2, 1.6), (0.35, -0.1, 1.2), 30),
    ):
        scene = {
            "name": name,
            "products": data,
            "camera": camera,
            "target": target,
            "fov_y": fov,
            "resolution": [2560, 1440],
            "samples": 32,
            "ground_size_m": 8,
            "source_commit": commit,
            "source_modified": True,
            "plan_identity": plant.identity,
        }
        (ROOT / f"{name}.json").write_text(json.dumps(scene, indent=2) + "\n")
    files = {
        str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in ROOT.iterdir()
        if p.is_file() and p.name != "asset.json"
    }
    manifest = {
        "version": "canonical-plant-reference-1",
        "plan": plant.recipe(),
        "identity": plant.identity,
        "scientific_comparison": "Quercus robur",
        "botanical_recipe_verified": False,
        "frame": "local ENU, X east/Y north/Z up; organ X transverse/Y forward/Z normal",
        "units": "metres",
        "scale": 1,
        "axes": len(tables["axis_ids"]),
        "leaves": len(tables["leaf_ids"]),
        "table": "canonical-plant.npz",
        "summary": plant.summary(),
        "bounds_anchor": [0, 0, 0],
        "near": near,
        "coarse": coarse,
        "material_contract": {
            "colour_space": "linear",
            "metallic": 0,
            "roughness": "perceptual",
            "normal": "tangent",
            "coverage": "opaque spatial leaf mesh",
            "transmission": "separate procedural transmittance/weight maps",
        },
        "material_checks": material_checks,
        "uv_pattern_botany_verified": False,
        "complete_arrays_reloaded_exact": True,
        "topological_branch_union": False,
        "files_sha256": files,
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__),
                Path(__file__).with_name("leaf_normal_experiment.py"),
                Path(__file__).with_name("leaf_normal_moments.py"),
                Path(__file__).with_name("organ_experiment.py"),
            )
        },
    }
    (ROOT / "asset.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(
        json.dumps(
            {
                "identity": plant.identity,
                "axes": manifest["axes"],
                "leaves": manifest["leaves"],
                "bytes": sum((ROOT / name).stat().st_size for name in files),
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()

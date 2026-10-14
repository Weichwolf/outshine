"""Complete dense crown cache from bounded shoot/pose packets, with matched detail."""

import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from bark_recipe import bark_recipe
from conifer_crown_recipes import crown_conifers
from europe_conifer_products import poses, scene, wood_product, write_json
from needle_material import write_needle_material
from needle_pose_stream import stream_foliage
from outshine_ref.vegetation.axis_surface import axis_mesh

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/vegetation/conifer-crowns"


def needle_meshes(species, directory, mode):
    result = []
    for index, needle in enumerate(species.needles):
        name = f"{species.key}-needle-{mode}-{index}.npz"
        whole = mode == "whole"
        sides = 4 if needle.section == "rhombus" or species.key == "pinus-cembra" else 8
        mesh = needle.mesh(
            4 if whole else 24,
            sides if whole else 16,
            axial_sampling="profile" if whole else "uniform",
        )
        np.savez_compressed(directory / name, **mesh)
        result.append(name)
    return result


def wood_meshes(species, plan, directory):
    result = []
    for path in plan.wood.paths():
        axis, level = plan.wood.axis(path), plan.wood.levels[len(path)]
        mesh = axis_mesh(
            axis.length,
            axis.bend,
            level.base_radius,
            level.tip_radius,
            16 if not path else 8,
            12 if not path else 8,
            bark=True,
        )
        result.append(
            wood_product(
                directory,
                f"{species.key}-whole-axis-{axis.id}",
                mesh,
                axis.origin[None],
                axis.frame[None],
                np.array([axis.id], np.uint64),
            )
        )
    return result


def packet_products(species, plan, directory, needles, shorts):
    meshes = needle_meshes(species, directory, "whole")
    result = []
    for index, records in enumerate(needles):
        for record in records:
            result.append(
                dict(record)
                | {
                    "mesh": meshes[index],
                    "surface": "leaf",
                    "needle_material": f"needle-{species.needles[0].section}-material.json",
                }
            )
    for depth, records in enumerate(shorts):
        pattern = plan.patterns[depth]
        if not records:
            continue
        radius = 0.0002 if species.key == "picea-abies" else 0.00065
        mesh = axis_mesh(pattern.short_length, 0, radius, radius * 0.7, 2, 6, bark=True)
        name = f"{species.key}-whole-short-{depth}.npz"
        np.savez_compressed(directory / name, **mesh)
        for record in records:
            result.append(
                dict(record)
                | {
                    "mesh": name,
                    "surface": "wood",
                    "detail_kind": "short-shoot",
                    "bark_material": "bark.json",
                }
            )
    return result


def near_products(species, plan, directory, path):
    axis, level = plan.wood.axis(path), plan.wood.levels[len(path)]
    mesh = axis_mesh(
        axis.length, axis.bend, level.base_radius, level.tip_radius, 16, 8, bark=True
    )
    result = [
        wood_product(
            directory,
            f"{species.key}-shoot-axis-{axis.id}",
            mesh,
            axis.origin[None],
            axis.frame[None],
            np.array([axis.id], np.uint64),
        )
    ]
    group = plan.foliage(path, 100000, retain=False)
    pattern = plan.patterns[len(path)]
    if pattern.short_length:
        radius = 0.0002 if species.key == "picea-abies" else 0.00065
        mesh = axis_mesh(pattern.short_length, 0, radius, radius * 0.7, 2, 6, bark=True)
        result.append(
            wood_product(
                directory,
                f"{species.key}-shoot-shorts",
                mesh,
                group["short_origins"],
                group["short_frames"],
                group["short_ids"],
            )
        )
    for index, name in enumerate(needle_meshes(species, directory, "shoot")):
        choose = group["variants"] == index
        pose_name = name.replace(".npz", "-poses.npz")
        poses(
            directory / pose_name,
            group["origins"][choose],
            group["frames"][choose],
            group["ids"][choose],
            group["scales"][choose],
        )
        result.append(
            {
                "mesh": name,
                "placements": pose_name,
                "instances": int(choose.sum()),
                "surface": "leaf",
                "needle_material": f"needle-{species.needles[0].section}-material.json",
            }
        )
    return result


def produce(species, provenance):
    began = time.perf_counter()
    directory = OUTPUT / species.key
    directory.mkdir(parents=True, exist_ok=True)
    plan = species.plant()
    write_json(directory / "whole-plan.json", plan.recipe())
    write_json(directory / "species.json", species.reference())
    bark = bark_recipe()
    if species.key == "pinus-sylvestris":
        bark.update(
            colour_low=[0.075, 0.034, 0.015, 1], colour_high=[0.27, 0.12, 0.055, 1]
        )
    write_json(directory / "bark.json", bark)
    write_needle_material(
        directory, species.needles[0].section, base_colour=species.colour
    )
    needles, shorts, statistics = stream_foliage(plan, directory, species.key)
    products = [
        *wood_meshes(species, plan, directory),
        *packet_products(species, plan, directory, needles, shorts),
    ]
    index = {
        "version": "complete-needle-packets-1",
        "identity": plan.identity,
        "recipe": "whole-plan.json",
        "summary": plan.summary(),
        "needle_packets": needles,
        "short_packets": shorts,
        "wood_products": [p for p in products if p.get("surface") == "wood"],
        "statistics": statistics,
    }
    write_json(directory / "whole-index.json", index)
    path = tuple(level.branches // 2 for level in species.levels[1:])
    for mode, output, selected in (
        ("whole", products, None),
        ("shoot", near_products(species, plan, directory, path), path),
    ):
        recipe = scene(species, plan, mode, output, provenance, selected)
        recipe.update(
            tables="whole-index.json",
            geometry_lod="third-order physical shoots; bounded complete pose packets",
            crown_calibrated=False,
        )
        write_json(directory / f"{mode}.json", recipe)
    return {
        "species": species.key,
        **plan.summary(),
        **statistics,
        "preparation_s": time.perf_counter() - began,
    }


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    sources = [
        Path(__file__),
        *[
            Path(__file__).with_name(n)
            for n in (
                "conifer_crown_recipes.py",
                "needle_pose_stream.py",
                "europe_conifer_products.py",
                "europe_conifer_recipes.py",
            )
        ],
        *(ROOT / "lib/src/outshine_ref/vegetation").glob("*.py"),
    ]
    provenance = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", *map(str, sources)], text=True
            ).strip()
        ),
        "source_sha256": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sources
        },
    }
    records = []
    for species in crown_conifers():
        record = produce(species, provenance)
        records.append(record)
        print(json.dumps(record), flush=True)
    write_json(OUTPUT / "products.json", {"cases": records, **provenance})


if __name__ == "__main__":
    main()

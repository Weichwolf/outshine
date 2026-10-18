"""Cache complete young-plant/near-shoot products, sharing physical prototypes."""

import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from bark_recipe import bark_recipe
from europe_conifer_recipes import conifers
from needle_material import write_needle_material
from outshine_ref.vegetation.axis_surface import axis_mesh

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/vegetation/europe-conifers"


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n")


def poses(path, origins, frames, ids, scales=None):
    if scales is None:
        scales = np.ones(len(origins))
    np.savez_compressed(path, origins=origins, frames=frames, ids=ids, scales=scales)


def wood_product(directory, name, mesh, origins, frames, ids):
    np.savez_compressed(directory / f"{name}.npz", **mesh)
    poses(directory / f"{name}-poses.npz", origins, frames, ids)
    return {
        "mesh": f"{name}.npz",
        "placements": f"{name}-poses.npz",
        "instances": len(origins),
        "surface": "wood",
        "bark_material": "bark.json",
    }


def prototypes(species, directory, mode):
    result = []
    for index, needle in enumerate(species.needles):
        name = f"{species.key}-needle-{mode}-{index}.npz"
        mesh = needle.mesh(24 if mode == "shoot" else 8, 16 if mode == "shoot" else 8)
        np.savez_compressed(directory / name, **mesh)
        result.append(name)
    return result


def products(species, plan, directory, mode, tables, selected_path):
    result = []
    paths = plan.wood.paths() if selected_path is None else (selected_path,)
    selected_id = None if selected_path is None else plan.wood.axis(selected_path).id
    for path in paths:
        axis, depth = plan.wood.axis(path), len(path)
        base_radius, tip_radius = plan.wood.radii(path)
        mesh = axis_mesh(
            axis.length,
            axis.bend,
            base_radius,
            tip_radius,
            16 if depth == 0 else 8,
            12 if depth == 0 else 8,
            bark=True,
        )
        result.append(
            wood_product(
                directory,
                f"{species.key}-{mode}-axis-{axis.id}",
                mesh,
                axis.origin[None],
                axis.frame[None],
                np.array([axis.id], np.uint64),
            )
        )
    ids = tables["foliage_short_axis_ids"]
    start = 0
    for depth, p in enumerate(plan.patterns):
        axes = plan.wood.axis_counts[depth]
        selected = (ids >= start) & (ids < start + axes)
        if selected_id is not None:
            selected &= ids == selected_id
        start += axes
        if not p or not p.short_length or not selected.any():
            continue
        radius = 0.0002 if species.key == "picea-abies" else 0.00065
        mesh = axis_mesh(p.short_length, 0, radius, radius * 0.7, 2, 6, bark=True)
        result.append(
            wood_product(
                directory,
                f"{species.key}-{mode}-short-{depth}",
                mesh,
                tables["foliage_short_origins"][selected],
                tables["foliage_short_frames"][selected],
                tables["foliage_short_ids"][selected],
            )
        )
    for index, name in enumerate(prototypes(species, directory, mode)):
        selected = tables["foliage_variants"] == index
        if selected_id is not None:
            selected &= tables["foliage_axis_ids"] == selected_id
        pose_name = name.replace(".npz", "-poses.npz")
        poses(
            directory / pose_name,
            tables["foliage_origins"][selected],
            tables["foliage_frames"][selected],
            tables["foliage_ids"][selected],
            tables["foliage_scales"][selected],
        )
        result.append(
            {
                "mesh": name,
                "placements": pose_name,
                "instances": int(selected.sum()),
                "surface": "leaf",
                "needle_material": f"needle-{species.needles[0].section}-material.json",
            }
        )
    return result


def scene(species, plan, mode, products, provenance, path):
    if mode == "shoot":
        axis = plan.wood.axis(path)
        height = axis.length + max(n.length for n in species.needles)
        target = axis.origin + 0.55 * axis.length * axis.frame[:, 1]
        camera = target + height * (axis.frame @ (1.1, 0.3, 1.8))
        fill = target + height * (axis.frame @ (-0.7, -0.3, 0.9))
        camera, target, fill = camera.tolist(), target.tolist(), fill.tolist()
        fov = 31
    else:
        camera, target, fov = [4.2, -7.3, 3.4], [0, 0, 1.7], 27
        fill = [-3, -4, 6]
    return {
        "version": "conifer-preview-1",
        "label": species.latin,
        "species": species.reference(),
        "plant_recipe": "whole-plan.json",
        "tables": "whole-tables.npz",
        "plan_identity": plan.identity,
        "products": products,
        "resolution": [2560, 1440],
        "samples": 64,
        "camera": camera,
        "target": target,
        "fov_y": fov,
        "ground_size_m": 30,
        "fill_position_m": fill,
        "fill_target_m": target,
        "fill_energy_w": 100 * height * height if mode == "shoot" else 800,
        "fill_size_m": height if mode == "shoot" else 4,
        "scope": "young whole plant" if mode == "whole" else "attached needle shoot",
        "geometry_lod": "8x8 needle prototype"
        if mode == "whole"
        else "24x16 needle prototype",
        "lod_image_error_proven": False,
        "selected_axis_path": path,
        **provenance,
    }


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    implementation = [
        Path(__file__),
        Path(__file__).with_name("europe_conifer_recipes.py"),
    ]
    implementation.extend((ROOT / "lib/src/outshine_ref/vegetation").glob("*.py"))
    implementation.extend(
        Path(__file__).with_name(name)
        for name in ("needle_material.py", "bark_recipe.py")
    )
    provenance = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", *map(str, implementation)],
                text=True,
            ).strip()
        ),
        "source_sha256": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in implementation
        },
    }
    cases = []
    for species in conifers():
        directory = OUTPUT / species.key
        directory.mkdir(parents=True, exist_ok=True)
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
        plan = species.plant()
        tables = plan.tables(100000, 100000)
        np.savez_compressed(directory / "whole-tables.npz", **tables)
        write_json(directory / "whole-plan.json", plan.recipe())
        for mode in ("whole", "shoot"):
            began = time.perf_counter()
            path = (
                None
                if mode == "whole"
                else (species.levels[1].branches // 2, species.levels[2].branches // 2)
            )
            summary = plan.summary(() if path is None else path)
            output = products(species, plan, directory, mode, tables, path)
            write_json(
                directory / f"{mode}.json",
                scene(species, plan, mode, output, provenance, path),
            )
            cases.append(
                {
                    "species": species.key,
                    "mode": mode,
                    **summary,
                    "products": len(output),
                    "table_payload_bytes": sum(a.nbytes for a in tables.values()),
                    "preparation_and_save_s": time.perf_counter() - began,
                }
            )
    write_json(OUTPUT / "products.json", {"cases": cases, **provenance})
    print(json.dumps(cases, indent=2))


if __name__ == "__main__":
    main()

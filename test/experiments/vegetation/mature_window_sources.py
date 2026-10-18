"""Requested mature shoots using the same cached closed physical needle meshes."""

import hashlib
import json

import numpy as np
from conifer_whole_products import BASE
from europe_conifer_products import poses, wood_product
from needle_group_source import source_geometry
from outshine_ref.vegetation.axis_surface import axis_mesh


def physical_shoot(plant, path, species, directory):
    directory.mkdir(parents=True, exist_ok=True)
    axis = plant.wood.axis(path)
    base, tip = plant.wood.radii(path)
    products = [
        wood_product(
            directory,
            "axis",
            axis_mesh(axis.length, axis.bend, base, tip, 16, 8),
            axis.origin[None],
            axis.frame[None],
            np.array([axis.id], np.uint64),
        )
    ]
    group = plant.foliage(path, 100000, retain=False)
    pattern = plant.patterns[len(path)]
    if pattern.short_length:
        radius = 0.0002 if species == "picea-abies" else 0.00065
        products.append(
            wood_product(
                directory,
                "shorts",
                axis_mesh(pattern.short_length, 0, radius, radius * 0.7, 2, 6),
                group["short_origins"],
                group["short_frames"],
                group["short_ids"],
            )
        )
    cached = BASE / species
    scene = json.loads((cached / "shoot.json").read_text())
    original = json.loads((cached / scene["plant_recipe"]).read_text())
    assert original["needles"] == plant.recipe()["needles"]
    needles = [p for p in scene["products"] if p["surface"] == "leaf"]
    assert len(needles) == len(plant.needles)
    for variant, product in enumerate(needles):
        selected = group["variants"] == variant
        filename = f"needle-{variant}-poses.npz"
        poses(
            directory / filename,
            group["origins"][selected],
            group["frames"][selected],
            group["ids"][selected],
            group["scales"][selected],
        )
        products.append(
            {
                "mesh": str((cached / product["mesh"]).resolve()),
                "placements": filename,
                "instances": int(selected.sum()),
                "surface": "leaf",
                "needle_material": str((cached / product["needle_material"]).resolve()),
            }
        )
    recipe_path = directory / "plant-recipe.json"
    recipe_path.write_text(json.dumps(plant.recipe(), indent=2) + "\n")
    (directory / "shoot.json").write_text(
        json.dumps(
            {
                "products": products,
                "selected_axis_path": path,
                "plan_identity": plant.identity,
            },
            indent=2,
        )
        + "\n"
    )
    source = source_geometry(directory)
    source["input_sha256"][str(recipe_path)] = hashlib.sha256(
        recipe_path.read_bytes()
    ).hexdigest()
    source["leaf"] = np.array(
        [source["ids"][int(i)][0] == "leaf" for i in source["organ_faces"]]
    )
    main = source["organ_faces"] == 0
    wood = {k: source[k] for k in ("vertices", "faces", "normals")}
    wood.update(faces=source["faces"][main], normals=source["normals"][main])
    assert sum(kind == "leaf" for kind, _ in source["ids"]) == len(group["ids"])
    assert not plant._groups
    return source, wood, axis

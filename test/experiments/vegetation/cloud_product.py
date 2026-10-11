"""Bake complete branch-cloud atlases and geometry from one canonical plant plan."""

import json
import time
from pathlib import Path

import numpy as np
from architecture_experiment import broadleaf
from group_cloud import greedy_cloud, group_geometry
from organ_experiment import write_product
from PIL import Image


def write_scenes(output, plant, product, commit, modified):
    old_directory = output.parent / "architecture"
    original = json.loads((old_directory / "architecture-after.json").read_text())
    assert original["plan_identity"] == plant.identity

    def imported(item):
        result = dict(item)
        prefix = str(Path("..") / old_directory.name)
        for key in ("mesh", "placements"):
            result[key] = f"{prefix}/{item[key]}"
        if "textures" in item:
            result["textures"] = {
                key: f"{prefix}/{value}" for key, value in item["textures"].items()
            }
        return result

    reference = original | {
        "name": "cloud-reference",
        "camera": [20, -40, 15],
        "target": [0, 0, 4.7],
        "products": [imported(item) for item in original["products"]],
        "source_commit": commit,
        "source_modified": modified,
    }
    reduced = reference | {
        "name": "cloud-tree",
        "products": [reference["products"][0], product],
    }
    for recipe in (reference, reduced):
        (output / f"{recipe['name']}.json").write_text(
            json.dumps(recipe, indent=2) + "\n"
        )


def build_tree(output, commit, modified, chart_size=8):
    plant = broadleaf()
    vertices, faces, normals, uvs, images, records = [], [], [], [], [], []
    began = time.perf_counter()
    maximum_displacement = 0.0
    for path in (path for path in plant.paths() if len(path) == 3):
        source = group_geometry(plant, path)
        charts = greedy_cloud(source, size=chart_size)
        first = len(images)
        for chart in charts:
            index = len(images)
            vertices.append(
                chart.mesh["vertices"] @ source["anchor_frame"].T
                + source["anchor_origin"]
            )
            faces.append(chart.mesh["faces"] + index * 4)
            normals.append(chart.mesh["normals"] @ source["anchor_frame"].T)
            uvs.append(chart.mesh["uv"])
            images.append(
                tuple(
                    np.rint(np.clip(image, 0, 1) * 255).astype(np.uint8)
                    for image in (chart.colour, chart.normal)
                )
            )
            maximum_displacement = max(maximum_displacement, chart.displacement_m)
        records.append(
            {
                "path": path,
                "first_chart": first,
                "charts": len(charts),
                "first_organ": int(source["ids"][0]),
                "organs": len(source["ids"]),
            }
        )
    baking_ms = (time.perf_counter() - began) * 1000
    columns = int(np.ceil(np.sqrt(len(images))))
    rows = int(np.ceil(len(images) / columns))
    border, cell = 4, chart_size + 8
    colour = np.zeros((rows * cell, columns * cell, 4), dtype=np.uint8)
    normal = np.zeros_like(colour)
    for index, (rgba, mapped) in enumerate(images):
        x, y = index % columns * cell + border, index // columns * cell + border
        colour[y : y + chart_size, x : x + chart_size] = rgba
        normal[y : y + chart_size, x : x + chart_size] = mapped
        uvs[index] = (uvs[index] * chart_size + (x, y)) / (columns * cell, rows * cell)
    Image.fromarray(np.flipud(colour)).save(output / "cloud-colour.png")
    Image.fromarray(np.flipud(normal)).save(output / "cloud-normal.png")
    mesh = {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
        "uv": np.concatenate(uvs),
    }
    product = write_product(
        output, "cloud-leaves", mesh, np.zeros((1, 3)), np.eye(3)[None], "leaf"
    )
    product.update(
        textures={"colour": "cloud-colour.png", "normal": "cloud-normal.png"}, card=True
    )
    write_scenes(output, plant, product, commit, modified)
    (output / "plant-recipe.json").write_text(
        json.dumps(plant.recipe(), indent=2) + "\n"
    )
    np.testing.assert_array_equal(
        np.concatenate(
            [
                np.arange(
                    record["first_organ"], record["first_organ"] + record["organs"]
                )
                for record in records
            ]
        ),
        np.arange(plant.summary()["leaves"]),
    )
    (output / "cloud-groups.json").write_text(json.dumps(records, indent=2) + "\n")
    (output / "cloud-product.json").write_text(
        json.dumps(
            {
                "version": "greedy-organ-cloud-1",
                "plan_identity": plant.identity,
                "product": product,
                "recipe": "plant-recipe.json",
                "distance_tolerance_m": 0.015,
                "normal_angle_limit_rad": 0.26,
                "chart_size": chart_size,
            },
            indent=2,
        )
        + "\n"
    )
    return {
        "plan_identity": plant.identity,
        "groups": len(records),
        "charts": len(images),
        "leaves": plant.summary()["leaves"],
        "leaf_triangles": len(mesh["faces"]),
        "maximum_projection_displacement_m": maximum_displacement,
        "baking_ms": baking_ms,
        "atlas_resolution": [colour.shape[1], colour.shape[0]],
        "atlas_rgba8_bytes": colour.nbytes + normal.nbytes,
        "atlas_bytes_with_mips_estimate": (colour.nbytes + normal.nbytes) * 4 / 3,
        "atlas_border_texels": border,
        "atlas_scope": "unlit RGBA8 colour/normal charts; theoretical mip estimate, actual Blender GPU allocation unmeasured",
        "mesh_array_bytes": sum(array.nbytes for array in mesh.values()),
        "native_runtime_quality_proven": False,
    }

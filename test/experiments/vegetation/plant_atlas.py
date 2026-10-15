"""Aggregate a complete cached group product into reusable unlit whole-plant views."""

import hashlib
import json
import time

import numpy as np
from cloud_product import pack_atlas, view_snapshot
from foliage_selection import directions
from group_cloud import Chart
from leaf_sheet import average_blocks
from PIL import Image
from plant_depth import depth_mesh
from shoot_plan import normal_frame
from textured_raster import source_image


def load_source(directory):
    manifest = json.loads((directory / "cloud-product.json").read_text())
    scene = json.loads((directory / "views-tree.json").read_text())
    assert scene["plan_identity"] == manifest["plan_identity"]
    recipe = json.loads((directory / "plant-recipe.json").read_text())
    assert (
        hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
        == manifest["plan_identity"]
    )
    paths = [
        directory / name
        for name in (
            "cloud-product.json",
            "views-tree.json",
            "cloud-groups.json",
            "plant-recipe.json",
        )
    ]
    mesh_path = directory / manifest["cache_mesh"]
    paths.append(mesh_path)
    with np.load(mesh_path, allow_pickle=False) as stored:
        mesh = {key: stored[key] for key in stored.files}
    records = json.loads((directory / "cloud-groups.json").read_text())
    maps = []
    for key in ("colour", "normal"):
        filename = manifest["product"]["textures"][key]
        path = directory / filename
        paths.append(path)
        with Image.open(path) as image:
            maps.append(np.flipud(np.asarray(image.convert("RGBA"))))
    product = scene["products"][0]
    wood_path = directory / product["mesh"]
    paths.append(wood_path)
    with np.load(wood_path, allow_pickle=False) as stored:
        wood = {key: stored[key] for key in stored.files}
    for item in (product, manifest["product"]):
        path = directory / item["placements"]
        paths.append(path)
        with np.load(path, allow_pickle=False) as stored:
            np.testing.assert_array_equal(stored["origins"], np.zeros((1, 3)))
            np.testing.assert_array_equal(stored["frames"], np.eye(3)[None])
    for array in (*mesh.values(), *wood.values(), *maps):
        array.setflags(write=False)
    return {
        "mesh": mesh,
        "records": records,
        "maps": maps,
        "wood": wood,
        "plan_identity": manifest["plan_identity"],
        "input_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths
        },
        "recipe": recipe,
    }


def projection(points, direction, padding=1.04):
    direction = np.asarray(direction, dtype=float)
    if (
        direction.shape != (3,)
        or not np.all(np.isfinite(direction))
        or np.linalg.norm(direction) == 0
    ):
        raise ValueError("finite nonzero projection direction required")
    direction = direction / np.linalg.norm(direction)
    first = int(np.argmin(np.abs(direction)))
    forward = np.eye(3)[first] - direction[first] * direction
    frame = normal_frame(forward, direction)
    projected = points @ frame
    lower, upper = projected[:, :2].min(axis=0), projected[:, :2].max(axis=0)
    centre, extent = (lower + upper) / 2, (upper - lower) * padding
    return frame, centre - extent / 2, centre + extent / 2, projected[:, 2].mean()


def draw_source(source, direction, size=128, bounds=None):
    mesh, _ = view_snapshot(source["mesh"], source["records"], view_direction=direction)
    points = np.concatenate((mesh["vertices"], source["wood"]["vertices"]))
    frame, lower, upper, offset = projection(points, direction)
    if bounds is not None:
        lower, upper = bounds
    image = source_image(
        mesh, source["maps"], source["wood"], frame, lower, upper, size
    )
    return image, (frame, lower, upper, offset)


def bake_chart(source, direction, size=128, grid=8, *, draw=None):
    image, (frame, lower, upper, _offset) = (
        draw_source(source, direction, size * 2)
        if draw is None
        else draw(direction, size * 2)
    )
    image = average_blocks(image, 2)
    normal = image[..., 4:7] @ frame
    encoded = np.concatenate(
        ((normal + image[..., 3, None]) / 2, image[..., 3, None]), axis=-1
    )
    mesh = depth_mesh(image, frame, lower, upper, grid)
    return Chart(mesh, image[..., :4], encoded, np.array([0]), frame, 0, 0), image[
        ..., 7
    ]


def bake_product(source, output, count=64, size=128, grid=8, *, draw=None):
    if type(size) is not int or size not in (32, 64, 128, 256):
        raise ValueError("bounded whole-plant view size required")
    began = time.perf_counter()
    views = [
        bake_chart(source, direction, size, grid, draw=draw)
        for direction in directions(count)
    ]
    output.mkdir(parents=True, exist_ok=True)
    (output / "plant-recipe.json").write_text(
        json.dumps(source["recipe"], indent=2) + "\n"
    )
    uvs = [chart.mesh["uv"].copy() for chart, _ in views]
    images = [
        tuple(
            np.uint8(np.rint(np.clip(image, 0, 1) * 255))
            for image in (chart.colour, chart.normal)
        )
        for chart, _ in views
    ]
    stats = pack_atlas(output, images, uvs, size)
    columns = int(np.ceil(np.sqrt(count)))
    width, height = stats["atlas_resolution"]
    material = np.zeros((height, width), dtype=np.uint8)
    for index, (_, coverage) in enumerate(views):
        x, y = index % columns * (size + 8) + 4, index // columns * (size + 8) + 4
        material[y : y + size, x : x + size] = np.uint8(
            np.rint(np.clip(coverage, 0, 1) * 255)
        )
    Image.fromarray(np.flipud(material)).save(output / "plant-leaf-weight.png")
    mesh = {
        "vertices": np.concatenate([chart.mesh["vertices"] for chart, _ in views]),
        "faces": np.concatenate(
            [
                chart.mesh["faces"] + index * (grid + 1) ** 2
                for index, (chart, _) in enumerate(views)
            ]
        ),
        "normals": np.concatenate([chart.mesh["normals"] for chart, _ in views]),
        "uv": np.concatenate(uvs),
        "directions": np.array([chart.frame[:, 2] for chart, _ in views]),
    }
    np.savez_compressed(output / "plant-views-mesh.npz", **mesh)
    stats["atlas_scope"] = (
        "unlit premultiplied RGBA8 colour/normal plus R8 leaf coverage; theoretical mip estimate, actual GPU allocation unmeasured"
    )
    stats["atlas_material_r8_bytes"] = material.nbytes
    stats["atlas_base_bytes"] = stats["atlas_rgba8_bytes"] + material.nbytes
    stats["atlas_bytes_with_mips_estimate"] = stats["atlas_base_bytes"] * 4 / 3
    points = source.get("bounds_points")
    if points is None:
        points = np.concatenate(
            (source["mesh"]["vertices"], source["wood"]["vertices"])
        )
    result = stats | {
        "version": "whole-plant-views-1",
        "plan_identity": source["plan_identity"],
        "recipe": "plant-recipe.json",
        "views": count,
        "chart_size": size,
        "cached_triangles": count * 2 * grid**2,
        "draw_triangles": 2 * grid**2,
        "vertices_per_view": (grid + 1) ** 2,
        "mesh_grid": grid,
        "generation_ms": (time.perf_counter() - began) * 1000,
        "mesh": "plant-views-mesh.npz",
        "textures": {
            "colour": "cloud-colour.png",
            "normal": "cloud-normal.png",
            "leaf_weight": "plant-leaf-weight.png",
        },
        "input_sha256": source["input_sha256"],
        "source_scope": source.get(
            "source_scope",
            "cached group views and wood; no organ placement or meshing, cold recipe-only construction remains open",
        ),
        "bounds_m": [points.min(axis=0).tolist(), points.max(axis=0).tolist()],
        "render_bounds_m": [
            mesh["vertices"].min(axis=0).tolist(),
            mesh["vertices"].max(axis=0).tolist(),
        ],
    }
    (output / "plant-product.json").write_text(json.dumps(result, indent=2) + "\n")
    return result

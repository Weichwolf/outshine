"""One complete 10x10-m vegetation tile from cached plants and shared contacts."""

import hashlib
import json
import os
import subprocess
from pathlib import Path

import numpy as np
from europe_conifer_products import OUTPUT, poses, write_json
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.vegetation import Blade
from outshine_ref.vegetation.shoot_plan import normal_frame
from scipy.spatial.transform import Rotation

SPECIES = ("picea-abies", "pinus-sylvestris", "pinus-cembra", "larix-decidua")


def terrain(directory):
    x, y = np.meshgrid(np.linspace(-5, 5, 17), np.linspace(-5, 5, 17), indexing="ij")
    z = 0.12 * np.sin(0.43 * x + 0.3) * np.cos(0.37 * y - 0.2)
    vertices = np.column_stack((x.ravel(), y.ravel(), z.ravel()))
    index = np.arange(x.size).reshape(x.shape)
    a, b, c, d = index[:-1, :-1], index[1:, :-1], index[1:, 1:], index[:-1, 1:]
    faces = np.stack(
        (np.stack((a, b, c), axis=-1), np.stack((a, c, d), axis=-1)), axis=-2
    ).reshape(-1, 3)
    mesh = Mesh(vertices, faces)
    normal = np.cross(
        vertices[faces[:, 1]] - vertices[faces[:, 0]],
        vertices[faces[:, 2]] - vertices[faces[:, 0]],
    )
    normal /= np.linalg.norm(normal, axis=1, keepdims=True)
    np.savez_compressed(
        directory / "tile-ground.npz",
        vertices=vertices,
        faces=faces,
        normals=np.repeat(normal[:, None], 3, axis=1).reshape(-1, 3),
    )
    poses(
        directory / "tile-ground-poses.npz",
        np.zeros((1, 3)),
        np.eye(3)[None],
        np.array([0], np.uint64),
    )
    return SurfaceSampler(mesh), {
        "mesh": "tile-ground.npz",
        "placements": "tile-ground-poses.npz",
        "instances": 1,
        "surface": "ground",
    }


def plants(directory, roots, source_directory=OUTPUT):
    products, plans = [], []
    rotations = Rotation.from_euler(
        "z", np.array((0.27, 1.16, -0.7, 2.1))[:, None]
    ).as_matrix()
    for index, key in enumerate(SPECIES):
        source = source_directory / key
        recipe = json.loads((source / "whole.json").read_text())
        for product in recipe["products"]:
            result = dict(product)
            for field in ("mesh", "bark_material", "needle_material"):
                if field in result:
                    result[field] = os.path.relpath(source / result[field], directory)
            with np.load(source / product["placements"]) as data:
                name = "tile-" + Path(product["placements"]).name
                poses(
                    directory / name,
                    data["origins"] @ rotations[index].T + roots[index],
                    np.einsum("ij,njk->nik", rotations[index], data["frames"]),
                    data["ids"],
                    data["scales"],
                )
            result.update(
                placements=name, plant_id=index, plant_identity=recipe["plan_identity"]
            )
            products.append(result)
        plans.append(
            {
                "id": index,
                "source_id": f"procedural:swiss-conifer-tile:{index}",
                "species": recipe["species"],
                "identity": recipe["plan_identity"],
                "origin_m": roots[index].tolist(),
                "frame": rotations[index].tolist(),
                "recipe": os.path.relpath(source / "whole-plan.json", directory),
                "tables": os.path.relpath(source / recipe["tables"], directory),
            }
        )
    poses(directory / "tile-roots.npz", roots, rotations, np.arange(4, dtype=np.uint64))
    write_json(
        directory / "tile-plans.json",
        {
            "version": "conifer-tile-plan-1",
            "tile_size_m": [10, 10],
            "terrain": "own procedural height fixture, no DEM source",
            "plants": plans,
        },
    )
    return products, plans


def grass(directory, sampler, roots):
    rng = np.random.default_rng(913)
    clumps = rng.uniform(-4.8, 4.8, (512, 2))
    clumps = clumps[
        (np.linalg.norm(clumps[:, None] - roots[None, :, :2], axis=-1) > 0.24).all(
            axis=1
        )
    ]
    xy = np.repeat(clumps, 12, axis=0) + rng.uniform(
        -0.065, 0.065, (len(clumps) * 12, 2)
    )
    samples = sampler.sample(xy)
    if any(samples.reasons):
        raise AssertionError("grass contacts must resolve on the selected tile surface")
    angle = rng.uniform(-np.pi, np.pi, len(xy))
    frames = normal_frame(
        np.broadcast_to((0, 0, 1), (len(xy), 3)),
        np.column_stack((np.cos(angle), np.sin(angle), np.zeros(len(xy)))),
    )
    scales = rng.uniform(0.72, 1.15, len(xy))
    products = []
    for variant, length in enumerate((0.13, 0.18, 0.23)):
        blade = Blade(
            length=length,
            width=0.0045,
            widest=0.33,
            lobes=0,
            lobe_depth=0,
            bend=0.3,
            fold=0.25,
            fold_power=1,
            twist=0.15,
            petiole=0,
            base_fill=0.12,
        )
        name = f"tile-grass-{variant}"
        np.savez_compressed(directory / f"{name}.npz", **blade.mesh(6))
        ids = np.arange(len(xy), dtype=np.uint64)[variant::3]
        poses(
            directory / f"{name}-poses.npz",
            samples.points_enu_m[variant::3],
            frames[variant::3],
            ids,
            scales[variant::3],
        )
        products.append(
            {
                "mesh": f"{name}.npz",
                "placements": f"{name}-poses.npz",
                "instances": len(ids),
                "surface": "grass",
            }
        )
    return products, dict(samples.report)


def make_tile(source_directory=OUTPUT):
    directory = source_directory / "tile"
    directory.mkdir(parents=True, exist_ok=True)
    sampler, ground = terrain(directory)
    xy = np.array(((-2.2, -1.4), (1.6, -2.2), (-1.2, 2), (2.1, 1.2)))
    contacts = sampler.sample(xy)
    if any(contacts.reasons):
        raise AssertionError("all tree roots require an actual terrain contact")
    products, plans = plants(directory, contacts.points_enu_m, source_directory)
    blades, report = grass(directory, sampler, contacts.points_enu_m)
    recipe = {
        "version": "conifer-preview-1",
        "label": "Schweizer Nadelgehölze · junge Bestandskachel",
        "species": {"latin": " · ".join(p["species"]["latin"] for p in plans)},
        "scope": "10x10-m procedural young-plant collection, not an ecological forest recipe",
        "products": [ground, *products, *blades],
        "plant_recipe": "tile-plans.json",
        "tables": "tile-roots.npz",
        "plan_identity": hashlib.sha256(
            (directory / "tile-plans.json").read_bytes()
        ).hexdigest(),
        "camera": [9, -13, 10],
        "target": [0, 0, 1.15],
        "fov_y": 39,
        "resolution": [2560, 1440],
        "samples": 64,
        "ground_size_m": 30,
        "own_ground": True,
        "fill_position_m": [-4, -6, 7],
        "fill_target_m": [0, 0, 1.5],
        "fill_energy_w": 1000,
        "fill_size_m": 5,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", str(Path(__file__))], text=True
            ).strip()
        ),
        "lod_image_error_proven": False,
        "grass_contact_report": report,
    }
    write_json(directory / "tile.json", recipe)
    print(
        json.dumps(
            {
                "products": len(recipe["products"]),
                "plants": len(plans),
                "grass": sum(p["instances"] for p in blades),
                "terrain_samples": dict(contacts.report),
            },
            indent=2,
        )
    )


def main():
    make_tile()


if __name__ == "__main__":
    main()

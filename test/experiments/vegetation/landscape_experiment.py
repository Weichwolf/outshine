"""Build three fixed landscape form studies from conserved, stable stand plans.

Crowns are diagnostic volumes. This is not a finished foliage/forest LOD product.
"""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from crown_surface import Crown
from stand_plan import StandPlan
from wood_mesh import branch_tubes

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def ground_height(x, y):
    return 1.2 * np.sin(0.03 * x) * np.cos(0.025 * y)


def conifer_mesh(height, spread, bole, sides=24):
    if (
        not all(np.isfinite(value) for value in (height, spread, bole))
        or height <= 0
        or spread <= 0
        or not 0 <= bole < 1
    ):
        raise ValueError(
            "positive finite crown dimensions and bole fraction in [0,1) required"
        )
    if not isinstance(sides, (int, np.integer)) or sides < 3:
        raise ValueError("at least three radial sides required")
    vertices, faces, normals = [], [], []
    angles = np.arange(sides) * 2 * np.pi / sides
    radial = np.column_stack((np.cos(angles), np.sin(angles), np.zeros(sides)))
    edge, following = np.arange(sides), (np.arange(sides) + 1) % sides
    for base, tip, width in ((0, 0.72, 1), (0.25, 0.88, 0.8), (0.48, 1, 0.58)):
        radius = spread * width / 2
        lower = height * (bole + (1 - bole) * base)
        upper = height * (bole + (1 - bole) * tip)
        ring = radial * radius + (0, 0, lower)
        points = np.vstack((ring, (0, 0, upper), (0, 0, lower)))
        local = np.vstack(
            (
                np.column_stack((edge, following, np.full(sides, sides))),
                np.column_stack((np.full(sides, sides + 1), following, edge)),
            )
        )
        side = radial + (0, 0, radius / (upper - lower))
        side /= np.linalg.norm(side, axis=1)[:, None]
        apex = side + np.roll(side, -1, axis=0)
        apex /= np.linalg.norm(apex, axis=1)[:, None]
        corners = np.concatenate(
            (
                np.stack((side, np.roll(side, -1, axis=0), apex), axis=1),
                np.tile((0, 0, -1), (sides, 3, 1)),
            )
        )
        faces.append(local + sum(len(part) for part in vertices))
        vertices.append(points)
        normals.append(corners.reshape(-1, 3))
    return {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces).astype(np.int32),
        "normals": np.concatenate(normals),
        "corner_normals": True,
    }


def validate_mesh(mesh):
    triangle = mesh["vertices"][mesh["faces"]]
    cross = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    assert np.all(np.isfinite(triangle)) and np.all(np.linalg.norm(cross, axis=1) > 0)
    normal = (
        mesh["normals"].reshape(-1, 3, 3)
        if mesh.get("corner_normals")
        else mesh["normals"][mesh["faces"]]
    )
    np.testing.assert_allclose(np.linalg.norm(normal, axis=2), 1, atol=1e-12)
    assert np.all(np.sum(cross * normal.mean(axis=1), axis=1) > 0)
    edges = np.concatenate(
        (mesh["faces"][:, [0, 1]], mesh["faces"][:, [1, 2]], mesh["faces"][:, [2, 0]])
    )
    _, counts = np.unique(np.sort(edges, axis=1), axis=0, return_counts=True)
    assert np.all(counts == 2)


def dimensions(recipe):
    if recipe["profiles"]:
        profiles = [
            json.loads((ROOT / "src/assets/world/species" / f"{name}.json").read_text())
            for name in recipe["profiles"]
        ]
        return [
            (
                profile["height_m"],
                profile["spread_m"],
                profile["bole_frac"],
                profile["dbh_cm"] / 200,
            )
            for profile in profiles
        ]
    return recipe["dimensions"]


def build(recipe, output):
    began = time.perf_counter()
    sizes = dimensions(recipe)
    plan = StandPlan(
        recipe["counts"],
        [size[0] for size in sizes],
        [size[1] for size in sizes],
        size=recipe["size_m"],
        levels=3,
        seed=recipe["seed"],
    )
    pending, site_blocks = [plan.root], []
    while pending:
        cell = pending.pop()
        children = plan.children(cell)
        if children:
            pending.extend(reversed(children))
        else:
            sites = plan.sites(cell)
            np.testing.assert_array_equal(
                sites["positions"], plan.sites(cell)["positions"]
            )
            site_blocks.append(sites)
    positions = np.concatenate([sites["positions"] for sites in site_blocks])
    kinds = np.concatenate([sites["kinds"] for sites in site_blocks])
    ids = np.array(
        [key for sites in site_blocks for key in sites["ids"]], dtype=np.int64
    )
    np.testing.assert_array_equal(
        np.bincount(kinds, minlength=len(sizes)), recipe["counts"]
    )
    assert len(np.unique(ids, axis=0)) == sum(recipe["counts"])
    positions[:, 2] = ground_height(positions[:, 0], positions[:, 1])
    variants = np.array(
        [
            int.from_bytes(
                hashlib.blake2b(key.tobytes(), digest_size=8).digest(), "little"
            )
            % 4
            for key in ids
        ]
    )
    np.savez(
        output / f"{recipe['id']}-sites.npz",
        positions=positions,
        kinds=kinds,
        variants=variants,
        ids=ids,
    )
    products = []
    for kind, (height, spread, bole, radius) in enumerate(sizes):
        for variant in range(4):
            scale = 0.85 + variant * 0.1
            if recipe["architecture"] == "conifer":
                crown = conifer_mesh(height * scale, spread * scale, bole)
            else:
                shape = Crown(
                    (
                        spread * scale / 2,
                        spread * scale / 2,
                        height * scale * (1 - bole) / 2,
                    ),
                    (0, 0, height * scale * (1 + bole) / 2),
                    recipe["seed"] + variant,
                )
                crown = shape.mesh(4) | {"corner_normals": False}
            validate_mesh(crown)
            stem = branch_tubes(
                np.array([[0, 0, -0.12], [0, 0, height * scale * (bole + 0.15)]]),
                np.array([-1, 0]),
                np.array([radius, radius * scale]),
            )
            wood = dict(zip(("vertices", "faces", "normals"), stem)) | {
                "corner_normals": True
            }
            for part, mesh in (("crown", crown), ("stem", wood)):
                filename = f"{recipe['id']}-{kind}-{variant}-{part}.npz"
                np.savez(output / filename, **mesh)
                products.append(
                    {
                        "kind": kind,
                        "variant": variant,
                        "part": part,
                        "mesh": filename,
                        "triangles": len(mesh["faces"]),
                    }
                )
    scene = recipe | {
        "sites": f"{recipe['id']}-sites.npz",
        "products": products,
        "resolution": [2560, 1440],
        "fov_y": 48.0,
        "samples": 16,
    }
    (output / f"{recipe['id']}.json").write_text(json.dumps(scene, indent=2) + "\n")
    return {
        "scene": recipe["id"],
        "plants": len(positions),
        "shared_meshes": len(products),
        "generation_ms": (time.perf_counter() - began) * 1000,
        "layout_identity": plan.layout_identity,
        "stable_sites": True,
        "counts_conserved": True,
        "positive_closed_crown_meshes": True,
        "foliage_lod_quality_proven": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    recipes = json.loads((HERE / "landscapes.json").read_text())
    results = [build(recipe, args.output) for recipe in recipes]
    report = {
        "cases": results,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    str(HERE / "landscape_experiment.py"),
                    str(HERE / "landscapes.json"),
                    str(HERE / "crown_surface.py"),
                    str(HERE / "stand_plan.py"),
                    str(HERE / "wood_mesh.py"),
                    "src/assets/world/species",
                ],
                cwd=ROOT,
                text=True,
            ).strip()
        ),
    }
    (args.output / "landscapes.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(results, ensure_ascii=False))


if __name__ == "__main__":
    main()

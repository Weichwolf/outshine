"""Small physical plant products for four flat research tiles, no GIS inputs."""

import argparse
import json
from dataclasses import replace
from pathlib import Path

import numpy as np
from botanical_profiles import plant_recipe
from outshine_ref.vegetation import Blade, PlantPlan
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh


class Assembly:
    def __init__(self):
        self.parts = []
        self.vertices = 0

    def add(self, mesh, origins, frames, surface):
        if not len(origins):
            return
        v, f = mesh["vertices"], mesh["faces"]
        vertices = np.einsum("nij,vj->nvi", frames, v) + origins[:, None]
        faces = f[None] + np.arange(len(origins))[:, None, None] * len(v)
        normals = np.einsum("nij,cj->nci", frames, mesh["normals"].reshape(-1, 3))
        uv = mesh.get("uv", np.zeros((len(v), 2)))[f.ravel()]
        self.parts.append(
            {
                "vertices": vertices.reshape(-1, 3),
                "faces": faces.reshape(-1, 3) + self.vertices,
                "normals": normals.reshape(-1, 3),
                "uv": np.tile(uv, (len(origins), 1)),
                "surface": np.full(len(origins) * len(f), surface, dtype=np.uint8),
            }
        )
        self.vertices += vertices.size // 3

    def finish(self):
        return {
            key: np.concatenate([p[key] for p in self.parts]) for key in self.parts[0]
        }


def flower():
    # An authored four-lobed bell form, not an accepted Calluna species model.
    angles = np.arange(8) * (2 * np.pi / 8)
    t = np.array([0, 0.6, 1.0])
    radius = np.array([0.001, 0.003, 0.0028])
    v = np.stack(
        np.broadcast_arrays(
            radius[:, None] * np.cos(angles),
            radius[:, None] * np.sin(angles),
            t[:, None] * 0.009 + (t[:, None] ** 4) * 0.001 * np.cos(4 * angles),
        ),
        axis=-1,
    ).reshape(-1, 3)
    a = (np.arange(2)[:, None] * 8 + np.arange(8)[None]).ravel()
    b = (a // 8) * 8 + (a + 1) % 8
    f = np.concatenate((np.c_[a, b, a + 8], np.c_[b, b + 8, a + 8]))
    p = v[f]
    n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    n /= np.linalg.norm(n, axis=1, keepdims=True)
    return {"vertices": v, "faces": f, "normals": np.repeat(n, 3, axis=0)}


def woody(key, seed):
    plant = plant_recipe("fagus" if key == "beech" else "quercus", seed)
    a, b, c = plant.levels
    if key == "heather":
        levels = (
            replace(a, length=0.48, base_radius=0.009, tip_radius=0.003),
            replace(
                b,
                branches=7,
                length=0.26,
                base_radius=0.002,
                tip_radius=0.0008,
                first=0.06,
                leaf_internode=0.015,
                angle=0.9,
            ),
            replace(
                c,
                branches=4,
                length=0.16,
                base_radius=0.0005,
                tip_radius=0.00015,
                leaf_internode=0.009,
                arrangement="opposite",
            ),
        )
        blades = (
            Blade(length=0.012, width=0.003, lobes=0, lobe_depth=0, petiole=0.001),
        )
    else:
        levels = (
            replace(a, length=6.5, base_radius=0.14, tip_radius=0.045),
            replace(
                b,
                branches=12,
                length=2.5,
                base_radius=0.035,
                tip_radius=0.01,
                first=0.3,
                leaf_internode=0.025,
                length_shape="rounded",
            ),
            replace(
                c,
                branches=7,
                length=1.15,
                base_radius=0.007,
                tip_radius=0.0008,
                leaf_internode=0.012,
                length_shape="rounded",
            ),
        )
        blades = plant.blades
    plan = PlantPlan(seed, levels, blades)
    data = plan.tables(10000, 100000)
    assembly = Assembly()
    for state in plan.expand(10000, 100000)["axes"]:
        level = levels[len(state.path)]
        mesh = axis_mesh(
            state.length,
            state.bend,
            level.base_radius,
            level.tip_radius,
            6 if len(state.path) < 2 else 3,
            6,
        )
        assembly.add(mesh, state.origin[None], state.frame[None], 0)
    for index, blade in enumerate(blades):
        selected = data["leaf_variants"] == index
        assembly.add(
            blade.mesh(3, 1),
            data["leaf_origins"][selected],
            data["leaf_frames"][selected],
            1 + index % 3,
        )
    if key == "heather":
        ends = data["leaf_origins"][::3]
        frames = data["leaf_frames"][::3]
        assembly.add(flower(), ends, frames, 4)
    return assembly.finish()


def cached_conifer(directory):
    recipe = json.loads((directory / "whole.json").read_text())
    assembly = Assembly()
    for product in recipe["products"]:
        with np.load(directory / product["mesh"]) as stored:
            mesh = {key: stored[key] for key in stored.files}
        with np.load(directory / product["placements"]) as poses:
            scales = (
                poses["scales"] if "scales" in poses else np.ones(len(poses["origins"]))
            )
            if scales.ndim == 1:
                scales = np.repeat(scales[:, None], 3, axis=1)
            # Product scales are uniform in these saved whole-plant products.
            for scale in np.unique(scales, axis=0):
                selected = np.all(scales == scale, axis=1)
                scaled = dict(mesh)
                scaled["vertices"] = mesh["vertices"] * scale
                normals = mesh["normals"] / scale
                scaled["normals"] = normals / np.linalg.norm(
                    normals, axis=-1, keepdims=True
                )
                assembly.add(
                    scaled,
                    poses["origins"][selected],
                    poses["frames"][selected],
                    0 if product["surface"] == "wood" else 2,
                )
    return assembly.finish()


def build(output, conifer):
    output.mkdir(parents=True, exist_ok=True)
    for key in ("oak", "beech", "heather"):
        np.savez_compressed(output / (key + ".npz"), **woody(key, 718))
    np.savez_compressed(output / "pine.npz", **cached_conifer(conifer))
    for index in range(8):
        mesh = graminoid_tuft_mesh(17 + index, segments=3)
        mesh["uv"] = mesh["uv"][mesh["faces"].ravel()]
        mesh["surface"] = np.full(len(mesh["faces"]), 1 + index % 3, dtype=np.uint8)
        np.savez_compressed(output / f"grass-{index}.npz", **mesh)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--conifer", type=Path, required=True)
    args = parser.parse_args()
    build(args.output, args.conifer)

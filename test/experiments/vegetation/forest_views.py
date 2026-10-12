"""Select whole-plant camera and light views for unchanged cached stand sites."""

import argparse
import json
import os
import subprocess
from pathlib import Path

import numpy as np
from landscape_experiment import dimensions
from preview_lighting import LANDSCAPE_SUN_ROTATION
from scipy.spatial.transform import Rotation


def choices(mesh, origins, scales, angles, pivot, eye=None, direction=None):
    if (eye is None) == (direction is None):
        raise ValueError("one perspective eye or orthographic direction required")
    rays = (
        (np.asarray(eye) - origins)
        if eye is not None
        else np.broadcast_to(direction, origins.shape)
    )
    c, s = np.cos(angles), np.sin(angles)
    local = (
        np.column_stack(
            (
                c * rays[:, 0] + s * rays[:, 1],
                -s * rays[:, 0] + c * rays[:, 1],
                rays[:, 2],
            )
        )
        / scales
    )
    if eye is not None:
        local -= pivot
    if not np.all(np.isfinite(local)) or np.any(np.linalg.norm(local, axis=1) == 0):
        raise ValueError("finite nonzero transformed directions required")
    normals = (
        mesh["directions"]
        if "directions" in mesh
        else mesh["normals"].reshape(-1, 6, 3)[:, 0]
    )
    return np.argmax(local @ normals.T, axis=1)


def write_products(output, atlas, mesh, manifest, sites):
    products = []
    vertices, triangles = (
        manifest.get("vertices_per_view", 4),
        manifest["draw_triangles"],
    )
    for role in ("camera", "sun"):
        for index in np.unique(sites[f"{role}_views"]):
            filename = f"plant-view-{index}.npz"
            np.savez_compressed(
                output / filename,
                vertices=mesh["vertices"][index * vertices : (index + 1) * vertices],
                faces=mesh["faces"][index * triangles : (index + 1) * triangles]
                - index * vertices,
                normals=mesh["normals"][
                    index * triangles * 3 : (index + 1) * triangles * 3
                ],
                uv=mesh["uv"][index * vertices : (index + 1) * vertices],
                corner_normals=True,
            )
            products.append(
                {
                    "part": "plant_view",
                    "mesh": filename,
                    "triangles": triangles,
                    "view": int(index),
                    "selection": role,
                    "card": True,
                    "textures": {
                        key: str(Path(os.path.relpath(atlas, output)) / value)
                        for key, value in manifest["textures"].items()
                    },
                    "pass_role": "camera" if role == "camera" else "shadow",
                    **({"shadow_light": "sun"} if role == "sun" else {}),
                }
            )
    return products


def build_scene(source, atlas, output):
    recipe = json.loads(source.read_text())
    manifest = json.loads((atlas / "plant-product.json").read_text())
    assert manifest["version"] == "whole-plant-views-1"
    with np.load(atlas / manifest["mesh"], allow_pickle=False) as stored:
        mesh = {key: stored[key] for key in stored.files}
    with np.load(source.parent / recipe["sites"], allow_pickle=False) as stored:
        sites = {key: stored[key] for key in stored.files}
    lower, upper = np.asarray(manifest["bounds_m"])
    span = upper - lower
    sizes = dimensions(recipe)
    scales = np.array(
        [
            (spread / max(span[:2]), spread / max(span[:2]), height / upper[2])
            for height, spread, _, _ in sizes
        ]
    )[sites["kinds"]]
    scales *= (0.85 + sites["variants"] * 0.1)[:, None]
    angles = np.remainder(
        np.asarray(sites["ids"][:, -1], float) * 2.399963229728653
        + sites["variants"] * 1.1
        + sites["ids"][:, 1] * 0.7
        + sites["ids"][:, 2] * 1.3,
        2 * np.pi,
    )
    sites["scales"], sites["angles"] = scales, angles
    queries = {
        "camera": {"eye": recipe["camera"]},
        "sun": {
            "direction": Rotation.from_euler("xyz", LANDSCAPE_SUN_ROTATION).apply(
                (0, 0, 1)
            )
        },
    }
    for name, query in queries.items():
        sites[f"{name}_views"] = choices(
            mesh, sites["positions"], scales, angles, (lower + upper) / 2, **query
        )
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output / "forest-sites.npz", **sites)
    recipe = recipe | {
        "id": "temperate-views",
        "sites": "forest-sites.npz",
        "products": write_products(output, atlas, mesh, manifest, sites),
        "plant_plan_identity": manifest["plan_identity"],
        "plant_product": str((atlas / "plant-product.json").resolve()),
        "plant_form_scope": "one affine broadleaf prototype mapped to catalog dimensions; species-specific organs and architecture remain open",
        "sun_rotation": LANDSCAPE_SUN_ROTATION,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
                text=True,
            ).strip()
        ),
    }
    (output / "temperate-views.json").write_text(json.dumps(recipe, indent=2) + "\n")
    return {
        "sites": len(sites["positions"]),
        "camera_triangles": len(sites["positions"]) * manifest["draw_triangles"],
        "shadow_triangles": len(sites["positions"]) * manifest["draw_triangles"],
        "products": len(recipe["products"]),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        type=Path,
        default=Path("build/experiments/vegetation/landscapes/temperate.json"),
    )
    parser.add_argument(
        "--atlas", type=Path, default=Path("build/experiments/vegetation/plant-atlas")
    )
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/forest-views")
    )
    args = parser.parse_args()
    print(json.dumps(build_scene(args.source, args.atlas, args.output), indent=2))


if __name__ == "__main__":
    main()

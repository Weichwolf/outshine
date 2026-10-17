"""Reuse the existing complete Pinus recipe and its physical product emitter."""

import json

import numpy as np
import shapely
from camera_height_inputs import digest
from europe_conifer_products import products
from europe_conifer_recipes import conifers
from outshine_ref.contracts import BuildingFootprint, HeightInterval, Mesh
from outshine_ref.vegetation import NeedledPlant
from recipe_graph_data import plain
from recipe_plant_detail import physical_detail
from recipe_plant_operators import prepared
from recipe_plant_values import seed_input


def expand(directory, rows):
    vertices, faces, leaf, offset = [], [], [], 0
    counts = {"wood": 0, "leaf": 0}
    for row in rows:
        with np.load(directory / row["mesh"], allow_pickle=False) as saved:
            points, triangles = saved["vertices"], saved["faces"]
        with np.load(directory / row["placements"], allow_pickle=False) as poses:
            origin, frame, scale = poses["origins"], poses["frames"], poses["scales"]
            if len(origin) != row["instances"] or len(poses["ids"]) != len(origin):
                raise ValueError("complete source-bound physical poses required")
            actual = np.einsum("nij,vj->nvi", frame, points)
            actual = actual * scale[:, None, None] + origin[:, None, :]
        n = len(origin)
        if not n:
            continue
        if not np.isfinite(actual).all():
            raise ValueError("finite complete plant geometry required")
        vertices.append(actual.reshape(-1, 3))
        indices = triangles[None] + np.arange(n)[:, None, None] * len(points) + offset
        faces.append(indices.reshape(-1, 3))
        leaf.append(np.full(n * len(triangles), row["surface"] == "leaf"))
        offset += n * len(points)
        counts[row["surface"]] += n
    return Mesh(np.vstack(vertices), np.vstack(faces)), np.concatenate(leaf), counts


def prototype(output):
    output.mkdir()
    run = prepared().evaluate({"seed": seed_input(718)}, outputs=("plant", "detail"))
    recipe = plain(run.outputs["plant"].value)
    detail = run.outputs["detail"].value
    facts = physical_detail(detail)
    species = next(s for s in conifers() if s.key == "pinus-sylvestris")
    if species.plant(718).recipe() != recipe:
        raise ValueError("existing product emitter and actual recipe disagree")
    plan = NeedledPlant.from_recipe(recipe)
    rows = products(species, plan, output, "whole", detail.tables, None)
    mesh, leaf, counts = expand(output, rows)
    if counts["leaf"] != facts["needles"]:
        raise ValueError("complete actual needle geometry required")
    np.savez_compressed(
        output / "complete.npz",
        vertices=mesh.vertices,
        faces=mesh.triangles,
        leaf_faces=leaf,
    )
    report = {
        "recipe": recipe,
        "definition": run.definition_fingerprint,
        "facts": facts,
        "products": rows,
        "instances": counts,
        "bounds_m": [
            mesh.vertices.min(axis=0).tolist(),
            mesh.vertices.max(axis=0).tolist(),
        ],
        "vertices": len(mesh.vertices),
        "triangles": len(mesh.triangles),
        "artifacts": {p.name: digest(p) for p in sorted(output.glob("*.npz"))},
        "scope": "Unchanged existing young Pinus recipe, wood/short-shoot/needle emitter and physical attachment reader; no botanical observation inferred from Wiener landcover.",
    }
    (output / "prototype.json").write_text(json.dumps(report, indent=2) + "\n")
    return mesh, report


def bounds_query(identity, bounds, origin):
    bounds, origin = np.asarray(bounds), np.asarray(origin)
    if (
        bounds.shape != (2, 3)
        or origin.shape != (3,)
        or not np.isfinite([*bounds, origin]).all()
    ):
        raise ValueError("finite measured plant bounds and actual root required")
    lo, hi = bounds + origin
    return BuildingFootprint(
        identity,
        shapely.box(*lo[:2], *hi[:2]),
        HeightInterval(float(lo[2]), float(hi[2])),
    )

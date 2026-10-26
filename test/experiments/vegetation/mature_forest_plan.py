"""Bind complete mature catalog forms to all unchanged synthetic stand sites."""

import hashlib
import json

import numpy as np
from forest_views import choices
from preview_lighting import LANDSCAPE_SUN_ROTATION
from scipy.spatial.transform import Rotation


def site_angles(sites):
    return np.remainder(
        np.asarray(sites["ids"][:, -1], float) * 2.399963229728653
        + sites["variants"] * 1.1
        + sites["ids"][:, 1] * 0.7
        + sites["ids"][:, 2] * 1.3,
        2 * np.pi,
    )


def load_plan(source_path, products_path):
    recipe = json.loads(source_path.read_text())
    site_path = source_path.parent / recipe["sites"]
    with np.load(site_path, allow_pickle=False) as stored:
        sites = {key: stored[key] for key in stored.files}
    sites["angles"] = site_angles(sites)
    for values in sites.values():
        values.setflags(write=False)
    cases = json.loads(products_path.read_text())
    inputs, models = [source_path, site_path, products_path], []
    covered = np.zeros(len(sites["ids"]), bool)
    for case in cases:
        directory = products_path.parent / case["product"]
        product_path = directory / "plant-product.json"
        manifest = json.loads(product_path.read_text())
        recipe_path = directory / manifest["recipe"]
        plant_recipe = json.loads(recipe_path.read_text())
        identity = hashlib.sha256(
            json.dumps(plant_recipe, sort_keys=True).encode()
        ).hexdigest()
        if identity != case["identity"] or identity != manifest["plan_identity"]:
            raise ValueError("stand form must preserve the complete recipe identity")
        path = directory / manifest["mesh"]
        with np.load(path, allow_pickle=False) as stored:
            mesh = {key: stored[key] for key in stored.files}
        selected = np.flatnonzero(
            (sites["kinds"] == case["kind"]) & (sites["variants"] == case["variant"])
        )
        if covered[selected].any():
            raise ValueError("stand site cannot have two complete catalog forms")
        covered[selected] = True
        inputs.extend((product_path, recipe_path, path))
        inputs.extend(
            directory / filename for filename in manifest["textures"].values()
        )
        models.append(
            {
                "case": case,
                "directory": directory,
                "manifest": manifest,
                "mesh": mesh,
                "sites": selected,
            }
        )
    if not covered.all():
        raise ValueError("every source stand site requires a complete mature form")
    counts = np.bincount(sites["kinds"], minlength=len(recipe["counts"]))
    np.testing.assert_array_equal(counts, recipe["counts"])
    return recipe, sites, models, inputs


def select_views(model, sites, *, eye=None, direction=None):
    selected = model["sites"]
    return choices(
        model["mesh"],
        sites["positions"][selected],
        np.ones((len(selected), 3)),
        sites["angles"][selected],
        np.mean(model["manifest"]["bounds_m"], axis=0),
        eye=eye,
        direction=direction,
    )


def initial_views(recipe, sites, models):
    sun = Rotation.from_euler("xyz", LANDSCAPE_SUN_ROTATION).apply((0, 0, 1))
    return [
        {
            "camera": select_views(model, sites, eye=recipe["camera"]),
            "sun": select_views(model, sites, direction=sun),
        }
        for model in models
    ]

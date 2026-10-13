"""Whole spatial graph and matched coarse/near products from one stable plan."""

import json
import subprocess

import numpy as np
from graph_products import graph_products
from spatial_examples import spatial_graph


def make_scenes(output):
    graph = spatial_graph()
    coarse = graph_products(graph, output, "coarse")
    near = graph_products(graph, output, "near", max_new_poses=0)
    assert coarse and near
    for a in near["products"]:
        b = next(
            p
            for p in coarse["products"]
            if p["generator_ids"] == a["generator_ids"] and p["surface"] == a["surface"]
        )

        with (
            np.load(output / a["placements"]) as x,
            np.load(output / b["placements"]) as y,
        ):
            for key in x.files:
                np.testing.assert_array_equal(x[key], y[key])
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    for name, product, camera, target, fov in (
        ("spatial-graph", near, (1.8, -3.0, 1.9), (0, 0, 0.80), 35),
        ("spatial-coarse", coarse, (1.8, -3.0, 1.9), (0, 0, 0.80), 35),
        ("spatial-detail", near, (0.48, -0.88, 1.05), (0, 0, 0.78), 35),
    ):
        recipe = product | {
            "name": name,
            "camera": camera,
            "target": target,
            "fov_y": fov,
            "ground_size_m": 10,
            "resolution": [2560, 1440],
            "samples": 32,
            "source_commit": commit,
            "source_modified": True,
        }
        (output / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
    return graph.summary() | {
        "near_prototype_triangles": near["prototype_triangles"],
        "coarse_prototype_triangles": coarse["prototype_triangles"],
    }

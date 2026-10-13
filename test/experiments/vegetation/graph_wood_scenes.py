"""Complete rounded graph plant products, direct coarse body and join inspection."""

import json
import subprocess

import numpy as np
from graph_products import graph_products
from organ_experiment import write_product
from spatial_examples import spatial_graph


def make_scenes(output, near_mesh, coarse_mesh):
    graph = spatial_graph()
    cap = sum(graph.capacities.values())
    graph.instances(3, cap)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    products = {}
    for detail, mesh in (("near", near_mesh), ("coarse", coarse_mesh)):
        emitted = graph_products(graph, output, detail, nodes=(3,), max_new_poses=0)
        wood = write_product(
            output,
            "joined-" + detail + "-wood",
            mesh,
            np.zeros((1, 3)),
            np.eye(3)[None],
            "wood",
        )
        wood["bark_material"] = "bark-material.json"
        products[detail] = emitted | {"products": [wood] + emitted["products"]}
    anchor = graph.instances(2, 0)[1].anchor
    vector = np.array([0.48, -0.88, 0.27])
    vector /= np.linalg.norm(vector)
    views = (
        ("joined-spatial-graph", "near", (1.8, -3.0, 1.9), (0, 0, 0.80), False),
        ("joined-spatial-coarse", "coarse", (1.8, -3.0, 1.9), (0, 0, 0.80), False),
        ("joined-spatial-detail", "near", (0.48, -0.88, 1.05), (0, 0, 0.78), False),
        (
            "joined-spatial-junction",
            "near",
            (anchor + 0.12 * vector).tolist(),
            anchor.tolist(),
            True,
        ),
    )
    for name, detail, camera, target, wood_only in views:
        product = products[detail]
        recipe = product | {
            "name": name,
            "camera": camera,
            "target": target,
            "fov_y": 35,
            "ground_size_m": 10,
            "resolution": [2560, 1440],
            "samples": 32,
            "source_commit": commit,
            "source_modified": True,
            "wood_form": "spatial-graph-collars-1",
            "near_collars": detail == "near",
        }
        if wood_only:
            recipe["products"] = product["products"][:1]
        (output / f"{name}.json").write_text(json.dumps(recipe, indent=2) + "\n")
    return graph.summary()

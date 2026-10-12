"""Current whole-plant, coarse and close products of the same generator graph."""

import json
import subprocess

from generator_examples import mixed_graph
from graph_products import graph_products


def make_scenes(output):
    graph = mixed_graph()
    near = graph_products(graph, output, "near")
    coarse = graph_products(graph, output, "coarse")
    assert near is not None and coarse is not None
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    views = (
        ("generator-plant", near, (2.5, -3.5, 1.8), (0, 0, 0.70), 38),
        ("generator-coarse", coarse, (2.5, -3.5, 1.8), (0, 0, 0.70), 38),
        ("generator-detail", near, (0.7, -1.0, 1.1), (0, 0, 0.85), 38),
    )
    for name, product, camera, target, fov in views:
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
    return graph.summary()

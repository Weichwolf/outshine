"""Standalone installed core against complete previously generated plant products."""

import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from outshine_ref.vegetation import TwigClusters
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.twig_radius_basis import apply_basis, radius_basis


def replay(source, poses, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    with np.load(source, allow_pickle=False) as z:
        stored = {k[6:]: z[k] for k in z.files if k.startswith("twig__")}
        recipe = json.loads(str(z["recipe"]))
        if (
            recipe.pop("format") != "twig-clusters-1"
            or recipe.pop("numpy") != np.__version__
        ):
            raise ValueError("matching frozen source recipe required")
        plan = TwigClusters(
            host_ids=z["base__hosts"],
            host_counts=np.diff(z["base__offsets"]),
            variants=z["base__variants"],
            origins=z["base__origins"],
            frames=z["base__frames"],
            **recipe,
        )
        cursor = 0
        for node in plan.host_ids:
            child = plan.twigs(int(node), 32)
            end = cursor + len(child["ids"])
            for key, value in child.items():
                np.testing.assert_array_equal(value, stored[key][cursor:end])
            cursor = end
        scalar_tubes = 0
        for key in np.unique(stored["keys"]):
            count, variant = divmod(int(key), 16)
            prototype = plan.prototype(count, variant)
            for field in ("origins", "frames", "nodes"):
                np.testing.assert_array_equal(
                    prototype[field], stored[f"p{key}_{field}"]
                )
            shoot = plan.shoot(count, variant)
            np.testing.assert_array_equal(shoot.length, stored[f"p{key}_length"])
            radii = stored["radii"][stored["keys"] == key]
            radii = radii[[0, len(radii) // 2, -1]]
            basis = radius_basis(shoot.length, shoot.bend)
            vertices, normals = apply_basis(basis, radii)
            for i, radius in enumerate(radii):
                scalar = axis_mesh(shoot.length, shoot.bend, radius, radius * 0.3, 3, 4)
                np.testing.assert_array_equal(basis["faces"], scalar["faces"])
                np.testing.assert_allclose(
                    vertices[i], scalar["vertices"], atol=1e-12, rtol=0
                )
                np.testing.assert_allclose(
                    normals[i], scalar["normals"], atol=1e-10, rtol=0
                )
                scalar_tubes += 1
    actual = plan.tables(100000)
    with np.load(poses, allow_pickle=False) as previous:
        for key, value in actual.items():
            np.testing.assert_array_equal(value, previous[key])
    digest = hashlib.sha256()
    for key in ("ids", "variants", "origins", "frames", "nodes", "twig_ids"):
        digest.update(memoryview(actual[key]).cast("B"))
    result = {
        "source_identity": plan.source_identity,
        "plan_identity": plan.identity,
        "summary": plan.summary(),
        "pose_sha256": digest.hexdigest(),
        "scalar_radius_cases": scalar_tubes,
        "core_module": sys.modules[TwigClusters.__module__].__file__,
        "sys_path": sys.path,
        "experiment_imports": [
            n
            for n in sys.modules
            if n
            in (
                "attached_crown",
                "twig_cluster_mesh",
                "twig_cluster_experiment",
                "twig_clusters",
                "twig_radius_basis",
            )
        ],
    }
    if result["experiment_imports"]:
        raise AssertionError("standalone client imported experiment implementations")
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--poses", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    replay(args.source, args.poses, args.output)

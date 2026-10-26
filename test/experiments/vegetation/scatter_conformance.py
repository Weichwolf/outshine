"""Independent Float64 contacts for actual primary, scattered and sunlight rays."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from diffuse_bounce import surface_optics
from diffuse_scatter import sample_sheet, uniform_samples
from grass_projection_reader import read_product
from scatter_reference import Float64Nearest
from scatter_render import camera_packet
from scatter_scene import ScatterScene


def compare(scene, oracle, origins, directions, length):
    ids, points, _, distances = scene.nearest(origins, directions, length)
    exact, t, tested = oracle.query(origins, directions, length)
    np.testing.assert_array_equal(ids >= 0, exact >= 0)
    np.testing.assert_allclose(distances[ids >= 0], t[exact >= 0], atol=1e-9, rtol=0)
    np.testing.assert_allclose(
        points[ids >= 0],
        origins[exact >= 0] + t[exact >= 0, None] * directions[exact >= 0],
        atol=1e-9,
        rtol=0,
    )
    return {
        "rays": len(origins),
        "hits": int(sum(ids >= 0)),
        "reference_triangle_tests": tested,
    }


def run(inputs, output):
    report, stand, _ = read_product(inputs)
    ground = np.array(((-3, -3, 0), (3, -3, 0), (3, 3, 0), (-3, 3, 0)), float)
    vertices = np.concatenate((stand["vertices"], ground))
    faces = np.concatenate(
        (stand["faces"], np.array(((0, 1, 2), (0, 2, 3))) + len(stand["vertices"]))
    )
    scene, oracle = ScatterScene(vertices, faces), Float64Nearest(vertices, faces)
    view = np.array((np.sqrt(3) / 2, 0, 0.5))
    pixels = np.random.default_rng(953).choice(1920 * 1080, 96, replace=False)
    packets = [camera_packet(view, 1920, 1080, 4, int(i), 1, 73) for i in pixels]
    origins = np.concatenate([p[0][:1] for p in packets])
    directions = np.broadcast_to(-view, origins.shape)
    checks = {"primary": compare(scene, oracle, origins, directions, 10)}
    ids, points, normals, _ = scene.nearest(origins, directions, 10)
    valid = ids >= 0
    points, normals = points[valid], normals[valid]
    state = {
        "leaf_rt": [0.08, 0.22, 0.025, 0.035, 0.12, 0.018],
        "ground_albedo": [0.2, 0.13, 0.055],
    }
    optical = surface_optics(ids[valid], len(stand["faces"]), state)
    rays, _ = sample_sheet(optical, normals, view, uniform_samples(74, 0, len(points)))
    side = np.where(np.sum(normals * rays, axis=1) >= 0, 1, -1)
    origins = points + 2e-6 * side[:, None] * normals
    checks["scattered"] = compare(scene, oracle, origins, rays, scene.index.scale + 1)
    for i, light in enumerate(
        (
            (np.sqrt(3) / 2, 0, 0.5),
            (-np.sqrt(3) / 2, 0, 0.5),
            (0.6, 0, 0.8),
            (-0.6, 0, 0.8),
            (0, 0.6, 0.8),
            (0, -0.6, 0.8),
        )
    ):
        light = np.asarray(light)
        side = np.where(normals @ light >= 0, 1, -1)
        origins = points + 2e-6 * side[:, None] * normals
        lengths = np.maximum(
            (scene.index.maximum_height + 4e-6 - origins[:, 2]) / light[2], 0
        )
        exact, _, tested = oracle.query(
            origins, np.broadcast_to(light, origins.shape), lengths
        )
        actual, _ = scene.visibility(points, normals, light, 2e-6)
        np.testing.assert_array_equal(actual, exact < 0)
        checks[f"beam-{i}"] = {"rays": len(points), "reference_triangle_tests": tested}
    result = {
        "stand_identity": report["stand_identity"],
        "source_sha256": hashlib.sha256(
            (inputs / "result.json").read_bytes()
        ).hexdigest(),
        "checks": checks,
        "accelerated_work": scene.work,
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(checks), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--inputs", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    run(a.inputs, a.output)

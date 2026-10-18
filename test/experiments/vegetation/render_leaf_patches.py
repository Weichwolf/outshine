"""Finished-array 3D reader: reference host, old spatial cloud, shared four-patch leaves."""

import argparse
import hashlib
import json
import resource
import time
from dataclasses import dataclass
from pathlib import Path

import bpy
import numpy as np
from group_cloud import greedy_cloud
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_mesh import mesh_object
from preview_shadows import configure_preview
from render_spatial_clouds import coverage_material


@dataclass(frozen=True)
class SurfaceDraw:
    mesh: dict
    colour: np.ndarray


def read_poses(inputs, source, node):
    with np.load(inputs / f"host-{node}/group-4.npz", allow_pickle=False) as z:
        poses = dict(z)
    np.testing.assert_array_equal(poses["ids"], source["ids"])
    np.testing.assert_array_equal(poses["variants"], source["leaf_variants"])
    np.testing.assert_array_equal(
        poses["physical_leaf_area_m2"], source["physical_leaf_area_m2"]
    )
    return poses


def read_charts(inputs, source, node):
    poses = read_poses(inputs, source, node)
    with np.load(inputs / "prototypes-4.npz", allow_pickle=False) as z:
        prototypes = dict(z)
    frame, origin = source["anchor_frame"], source["anchor_origin"]
    charts = []
    for leaf, variant in enumerate(poses["variants"]):
        rotation = poses["frames"][leaf]
        point = poses["origins"][leaf]
        r = frame.T @ rotation
        for part in range(4):
            key = f"v{variant}_p{part}_"
            vertices = (
                prototypes[key + "vertices"] @ rotation.T + point - origin
            ) @ frame
            mesh = {name: prototypes[key + name] for name in ("faces", "uv")}
            mesh |= {"vertices": vertices, "normals": prototypes[key + "normals"] @ r.T}
            charts.append(SurfaceDraw(mesh, prototypes[key + "colour"]))
    return tuple(charts)


def read_connected(inputs, folder, source, node):
    poses = read_poses(inputs, source, node)
    prototypes = {}
    for variant in np.unique(poses["variants"]):
        with np.load(folder / f"prototype-{variant}.npz", allow_pickle=False) as z:
            prototypes[int(variant)] = dict(z)
    frame, origin = source["anchor_frame"], source["anchor_origin"]
    draws = []
    for leaf, variant in enumerate(poses["variants"]):
        p = prototypes[int(variant)]
        r = poses["frames"][leaf]
        mesh = {name: p[name] for name in ("faces", "uv")}
        mesh |= {
            "vertices": (p["vertices"] @ r.T + poses["origins"][leaf] - origin) @ frame,
            "normals": p["normals"] @ r.T @ frame,
        }
        draws.append(SurfaceDraw(mesh, p["colour"]))
    return tuple(draws)


def scene_objects(source, spatial, patches, direction, *, camera_patches=None):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    d = np.asarray(direction, float)
    d /= np.linalg.norm(d)
    side = np.cross((0, 0, 1), d)
    side /= np.linalg.norm(side)
    camera_patches = patches if camera_patches is None else camera_patches
    points = np.vstack(
        (source["vertices"], *[c.mesh["vertices"] for c in (*spatial, *camera_patches)])
    )
    centre = (points.max(axis=0) + points.min(axis=0)) / 2
    distance = float(np.linalg.norm(np.ptp(points, axis=0))) * 1.15
    mesh_object(
        "exact coarse source",
        source["vertices"] - distance * side,
        source["faces"],
        source["normals"],
        coverage_material("source coverage"),
        True,
    )
    materials = {}
    for name, charts, offset in (
        ("old spatial", spatial, 0.0),
        ("shared source surfaces", patches, distance),
    ):
        for i, c in enumerate(charts):
            key = id(c.colour)
            if key not in materials:
                materials[key] = coverage_material(
                    f"{name} image {len(materials)}", c.colour[..., 3]
                )
            mesh_object(
                f"{name} {i}",
                c.mesh["vertices"] + offset * side,
                c.mesh["faces"],
                c.mesh["normals"],
                materials[key],
                True,
                uv=c.mesh["uv"],
            )
    scene = bpy.context.scene
    configure_preview(scene)
    scene.eevee.taa_render_samples = 16
    scene.camera = configure_camera(
        {
            "projection": "orthographic",
            "eye": (centre + 3 * d).tolist(),
            "target": centre.tolist(),
            "span_m": distance * 3.1,
        }
    )
    scene.camera.data.clip_start, scene.camera.data.clip_end = 0.01, 10
    scene.world = bpy.data.worlds.new("neutral diagnostic background")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (
        0.2,
        0.22,
        0.24,
        1,
    )
    scene.view_settings.view_transform = "Standard"
    scene.render.resolution_x, scene.render.resolution_y = 2560, 1440
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    return scene, {
        "direction": d.tolist(),
        "span_m": distance * 3.1,
        "target": centre.tolist(),
        "triangles": [
            len(source["faces"]),
            2 * len(spatial),
            sum(len(c.mesh["faces"]) for c in patches),
        ],
        "isolated_chart_materials": len(materials),
    }


def render(inputs, output, node, *, connected=None):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    with np.load(inputs / f"host-{node}/source.npz", allow_pickle=False) as z:
        source = dict(z)
    began = time.perf_counter()
    old = read_charts(inputs, source, node)
    patches = (
        old if connected is None else read_connected(inputs, connected, source, node)
    )
    spatial = greedy_cloud(source, size=16)
    rows = []
    for name, direction in (
        ("oblique", (np.cos(1.8), np.sin(1.8), 0.8)),
        ("grazing", (np.cos(0.4), np.sin(0.4), 0.05)),
    ):
        scene, row = scene_objects(
            source, spatial, patches, direction, camera_patches=old
        )
        scene.render.filepath = str(output / (name + ".png"))
        before = time.perf_counter()
        bpy.ops.render.render(write_still=True)
        rows.append(row | {"view": name, "render_s": time.perf_counter() - before})
    paths = [
        inputs / f"host-{node}/source.npz",
        inputs / f"host-{node}/group-4.npz",
        inputs / "prototypes-4.npz",
    ]
    if connected is not None:
        paths.extend(
            connected / f"prototype-{v}.npz" for v in np.unique(source["leaf_variants"])
        )
    report = {
        "host_node": node,
        "resolution": [2560, 1440],
        "blender": bpy.app.version_string,
        "samples": 16,
        "candidate": "independent patches"
        if connected is None
        else "connected carrier",
        "camera_basis": "original independent-patch comparison, unchanged for connected candidate",
        "cases": rows,
        "wall_s": time.perf_counter() - began,
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Private finished-NPZ reader, no source pose/mesh/prototype construction for patch candidate. Old spatial method rebuilt only as control. Left reference, middle spatial, right shared patches: enlarged unlit coverage, no material/light/shadow/native/whole-crown admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for k in ("inputs", "output"):
        p.add_argument("--" + k, type=Path, required=True)
    p.add_argument("--host", type=int, default=17)
    p.add_argument("--connected", type=Path)
    a = p.parse_args()
    render(a.inputs, a.output, a.host, connected=a.connected)

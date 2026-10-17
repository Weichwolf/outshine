"""Explicit prepared mesh-patch research inputs; no provider or runtime cache ABI."""

import argparse
import hashlib
import json
import tempfile
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from outshine_ref.contracts import LocalFrame, Mesh
from outshine_ref.geometry.surface_patches import SurfacePatch, SurfacePatches
from outshine_ref.geometry.surface_patches._patch import boundary
from stair_module_experiment import write_json
from stair_module_inputs import FIXTURE, read_arrays


def frame_for(origin):
    return LocalFrame("Wien-transport-ENU-" + "-".join(map(str, origin)) + "-up0m")


def from_field(layout, field, frame):
    ground = layout["ground"]
    roles = field["constructed"]
    constructed = np.zeros(len(ground), bool)
    if len(roles) == len(ground):
        constructed[:] = roles
    else:
        constructed[~ground] = roles
    mesh = Mesh(np.c_[layout["xy"], field["heights"]], layout["faces"])
    data = {"ground": ground, "constructed": constructed}
    content = hashlib.sha256((frame.identifier + "\0terrain_top").encode())
    content.update(mesh.vertices.tobytes())
    content.update(mesh.triangles.tobytes())
    for key, value in sorted(data.items()):
        content.update(key.encode() + value.dtype.str.encode() + value.tobytes())
    return SurfacePatch(
        "terrain-" + content.hexdigest(), frame, "terrain_top", mesh, data
    )


def save_patch(output, index, patch):
    name = "assets/patch-" + str(index) + ".npz"
    np.savez_compressed(
        output / name,
        vertices=patch.mesh.vertices,
        faces=patch.mesh.triangles,
        **patch.face_data,
    )
    return {"patch_id": patch.patch_id, "file": name, "sha256": digest(output / name)}


def load_patch(output, row, frame):
    path = (output / row["file"]).resolve()
    if not path.is_relative_to(output.resolve()) or digest(path) != row["sha256"]:
        raise ValueError("prepared patch path or bytes changed")
    saved = read_arrays(path)
    if set(saved) != {"vertices", "faces", "ground", "constructed"}:
        raise ValueError("complete prepared field-patch attributes required")
    return SurfacePatch(
        row["patch_id"],
        frame,
        "terrain_top",
        Mesh(saved["vertices"], saved["faces"]),
        {key: saved[key] for key in ("ground", "constructed")},
    )


def load_prepared(output):
    manifest = json.loads((output / "patches.json").read_text())
    if manifest["version"] != 1 or not 1 <= len(manifest["patches"]) <= 1024:
        raise ValueError("bounded prepared research patch manifest required")
    frame = frame_for(manifest["origin_lat_lon"])
    if manifest["frame"] != frame.identifier:
        raise ValueError("prepared patch frame changed")
    root = load_patch(output, manifest["patches"][0], frame)
    view = SurfacePatches(root)
    for row in manifest["patches"][1:]:
        view, _note = view.replace(load_patch(output, row, frame), row["removals"])
    return view, manifest


def whole(view):
    bounds = view.patches[0].bounds_xy_m
    result = view.query(bounds, max_faces=65536)
    if result.mesh is None:
        raise ValueError("whole diagnostic view failed: " + result.reason)
    return result


def query_points(view):
    root = view.patches[0].mesh
    points = [root.vertices[:: max(1, len(root.vertices) // 128), :2]]
    for patch in view.patches:
        edges = boundary(patch.mesh, np.arange(len(patch.mesh.triangles)))
        points.append(np.array(sorted({p for e in edges for p in e}))[:16, :2])
    result = np.unique(np.vstack(points), axis=0)
    return np.vstack([result, root.vertices[:, :2].max(axis=0) + 10])


def fresh(output):
    if (
        output.exists()
        or not output.name.startswith("outshine-")
        or not output.resolve().is_relative_to(tempfile.gettempdir())
    ):
        raise ValueError("fresh explicit outshine-* system-temp output required")


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--supports", type=Path, required=True)
    parser.add_argument(
        "--cached-module", type=Path, nargs=2, action="append", default=[]
    )
    parser.add_argument(
        "--reference",
        type=Path,
        default=FIXTURE.with_name("stair-north-bridges-vienna.json"),
    )
    parser.add_argument("--output", type=Path, required=True)
    return parser


def save_manifest(output, manifest):
    write_json(output / "patches.json", manifest)

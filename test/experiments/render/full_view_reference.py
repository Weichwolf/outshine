"""Original whole-view mesh and Double benchmark poses; opaque control only."""

import hashlib
import json
import math
import tarfile

import numpy as np
from check_building_full_view import check, read
from packed_normals import unpack


def load(path):
    root = path.parent
    manifest = json.loads((root / "manifest.json").read_text())
    if path.name not in manifest:
        raise ValueError("input must belong to its delivered manifest")
    for name, entry in manifest.items():
        raw = (root / name).read_bytes()
        if (
            len(raw) != entry["bytes"]
            or hashlib.sha256(raw).hexdigest() != entry["sha256"]
        ):
            raise ValueError("whole-view manifest mismatch: " + name)
    document = json.loads(read(path))
    context = json.loads((root / "context.json").read_text())
    if (
        context["parent"] != document["parent"]
        or context["projection"] != document["view"]
    ):
        raise ValueError("whole-view context belongs to another input")
    with tarfile.open(root / "original-inputs.tar.gz") as archive:
        member = archive.getmember(
            document["parent"] + "/" + document["view_key"] + ".bin"
        )
        if not member.isfile() or member.size > 64 * 1024**2:
            raise ValueError("bounded original native view required")
        original = archive.extractfile(member).read()
    receipt = check(document, original)
    source = source_arrays(document["results"][0])
    receipt.update(
        opaque_assumption=True,
        material_scope=context["material_scope"],
        part_scope=context["part_scope"],
    )
    return source, document, context, receipt


def source_arrays(result):
    """Decode validated native geometry; callers own input/provenance checks."""
    data = result["geometry"]
    arrays = {
        key: []
        for key in (
            "positions",
            "uv",
            "normal",
            "colour",
            "metres",
            "indices",
            "binding",
        )
    }
    offset = 0
    for stream, name in enumerate(("walls", "roofs")):
        geometry = data[name]
        n, count = len(geometry["positions"]), len(geometry["triangles"])
        for key, native in (
            ("positions", "positions"),
            ("uv", "uv"),
            ("normal", "normal_words"),
        ):
            arrays[key].append(
                np.asarray(
                    geometry[native], np.uint32 if key == "normal" else np.float32
                )
            )
        colours = data["wall_colours"] if stream == 0 else []
        metres = data["wall_material_metres"] if stream == 0 else []
        arrays["colour"].append(
            np.asarray(colours, np.float32).reshape(n, 4)
            if colours
            else np.ones((n, 4), np.float32)
        )
        arrays["metres"].append(
            np.asarray(metres, np.float32).reshape(n, 2)
            if metres
            else np.zeros((n, 2), np.float32)
        )
        arrays["indices"].append(np.asarray(geometry["triangles"], np.int64) + offset)
        bindings = np.full((count, 6), -1, np.int32)
        for slot, call in enumerate(result["mesh_calls"]):
            first, length = call[("wall" if stream == 0 else "roof") + "_index_range"]
            start, stop = first // 3, (first + length) // 3
            bindings[start:stop, 0] = slot
            bindings[start:stop, 1] = -1 if call["source"] is None else call["source"]
            bindings[start:stop, 2] = -1 if call["part"] is None else call["part"]
            bindings[start:stop, 4] = stream
            bindings[start:stop, 5] = np.arange(start, stop)
        arrays["binding"].append(bindings)
        offset += n
    source = {name: np.concatenate(parts) for name, parts in arrays.items()}
    source["vertex_normal"] = unpack(source["normal"])
    return source


def views(document, context):
    """Exactly two supplied 60-frame world-axis rotation/motion sequences."""
    frame = context["scene_frame"]
    basis = np.column_stack([frame[k] for k in ("x_ecef", "y_ecef", "z_ecef")])
    origin = np.array(frame["origin_ecef"]) - document["anchor_ecef"]
    projection = (
        context["projection"]["focal_px"],
        context["profile"]["width_px"],
        context["profile"]["height_px"],
    )
    result = {}
    for name in ("static", "moving"):
        motion = context["motion"][name]
        start = np.array(motion["start"]["world_from_camera"]).reshape(4, 4).T
        omega = np.array(motion["angular_velocity_deg_s"], float)
        speed = np.linalg.norm(omega)
        axis = omega / speed if speed else np.zeros(3)
        x, y, z = axis
        skew = np.array([[0, -z, y], [z, 0, -x], [-y, x, 0]])
        for index in range(motion["frames"]):
            seconds = (index + 1) * motion["time_step_s"]
            angle = math.radians(math.remainder(speed * seconds, 360))
            delta = (
                np.eye(3)
                + math.sin(angle) * skew
                + (1 - math.cos(angle)) * (skew @ skew)
            )
            rotation = delta @ start[:3, :3]
            position = start[:3, 3] + seconds * np.array(motion["linear_velocity_mps"])
            eye = origin + basis @ position
            camera = np.stack(
                (
                    basis @ rotation[:, 0],
                    basis @ rotation[:, 1],
                    -basis @ rotation[:, 2],
                )
            )
            result[f"{name}-{index + 1:03d}"] = eye, camera, *projection
    return result

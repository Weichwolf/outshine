"""Shared affine support templates versus independent fully emitted prisms."""

import numpy as np
import shapely
from building_mesh import solid
from indoor_mesh_checks import closed_mesh
from indoor_rail_geometry import hollow_sweep
from indoor_rail_supports import SupportLimits


def box_mesh():
    storage = [], [], [], []
    solid(storage, shapely.box(0, -0.5, 1, 0.5), -0.5, 0.5, 0)
    return {
        "vertices": np.asarray(storage[0], dtype=float),
        "faces": np.asarray(storage[1], dtype=np.uint32),
        "parts": np.asarray(storage[3], dtype=np.uint32),
    }


def support_templates(rail_limits, support_limits=None):
    support_limits = SupportLimits() if support_limits is None else support_limits
    post, _ = hollow_sweep(
        [(0, 0), (1, 0)],
        rail_limits.width_m,
        rail_limits.wall_m,
        cap_m=support_limits.cap_fraction,
    )
    box = box_mesh()
    closed_mesh(box)
    return {"post": post, "below_tread_bracket": box}


def materialize(template, transform):
    transform = np.asarray(transform, dtype=float)
    if (
        transform.shape != (3, 4)
        or not np.isfinite(transform).all()
        or np.linalg.det(transform[:, :3]) <= 0
    ):
        raise ValueError("Finite positive-orientation support transform required")
    return {
        "vertices": template["vertices"] @ transform[:, :3].T + transform[:, 3],
        "faces": template["faces"],
        "parts": template["parts"],
    }


def reference_support(row, rail_limits, support_limits=None):
    support_limits = SupportLimits() if support_limits is None else support_limits
    pose = row["transform"]
    if row["kind"] == "post":
        height = float(np.linalg.norm(pose[:, 0]))
        basis = pose[:, :3].copy()
        basis[:, 0] /= height
        mesh, _ = hollow_sweep(
            [(0, 0), (height, 0)],
            rail_limits.width_m,
            rail_limits.wall_m,
            basis,
            pose[:, 3],
            cap_m=support_limits.cap_fraction * height,
        )
        return mesh
    polygon = np.array([[0, -0.5], [1, -0.5], [1, 0.5], [0, 0.5]])
    xy = polygon @ pose[:2, :2].T + pose[:2, 3]
    low, high = pose[2, 3] - abs(pose[2, 2]) / 2, pose[2, 3] + abs(pose[2, 2]) / 2
    storage = [], [], [], []
    solid(storage, shapely.Polygon(xy), low, high, 0)
    return {
        "vertices": np.asarray(storage[0], dtype=float),
        "faces": np.asarray(storage[1], dtype=np.uint32),
        "parts": np.asarray(storage[3], dtype=np.uint32),
    }

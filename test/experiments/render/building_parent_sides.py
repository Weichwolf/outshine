"""Charge fixed directional parent-side descriptors before proposing payload splits."""

import numpy as np
from facing_cones import cone_excludes, prepare_cone
from native_backface_cost import unit_normals


def prepare_sides(data):
    triangle = data["g__positions"].astype(float)[data["g__indices"]]
    normals = unit_normals(triangle)
    major = np.abs(normals).argmax(axis=1)
    ids = major * 2 + (normals[np.arange(len(normals)), major] < 0)
    invalid = ((normals * normals).sum(axis=1) < 0.5) | ~np.isfinite(normals).all(
        axis=1
    )
    ids[invalid] = 6
    members, descriptors, ranges = [], [], []
    start = 0
    for bucket in np.unique(ids):
        faces = np.flatnonzero(ids == bucket)
        cone = prepare_cone(triangle[faces], normals[faces])
        if cone is None:
            cone = np.full(8, np.nan)
            cone[7] = np.max(np.abs(triangle[faces]))
        descriptors.append(cone)
        members.append(faces)
        ranges.append([start, len(faces)])
        start += len(faces)
    return {
        "cones": np.array(descriptors),
        "ranges": np.array(ranges, np.uint32),
        "members": members,
        "triangle": triangle,
        "normals": normals,
        "whole": prepare_cone(triangle, normals),
    }


def query_cones(cones, eye):
    margin = (
        64 * np.finfo(np.float32).eps * (np.linalg.norm(eye) + cones[:, 7].max() + 1)
    )
    return np.array([cone_excludes(c, eye, margin) for c in cones]), margin


def query_sides(prepared, data, eye):
    rejected, margin = query_cones(prepared["cones"], eye)
    dropped = np.zeros(len(prepared["triangle"]), bool)
    for reject, faces in zip(rejected, prepared["members"], strict=True):
        if reject:
            dropped[faces] = True
    signed = np.einsum(
        "ij,ij->i", prepared["normals"], eye - prepared["triangle"][:, 0]
    )
    valid = (prepared["normals"] ** 2).sum(axis=1) >= 0.5
    control = valid & (signed < -margin)
    if (dropped & ~control).any():
        raise ValueError(
            "side rejection must imply each original face's strict backface predicate"
        )
    faces = np.flatnonzero(~dropped)
    triangle_bytes = data["g__indices"].nbytes + data["g__triangle_groups"].nbytes
    all_bytes = sum(v.nbytes for v in data.values())
    metadata = prepared["cones"].nbytes + prepared["ranges"].nbytes
    payload = (
        all_bytes
        - triangle_bytes
        + data["g__indices"][faces].nbytes
        + data["g__triangle_groups"][faces].nbytes
    )
    return faces, {
        "input_triangles": len(dropped),
        "retained_triangles": len(faces),
        "rejected_triangles": int(dropped.sum()),
        "strict_backface_control_triangles": int(control.sum()),
        "whole_parent_cone_rejected_triangles": len(dropped)
        if cone_excludes(prepared["whole"], eye, margin)
        else 0,
        "side_descriptors": len(prepared["cones"]),
        "descriptor_and_range_bytes": metadata,
        "baseline_render_array_bytes": all_bytes,
        "selected_render_array_bytes_with_metadata": payload + metadata,
        "saved_bytes_after_metadata": all_bytes - payload - metadata,
        "unchanged_positions_bytes": data["g__positions"].nbytes,
        "diagnostic_original_parent_face_map_bytes": sum(
            v.nbytes for v in prepared["members"]
        ),
        "margin_m": float(margin),
    }

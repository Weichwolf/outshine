"""Rigid per-segment reference parts; welded skins need a different binding."""

import numpy as np
from outshine_ref.vegetation.axis_surface import axis_mesh


def wood_owners(crown, mesh):
    stem = axis_mesh(crown.length_m, crown.bend_rad, 0.0005, 0.00012, 3, 4)
    owners = np.r_[
        np.repeat(np.arange(1, len(crown.parents)), 18),
        np.repeat(crown.hosts, len(stem["vertices"])),
    ].astype(np.int32)
    if len(owners) != len(mesh["vertices"]):
        raise ValueError("exact complete segmented source wood layout required")
    face_owner = owners[mesh["faces"]]
    if not np.all(face_owner == face_owner[:, :1]):
        raise ValueError("all faces must belong to one rigid segment; no welded skin")
    return owners


def pose_wood(mesh, owners, source, moved, rotations):
    result = dict(mesh)
    r = rotations[owners]
    result["vertices"] = moved[owners] + np.einsum(
        "nij,nj->ni", r, mesh["vertices"] - source[owners]
    )
    face_owner = owners[mesh["faces"]]
    if not np.all(face_owner == face_owner[:, :1]):
        raise ValueError("cannot rigidly pose cross-owner faces")
    normal_owner = face_owner.ravel()
    result["normals"] = np.einsum(
        "nij,nj->ni", rotations[normal_owner], mesh["normals"]
    )
    return result

"""Restore known-valid finite surface products without replaying geometry history."""

import io
import zipfile
from types import MappingProxyType

import numpy as np
import shapely

from ...contracts import LocalFrame, Mesh
from ._archive import MAX_BYTES, inspect_archive, metadata
from ._field import SurfacePatches
from ._patch import SurfacePatch, label, readonly


def restore_patch(archive, row, index, frame, kind):
    if type(row) is not dict or set(row) != {"id", "max_faces", "attributes"}:
        raise ValueError("exact prepared patch description required")
    label(row["id"])
    names = row["attributes"]
    if type(names) is not list or len(names) > 8:
        raise ValueError("bounded prepared attribute names required")
    for name in names:
        label(name)
    if len(set(names)) != len(names):
        raise ValueError("unique prepared attribute names required")
    prefix = "p" + str(index)
    vertices, faces = archive[prefix + "-v"], archive[prefix + "-f"]
    if (
        vertices.dtype.kind != "f"
        or vertices.dtype.itemsize != 8
        or faces.dtype.kind != "u"
        or faces.dtype.itemsize != 4
        or not 1 <= len(vertices) <= 65536
        or type(row["max_faces"]) is not int
        or not 1 <= len(faces) <= row["max_faces"] <= 65536
    ):
        raise ValueError("bounded original prepared mesh buffers required")
    mesh = Mesh(vertices, faces)
    object.__setattr__(mesh, "vertices", readonly(mesh.vertices))
    object.__setattr__(mesh, "triangles", readonly(mesh.triangles))
    if np.max(abs(mesh.vertices), initial=0) > 1e9:
        raise ValueError("prepared metric coordinate range exceeded")
    data = {}
    for i, name in enumerate(names):
        values = archive[prefix + "-a" + str(i)]
        if (
            values.shape != (len(faces),)
            or values.dtype.kind not in "buif"
            or values.dtype.itemsize > 8
            or not np.isfinite(values).all()
        ):
            raise ValueError("one finite prepared primitive per face required")
        data[name] = readonly(values)
    active = archive[prefix + "-active"]
    if active.dtype != np.dtype(bool) or active.shape != (len(faces),):
        raise ValueError("one prepared active-mask bit per face required")
    if np.any(active.view(np.uint8) > 1):
        raise ValueError("canonical prepared active-mask bytes required")
    xy = mesh.vertices[:, :2]
    polygons = shapely.polygons(mesh.vertices[mesh.triangles, :2])
    polygons.flags.writeable = False
    part = object.__new__(SurfacePatch)
    for name, value in {
        "patch_id": row["id"],
        "frame": frame,
        "kind": kind,
        "mesh": mesh,
        "face_data": MappingProxyType(data),
        "max_faces": row["max_faces"],
        "_polygons": polygons,
        "_tree": shapely.STRtree(polygons),
        "_bounds": (*xy.min(axis=0), *xy.max(axis=0)),
    }.items():
        object.__setattr__(part, name, value)
    return part, readonly(active)


def restore_surface(payload, *, expected_sha256, frame, max_bytes=MAX_BYTES):
    """Restore only a known-valid product bound by the caller's expected digest.

    The existing producer/product record must bind these bytes, this codec version
    and the supplied frame. A digest calculated from unknown input grants no
    geometric validity. The caller publishes products only after normal strict
    construction; ordinary cache hits need no second geometric admission service.
    Structural checks run here; unknown products use strict construction/rebuild.
    """
    if not isinstance(frame, LocalFrame):
        raise TypeError("explicit caller-owned prepared frame required")
    try:
        inspect_archive(payload, expected_sha256, max_bytes)
        return _restore_arrays(payload, frame)
    except (zipfile.BadZipFile, EOFError, KeyError) as error:
        raise ValueError("incomplete or malformed prepared surface archive") from error


def _restore_arrays(payload, frame):
    with np.load(io.BytesIO(payload), allow_pickle=False) as archive:
        meta = metadata(archive)
        if meta["frame"] != frame.identifier:
            raise ValueError("prepared frame differs from caller-owned frame")
        label(meta["kind"])
        rows = meta["patches"]
        if (
            type(rows) is not list
            or type(meta["max_patches"]) is not int
            or not 1 <= len(rows) <= meta["max_patches"] <= 4096
        ):
            raise ValueError("bounded prepared patch set and capacity required")
        parts, active, expected = [], [], {"metadata"}
        for i, row in enumerate(rows):
            part, mask = restore_patch(archive, row, i, frame, meta["kind"])
            parts.append(part)
            active.append(mask)
            prefix = "p" + str(i)
            expected.update(prefix + suffix for suffix in ("-v", "-f", "-active"))
            expected.update(prefix + "-a" + str(j) for j in range(len(part.face_data)))
        schema = {k: v.dtype for k, v in parts[0].face_data.items()}
        if (
            set(archive.files) != expected
            or len({p.patch_id for p in parts}) != len(parts)
            or any(
                {k: v.dtype for k, v in p.face_data.items()} != schema for p in parts
            )
        ):
            raise ValueError(
                "complete unique prepared patches with one schema required"
            )
    result = object.__new__(SurfacePatches)
    for name, value in {
        "patches": tuple(parts),
        "_active": tuple(active),
        "_counts": tuple(int(mask.sum()) for mask in active),
        "max_patches": meta["max_patches"],
    }.items():
        object.__setattr__(result, name, value)
    return result

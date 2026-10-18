"""Shared unlit source-leaf surface partitions, not shared approximate leaf poses."""

from dataclasses import dataclass, replace

import numpy as np
from group_cloud import make_chart, organ_statistics
from outshine_ref.vegetation.normal_distribution import (
    jensen_error_bound,
    normal_partition_ids,
)
from raster_batch import rasterize


@dataclass(frozen=True)
class LeafPatchPrototype:
    charts: tuple
    face_partition: np.ndarray
    normal_bounds: np.ndarray
    source_triangles: int


def partition_faces(mesh, patches):
    if type(patches) is not int or patches not in (1, 2, 4):
        raise ValueError("one, two or four declared leaf patches required")
    uv = mesh["parameters"][mesh["faces"]].mean(axis=1)
    owners = np.zeros(len(uv), np.int32)
    if patches >= 2:
        owners += (uv[:, 1] > 0).astype(np.int32)
    if patches == 4:
        owners += 2 * (uv[:, 0] >= 0.5).astype(np.int32)
    if len(np.unique(owners)) != patches:
        raise ValueError("complete nonempty surface partition required")
    owners.setflags(write=False)
    return owners


def normal_field(source, chart, size, *, partitions=1):
    """Positive mass/vector field in the original blade frame, with no light."""
    if type(partitions) is not int or partitions not in (1, 4):
        raise ValueError("one or four diagnostic normal bins required")
    projected = source["vertices"] @ chart.frame
    corners = chart.mesh["vertices"] @ chart.frame
    mask, normals = rasterize(
        projected,
        source["faces"],
        source["normals"].reshape(-1, 3, 3),
        corners[:, :2].min(axis=0),
        corners[:, :2].max(axis=0),
        2 * size,
    )
    normals = normals[mask].astype(float)
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    y, x = np.nonzero(mask)
    bins = normal_partition_ids(normals, 4, chart.frame) if partitions == 4 else 0
    slot = (y // 2 * size + x // 2) * partitions + bins
    field = np.empty((size * size, partitions, 4))
    weights = np.column_stack((np.ones(len(normals)), normals)) * 0.25
    for channel in range(4):
        field[..., channel] = np.bincount(
            slot, weights=weights[:, channel], minlength=size * size * partitions
        ).reshape(-1, partitions)
    np.testing.assert_array_equal(
        field[..., 0].sum(axis=1).reshape(size, size), chart.colour[..., 3]
    )
    field.setflags(write=False)
    return field


def chart_moment(chart):
    """Decode the existing premultiplied normal map without normalizing its vector."""
    rgba = chart.normal.reshape(-1, 4)
    mass = rgba[:, 3]
    vectors = (2 * rgba[:, :3] - mass[:, None]) @ chart.frame.T
    return np.column_stack((mass, vectors))[:, None, :]


def build_prototype(blade, variant, patches=2, size=16):
    if type(size) is not int or size not in (8, 16, 32):
        raise ValueError("bounded isolated patch resolution required")
    # Validate partition capacity before even the small canonical mesh is built.
    if type(patches) is not int or patches not in (1, 2, 4):
        raise ValueError("one, two or four declared leaf patches required")
    mesh = blade.mesh(8, 1)
    owners = partition_faces(mesh, patches)
    source = mesh | {
        "organ_faces": owners,
        "ids": np.arange(patches),
        "variants": np.full(len(mesh["vertices"]), variant, np.uint8),
    }
    _, normals = organ_statistics(source)
    charts, bounds = [], []
    for part in range(patches):
        chart = make_chart(source, np.array([part]), normals[part], size)
        mask = owners == part
        partial = source | {
            "faces": mesh["faces"][mask],
            "normals": mesh["normals"].reshape(-1, 3, 3)[mask].reshape(-1, 3),
        }
        field = normal_field(partial, chart, size)
        mass, vectors = field[:, 0, 0], field[:, 0, 1:]
        encoded = np.column_stack(
            ((vectors @ chart.frame + mass[:, None]) / 2, mass)
        ).reshape(size, size, 4)
        chart = replace(chart, normal=encoded)
        bounds.append(float(jensen_error_bound(field).max()))
        for a in (
            *chart.mesh.values(),
            chart.colour,
            chart.normal,
            chart.members,
            chart.frame,
        ):
            a.setflags(write=False)
        charts.append(chart)
    bounds = np.asarray(bounds)
    bounds.setflags(write=False)
    return LeafPatchPrototype(tuple(charts), owners, bounds, len(mesh["faces"]))

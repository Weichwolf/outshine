"""Direct rectangle-ring products; contact faces require a checked immutable host."""

from dataclasses import dataclass
from functools import lru_cache
from types import MappingProxyType

import numpy as np
import shapely

from ..contracts import immutable
from .modelling.scope import scalar

MAX_FIELDS = 64
FRAME_M = 0.08
DEPTH_M = 0.12
GLASS_INSET_M = 0.04
GLASS_M = 0.006


def triangle_surface(raw, material):
    points = raw["vertices"][raw["faces"]]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    along = points[:, 1] - points[:, 0]
    along /= np.linalg.norm(along, axis=1, keepdims=True)
    across = np.cross(normals, along)
    uv = np.stack(
        (
            np.einsum("tij,tj->ti", points, along),
            np.einsum("tij,tj->ti", points, across),
        ),
        axis=2,
    )
    return {
        "vertices": points.reshape(-1, 3),
        "faces": np.arange(points.size // 3, dtype=np.uint32).reshape(-1, 3),
        "normals": np.repeat(normals, 3, axis=0),
        "uv": uv.reshape(-1, 2),
        "materials": np.full(len(points), material, dtype=np.uint16),
        "facade_extent": np.zeros((points.size // 3, 3)),
    }


def corners(item):
    a, low, b, high = item["bounds"]
    xy = np.array([[a, low], [b, low], [b, high], [a, high]])
    return np.c_[
        xy @ item["axes"] + item["origin_xy"],
        item["intercept_m"] + item["slope"] * xy[:, 0],
    ]


def quads(top, bottom, *, inside=False):
    output = np.stack(
        [top, np.roll(top, -1, axis=0), np.roll(bottom, -1, axis=0), bottom], axis=1
    )
    return output[:, ::-1] if inside else output


@lru_cache(maxsize=32, typed=True)
def template(width, length, slope, *, contact=False):
    width, length, slope = (scalar(v) for v in (width, length, slope))
    inset = FRAME_M / np.hypot(1, slope)
    if width <= 2 * inset or length <= 2 * FRAME_M or type(contact) is not bool:
        raise ValueError("Rooflight dimensions leave no positive clear aperture")
    outer = np.array([[0.0, 0, 0], [width, 0, 0], [width, length, 0], [0, length, 0]])
    inner = np.array(
        [
            [inset, FRAME_M, 0],
            [width - inset, FRAME_M, 0],
            [width - inset, length - FRAME_M, 0],
            [inset, length - FRAME_M, 0],
        ]
    )
    below = np.array([0, 0, DEPTH_M])
    top = np.stack(
        [outer, np.roll(outer, -1, axis=0), np.roll(inner, -1, axis=0), inner], axis=1
    )
    frame = [top, (top - below)[:, ::-1], quads(inner, inner - below)]
    if not contact:
        frame.append(quads(outer, outer - below, inside=True))
    glass_top = inner - [0, 0, GLASS_INSET_M]
    glass_bottom = glass_top - [0, 0, GLASS_M]
    glass = [
        glass_top[None],
        glass_bottom[None, ::-1],
        quads(glass_top, glass_bottom, inside=True),
    ]
    surfaces = np.concatenate([*frame, *glass])
    surfaces[:, :, 2] += slope * surfaces[:, :, 0]
    vertices = surfaces.reshape(-1, 3)
    indices = np.arange(len(vertices), dtype=np.uint32).reshape(-1, 4)
    faces = indices[:, [0, 1, 2, 0, 2, 3]].reshape(-1, 3)
    value = triangle_surface({"vertices": vertices, "faces": faces}, 10)
    value["materials"][-12:] = 3
    for array in value.values():
        array.flags.writeable = False
    return MappingProxyType(value)


def place(item, raw):
    a, low, _b, _high = item["bounds"]
    transform = np.eye(3)
    transform[:2, :2] = item["axes"]
    origin = np.r_[
        np.array([a, low]) @ item["axes"] + item["origin_xy"],
        item["intercept_m"] + item["slope"] * a,
    ]
    return dict(
        raw,
        vertices=raw["vertices"] @ transform + origin,
        normals=raw["normals"] @ transform,
    )


def closed(fields):
    if len(fields) > MAX_FIELDS:
        raise ValueError("Rooflight product capacity exceeded")
    return [
        place(
            item,
            template(
                item["bounds"][2] - item["bounds"][0],
                item["bounds"][3] - item["bounds"][1],
                item["slope"],
            ),
        )
        for item in fields
    ]


@dataclass(frozen=True)
class RooflightHost:
    fields: tuple
    shell: object


def contact_coverage(shell, item, material):
    identifier = item["id"] + ":lightwell"
    if identifier not in shell.source_ids:
        raise ValueError("Rooflight host lacks the owned lightwell source")
    selected = shell.source_indices == shell.source_ids.index(identifier)
    mesh = shell.mesh
    if np.any(mesh["materials"][selected] != material):
        raise ValueError("Rooflight host contact has an unexpected material")
    points = mesh["vertices"][mesh["faces"][selected]]
    normals = mesh["normals"][mesh["faces"][selected, 0]]
    vertices = corners(item)
    for first, last in zip(vertices, np.roll(vertices, -1, axis=0), strict=True):
        tangent = last[:2] - first[:2]
        length = np.linalg.norm(tangent)
        tangent /= length
        inward = np.r_[-tangent[1], tangent[0], 0]
        owned = (normals @ inward > 0.999999) & (
            abs((points[:, :, :2] - first[:2]) @ inward[:2]).max(axis=1) < 1e-7
        )
        if not owned.any():
            raise ValueError("Rooflight host has no mating wall")
        chart = np.stack(
            [(points[owned, :, :2] - first[:2]) @ tangent, points[owned, :, 2]], axis=2
        )
        domain = shapely.Polygon(
            [
                (0, first[2]),
                (length, last[2]),
                (length, last[2] - DEPTH_M),
                (0, first[2] - DEPTH_M),
            ]
        )
        coverage = shapely.union_all(shapely.polygons(chart)).intersection(domain)
        if coverage.symmetric_difference(domain).area > 1e-7:
            raise ValueError("Rooflight host leaves an uncovered frame contact")


def prepare_host(shell, fields, *, material=1):
    if not len(fields) <= MAX_FIELDS or len({p["id"] for p in fields}) != len(fields):
        raise ValueError("Rooflight host fields require bounded unique ownership")
    for item in fields:
        contact_coverage(shell, item, material)
    if any(array.flags.writeable for array in shell.mesh.values()):
        raise ValueError("Prepared rooflight host needs read-only mesh arrays")
    return RooflightHost(immutable(fields), shell)


def contact(host):
    if not isinstance(host, RooflightHost):
        raise TypeError("Contact skin needs its checked host")
    return [
        place(
            item,
            template(
                item["bounds"][2] - item["bounds"][0],
                item["bounds"][3] - item["bounds"][1],
                item["slope"],
                contact=True,
            ),
        )
        for item in host.fields
    ]

"""Convex native shape plans as closed halfspaces; unsupported sources stay original."""

import itertools
from dataclasses import dataclass

import numpy as np
import shapely


@dataclass
class Prisms:
    planes: np.ndarray
    offsets: np.ndarray
    binding: np.ndarray
    bounds: np.ndarray
    roof: np.ndarray
    fallback_faces: np.ndarray


def roof_planes(shape):
    """Accept continuous concave piecewise planes, not arbitrary sampled roofs."""
    if shape["roof_kind"] == 2:
        hip = shape.get("hip")
        if not hip or not hip["available"] or hip["maximum_distance_m"] <= 0:
            raise ValueError("missing hip plan")
        scale = shape["rise_m"] / hip["maximum_distance_m"]
        origin = np.array(hip["origin"])
        planes = []
        for facet in hip["facets"]:
            gradient = scale * np.array(facet["gradient"])
            planes.append(
                [
                    *gradient,
                    shape["roof_deck_m"]
                    + scale * facet["intercept_m"]
                    - gradient @ origin,
                ]
            )
        planes = np.array(planes)
        # Every supplied facet must be on the lower envelope, including its corners.
        for facet, plane in zip(hip["facets"], planes, strict=True):
            xy = np.array(facet["ring"]) + origin
            if not np.allclose(
                xy @ plane[:2] + plane[2],
                np.min(xy @ planes[:, :2].T + planes[:, 2], axis=1),
                atol=1e-7,
                rtol=1e-10,
            ):
                raise ValueError("nonconcave hip plan")
        return planes
    bands = shape["roof_bands"]
    if shape["roof_kind"] not in (0, 1, 3, 4) or not bands:
        raise ValueError("unsupported roof")
    if (
        bands[0]["v_interval"]["min"] is not None
        or bands[-1]["v_interval"]["max"] is not None
    ):
        raise ValueError("unbounded roof-band partition required")
    for a, b in itertools.pairwise(bands):
        join = a["v_interval"]["max"]
        if (
            join is None
            or join != b["v_interval"]["min"]
            or a["slope_v"] < b["slope_v"]
        ):
            raise ValueError("nonconcave band partition")
        if not np.isclose(
            a["height_at_v0_m"] + join * a["slope_v"],
            b["height_at_v0_m"] + join * b["slope_v"],
            atol=1e-8,
            rtol=1e-10,
        ):
            raise ValueError("discontinuous roof bands")
    axis = np.array(shape["axis_u"])
    v = np.array([-axis[1], axis[0]])
    result = []
    for band in bands:
        gradient = band["slope_v"] * v
        result.append([*gradient, band["height_at_v0_m"] - gradient @ shape["centre"]])
    return np.array(result)


def shape_planes(shape):
    ring = np.asarray(shape["ring"], float)
    polygon = shapely.Polygon(ring, shape["holes"])
    if (
        not polygon.is_valid
        or polygon.area <= 0
        or shape["holes"]
        or not polygon.equals(polygon.convex_hull)
    ):
        raise ValueError("nonconvex or holed footprint")
    roof = roof_planes(shape)
    if np.min(ring @ roof[:, :2].T + roof[:, 2]) < shape["roof_deck_m"] - 1e-7:
        raise ValueError("roof below native wall deck")
    witness = np.array(polygon.representative_point().coords)[0]
    if np.min(roof[:, :2] @ witness + roof[:, 2]) <= shape["bottom_m"]:
        raise ValueError("no positive interior witness")
    for control in shape["height_controls"]:
        if control["inside"]:
            height = np.min(roof[:, :2] @ control["point"] + roof[:, 2])
            if not np.isclose(
                height,
                shape["roof_deck_m"] + control["height_m"],
                atol=1e-7,
                rtol=1e-10,
            ):
                raise ValueError("roof control mismatch")
    edge = np.roll(ring, -1, axis=0) - ring
    area = np.sum(
        ring[:, 0] * np.roll(ring[:, 1], -1) - ring[:, 1] * np.roll(ring[:, 0], -1)
    )
    normal = np.column_stack((edge[:, 1], -edge[:, 0])) * np.sign(area)
    walls = np.column_stack(
        (normal, np.zeros(len(ring)), np.sum(normal * ring, axis=1))
    )
    bottom = np.array([[0, 0, -1, -shape["bottom_m"]]])
    upper = np.column_stack((-roof[:, :2], np.ones(len(roof)), roof[:, 2]))
    planes = np.concatenate((bottom, walls, upper))
    planes /= np.linalg.norm(planes[:, :3], axis=1)[:, None]
    if not np.isfinite(planes).all():
        raise ValueError("nonfinite shape planes")
    top = float(np.min(np.max(ring @ roof[:, :2].T + roof[:, 2], axis=0)))
    if top <= shape["bottom_m"]:
        raise ValueError("empty height range")
    low, high = ring.min(0), ring.max(0)
    corners = np.array(
        [
            [x, y, z]
            for x in (low[0], high[0])
            for y in (low[1], high[1])
            for z in (shape["bottom_m"], top)
        ]
    )
    return planes, corners, np.arange(len(planes)) >= 1 + len(ring)


def compile_prisms(document, source):
    """Atomic source fallback; original triangle identities remain in fallback_faces."""
    records, offsets, binding, bounds, roofs = [], [0], [], [], []
    fallback, reasons = [], {}
    source_indices = source["binding"][:, 1]
    plans = {item["index"]: item for item in document["sources"]}
    if sum(len(s["surface"]["shapes"]) for s in plans.values() if s["surface"]) > 65536:
        raise ValueError("bounded whole-cell shape table required")
    if (
        len(plans) != len(document["sources"])
        or set(source_indices) != set(plans)
        or -1 in plans
    ):
        raise ValueError("complete original source mapping required")
    for index in sorted(plans):
        native = plans[index]["surface"]
        try:
            if not native or not native["projectable"] or not native["shapes"]:
                raise ValueError("missing projectable plan")
            shapes = [shape_planes(shape) for shape in native["shapes"]]
        except ValueError as error:
            fallback.extend(np.flatnonzero(source_indices == index))
            reasons[index] = str(error)
            continue
        basis = np.array([native[name] for name in ("east", "north", "up")])
        origin = np.array(native["origin"])
        for part, (planes, corners, roof) in enumerate(shapes):
            xyz = planes[:, :3] @ basis
            records.extend(np.column_stack((xyz, planes[:, 3] + xyz @ origin)))
            roofs.extend(roof)
            offsets.append(len(records))
            binding.append([index, part])
            global_corners = corners @ basis + origin
            bounds.append([global_corners.min(0), global_corners.max(0)])
    data = Prisms(
        np.array(records).reshape(-1, 4),
        np.array(offsets, np.uint32),
        np.array(binding, np.int32).reshape(-1, 2),
        np.array(bounds).reshape(-1, 2, 3),
        np.array(roofs, bool),
        np.array(fallback, np.uint32),
    )
    return data, reasons


def intersect_planes(planes, origins, directions):
    """Cyrus/Beck entering boundary; front-face convention culls interior exits."""
    distance = planes[:, 3] - origins @ planes[:, :3].T
    divisor = directions @ planes[:, :3].T
    t = np.divide(distance, divisor, out=np.zeros_like(distance), where=divisor != 0)
    entering = np.where(divisor < 0, t, -np.inf)
    plane = np.argmax(entering, axis=1)
    near = entering[np.arange(len(origins)), plane]
    far = np.min(np.where(divisor > 0, t, np.inf), axis=1)
    valid = (
        (near > 0.1) & (near <= far) & ~np.any((divisor == 0) & (distance < 0), axis=1)
    )
    return np.where(valid, near, np.inf), plane

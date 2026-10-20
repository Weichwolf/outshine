"""Restrict a shared P1 functional height owner to complete border polylines."""

import hashlib
import itertools
from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.geometry import SurfaceSampler
from water_field_difference import height, surface


@dataclass(frozen=True)
class EdgeProfile:
    points: np.ndarray
    segments: np.ndarray
    parent_faces: np.ndarray
    context: str
    report: dict


@dataclass(frozen=True)
class ProfileOwner:
    mesh: object
    anchor: np.ndarray
    mask: object
    vertices: np.ndarray
    gradient: np.ndarray
    polygons: object
    sampler: object
    tree: object
    context: str


def profile_owner_context(mesh, anchor, mask, owner_version):
    if not isinstance(owner_version, str) or not owner_version:
        raise ValueError("explicit owner version required")
    digest = hashlib.sha256(owner_version.encode())
    for array in (anchor, mesh.vertices, mesh.triangles):
        digest.update(np.ascontiguousarray(array).tobytes())
    digest.update(mask.wkb)
    return digest.hexdigest()


def prepare_profile_owner(mesh, *, anchor, mask, owner_version):
    anchor = np.asarray(anchor, dtype=float)
    if (
        anchor.shape != (2,)
        or not np.isfinite(anchor).all()
        or not isinstance(owner_version, str)
        or not owner_version
    ):
        raise ValueError("explicit finite owner frame and version required")
    vertices, gradient, polygons, _area = surface(mesh)
    context = profile_owner_context(mesh, anchor, mask, owner_version)
    for array in (vertices, gradient, polygons):
        array.setflags(write=False)
    frame = anchor.copy()
    frame.setflags(write=False)
    return ProfileOwner(
        mesh,
        frame,
        mask,
        vertices,
        gradient,
        polygons,
        SurfaceSampler(mesh, xy_tolerance_m=0, max_faces=16384),
        shapely.STRtree(polygons),
        context,
    )


def masked_profile_owner(owner, *, mask, owner_version):
    """Bind a smaller eligibility mask while sharing the immutable height data."""
    if (
        not isinstance(mask, shapely.Geometry)
        or mask.geom_type not in ("Polygon", "MultiPolygon")
        or mask.is_empty
        or not mask.is_valid
        or shapely.get_num_coordinates(mask) > 32768
        or not np.isfinite(shapely.get_coordinates(mask)).all()
    ):
        raise ValueError("bounded valid local water mask required")
    if mask.difference(owner.mask).area > max(1e-8, mask.area * 1e-12):
        raise ValueError("local eligibility mask extends beyond common water")
    return ProfileOwner(
        owner.mesh,
        owner.anchor,
        mask,
        owner.vertices,
        owner.gradient,
        owner.polygons,
        owner.sampler,
        owner.tree,
        profile_owner_context(owner.mesh, owner.anchor, mask, owner_version),
    )


def cut_points(line, polygons, hits, a, b):
    direction = b - a
    square = float(direction @ direction)
    if square == 0:
        raise ValueError("zero-length shared border segment")
    cuts = {0.0: a, 1.0: b}
    for index in sorted(map(int, hits)):
        cut = line.intersection(polygons[index])
        for part in shapely.get_parts(cut):
            if part.is_empty:
                continue
            if part.geom_type == "Point":
                candidates = np.asarray(part.coords)
            elif part.geom_type == "LineString":
                candidates = np.asarray(part.coords)[[0, -1]]
            else:
                continue
            for point in candidates:
                if np.array_equal(point, a) or np.array_equal(point, b):
                    continue
                t = float((point - a) @ direction / square)
                if not 0 < t < 1:
                    raise ValueError("unrepresentable interior border cut")
                if t in cuts and not np.array_equal(cuts[t], point):
                    raise ValueError("ambiguous numeric border cut")
                cuts[t] = point
    return [p for _, p in sorted(cuts.items())]


def profile_border(
    mesh,
    polylines,
    *,
    anchor,
    mask,
    owner_version,
    segment_limit=8192,
    pair_limit=65536,
):
    owner = prepare_profile_owner(
        mesh, anchor=anchor, mask=mask, owner_version=owner_version
    )
    return restrict_profile(
        owner, polylines, segment_limit=segment_limit, pair_limit=pair_limit
    )


def restrict_profile(owner, polylines, *, segment_limit=8192, pair_limit=65536):
    """Split at every root-facet crossing; no snapping or endpoint-only inference.

    One returned product covers every input polyline, or the call refuses. Source
    projection/cohort ownership belongs to the caller. Heights use this owner's
    frame, mask and planes; intrinsic root creases are preserved. This does not
    invent compatible independent owners or guarantee later fine-fit feasibility.
    """
    if (
        type(segment_limit) is not int
        or not 1 <= segment_limit <= 8192
        or type(pair_limit) is not int
        or not 1 <= pair_limit <= 65536
    ):
        raise ValueError("bounded profile work required")
    if not polylines or sum(len(p) for p in polylines) > 2 * segment_limit:
        raise ValueError("bounded nonempty profile coordinates required")
    vertices, gradient, polygons = owner.vertices, owner.gradient, owner.polygons
    sampler, tree = owner.sampler, owner.tree
    points, lookup, segments, source_segment = [], {}, [], []
    input_segments = pairs = 0
    for polyline_index, polyline in enumerate(polylines):
        xy = np.asarray(polyline, dtype=float)
        if xy.ndim != 2 or xy.shape[1] != 2 or len(xy) < 2 or not np.isfinite(xy).all():
            raise ValueError("finite nonempty shared border polylines required")
        if not owner.mask.covers(shapely.LineString(xy)):
            raise ValueError("shared border extends outside precise water mask")
        for local_segment, (a, b) in enumerate(itertools.pairwise(xy)):
            input_segments += 1
            if input_segments > segment_limit:
                raise ValueError("input profile segment budget exhausted")
            line = shapely.LineString([a, b])
            hits = tree.query(line)
            pairs += len(hits)
            if pairs > pair_limit:
                raise ValueError("profile triangle-pair budget exhausted")
            cuts = cut_points(line, polygons, hits, a, b)
            ids = []
            for point in cuts:
                key = tuple(point)
                if key not in lookup:
                    lookup[key] = len(points)
                    points.append(point)
                ids.append(lookup[key])
            for first, last in itertools.pairwise(ids):
                if first == last or np.array_equal(points[first], points[last]):
                    raise ValueError("unrepresentable zero-length profile interval")
                if len(segments) == segment_limit:
                    raise ValueError("output profile segment budget exhausted")
                segments.append((first, last))
                source_segment.append((polyline_index, local_segment))
    xy = np.asarray(points).reshape(-1, 2)
    segments = np.asarray(segments, dtype=np.uint32).reshape(-1, 2)
    if not len(segments):
        raise ValueError("no shared positive-length border")
    if not shapely.covers(owner.mask, shapely.points(xy)).all():
        raise ValueError("computed profile vertex outside precise water mask")
    endpoints = sampler.sample(xy, max_queries=2 * segment_limit, max_candidates=262144)
    middle_xy = xy[segments].mean(axis=1)
    middle = sampler.sample(middle_xy, max_queries=segment_limit, max_candidates=262144)
    if any(endpoints.reasons) or any(middle.reasons):
        raise ValueError("border not completely covered by the functional owner")
    parents = middle.face_indices
    expected = np.array(
        [
            height(vertices, gradient, int(parent), xy[edge])
            for parent, edge in zip(parents, segments, strict=True)
        ]
    )
    error = float(abs(expected - endpoints.points_enu_m[segments, 2]).max())
    if error > 1e-9:
        raise ValueError("profile interval crosses an unresolved root crease")
    digest = hashlib.sha256(owner.context.encode())
    for array in (endpoints.points_enu_m, segments, parents):
        digest.update(np.ascontiguousarray(array).tobytes())
    owned = [
        np.array(a, copy=True) for a in (endpoints.points_enu_m, segments, parents)
    ]
    for array in owned:
        array.setflags(write=False)
    return EdgeProfile(
        *owned,
        owner.context,
        {
            "input_polylines": len(polylines),
            "input_segments": input_segments,
            "profile_vertices": len(xy),
            "profile_segments": len(segments),
            "triangle_candidates": pairs,
            "maximum_parent_plane_up_error_m": error,
            "source_segment_addresses": source_segment,
            "numeric_payload_bytes": sum(a.nbytes for a in owned),
            "profile_content_digest": digest.hexdigest(),
            "scope": "complete declared border restricted to common root planes in binary64; not an interval/geographic/Earth bound or cross-cohort ownership proof",
        },
    )

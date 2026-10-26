"""Bounded geometric cuts of retained native surfaces; no inferred face ownership."""

import bisect
import copy
import math
from itertools import pairwise

from mvt_native_cut_profiles import canonical_profile
from mvt_stair_native_crosscuts import cross_section


def _faces(native, crop):
    provenance = native["provenance"]
    if (
        crop["anchor"] != provenance["frame_longitude_latitude_deg"]
        or crop["region"] != provenance["ground_region_key"]
        or crop["manifest"] != provenance["surface_manifest_key"]
        or crop["manifest_pages"] != provenance["manifest_page_count"]
        or not 1 <= len(crop["pages"]) <= 64
    ):
        raise ValueError("retained native crop and query sources need the same frame")
    faces = []
    identities = set()
    for page in crop["pages"]:
        if page["level"] != 0:
            raise ValueError("fine native surface pages required")
        for triangle in page["triangles"]:
            key = (page["key"], triangle["triangle"])
            xyz = triangle["enu_m"]
            if (
                key in identities
                or type(triangle["triangle"]) is not int
                or triangle["triangle"] < 0
                or len(xyz) != 3
                or any(len(p) != 3 or not all(map(math.isfinite, p)) for p in xyz)
            ):
                raise ValueError("unique finite retained native triangles required")
            identities.add(key)
            faces.append(dict(triangle, page=page["key"]))
            if len(faces) > 8192:
                raise ValueError("native cut input face budget")
    if len(faces) != provenance["cropped_triangle_count"]:
        raise ValueError("complete retained crop required")
    return faces


def _plane(source, station):
    axis = source["original_source_xy_m"]
    if not 2 <= len(axis) <= 256 or any(
        len(p) != 2 or not all(map(math.isfinite, p)) for p in axis
    ):
        raise ValueError("bounded finite original axis required")
    arc = [0.0]
    for a, b in pairwise(axis):
        length = math.dist(a, b)
        if not math.isfinite(length) or length <= 0:
            raise ValueError("positive finite original axis segments required")
        arc.append(arc[-1] + length)
    if not math.isfinite(station) or not 0 <= station <= arc[-1]:
        raise ValueError("query outside original source; endpoint context is not free")
    at = min(len(axis) - 2, bisect.bisect_right(arc, station) - 1)
    tangent = [
        (b - a) / (arc[at + 1] - arc[at]) for a, b in zip(axis[at], axis[at + 1])
    ]
    center = [a + (station - arc[at]) * t for a, t in zip(axis[at], tangent)]
    return center, tangent


def _cut(source, station, half_span_m, bounds, faces):
    center, tangent = _plane(source, station)
    endpoints = [
        [center[0] - side * tangent[1], center[1] + side * tangent[0]]
        for side in (-half_span_m, half_span_m)
    ]
    if any(
        not bounds[axis] <= point[axis] <= bounds[axis + 3]
        for point in endpoints
        for axis in (0, 1)
    ):
        raise ValueError("query outside observed native crop")
    raw = cross_section(faces, center, tangent, half_span_m)
    keys = {(row["page"], row["triangle"]) for row in raw["segments"]}
    cut = {
        key: copy.deepcopy(source[key])
        for key in ("native_way", "original_source_xy_m", "source_tags")
    }
    cut.update(
        source_station_m=station,
        center_xy_m=center,
        tangent_xy=tangent,
        half_span_m=half_span_m,
        triangles=copy.deepcopy(
            [f for f in faces if (f["page"], f["triangle"]) in keys]
        ),
    )
    profile = canonical_profile(cut)
    if any(not bounds[2] <= p[2] <= bounds[5] for p in profile["profile_xyz_m"]):
        raise ValueError("profile leaves the observed height window")
    return cut, profile


def query_profiles(native, crop, queries, *, half_span_m=3.0):
    """Keep complete single-height bands, explicitly decline other local cuts.

    Each query has native_way and source_station_m. Source stations derive the
    plane; no nearest/highest surface or normal filter supplies ownership.
    Crop observations are finite inputs, not a worldwide completeness claim.
    """
    if (
        not 1 <= len(queries) <= 64
        or not 0 < half_span_m <= 128
        or not math.isfinite(half_span_m)
        or not 1 <= len(native["cuts"]) <= 64
    ):
        raise ValueError("bounded finite native query collections required")
    bounds = native["provenance"]["crop_enu_m"]
    if (
        len(bounds) != 6
        or not all(map(math.isfinite, bounds))
        or any(bounds[i] >= bounds[i + 3] for i in range(3))
    ):
        raise ValueError("finite increasing observed crop bounds required")
    sources = {}
    for cut in native["cuts"]:
        way = cut["native_way"]
        if way in sources and any(
            cut[key] != sources[way][key]
            for key in ("original_source_xy_m", "source_tags")
        ):
            raise ValueError(
                "one retained original axis and tag set per source required"
            )
        sources[way] = cut
    faces = _faces(native, crop)
    cuts, profiles, notes = [], [], []
    for query in queries:
        way, station = query["native_way"], query["source_station_m"]
        try:
            if way not in sources:
                raise ValueError("unknown original query source")
            cut, profile = _cut(sources[way], station, half_span_m, bounds, faces)
            cuts.append(cut)
            profiles.append(profile)
            notes.append(dict(query, status="complete_geometric_profile"))
        except ValueError as error:
            notes.append(dict(query, status="declined", reason=str(error)))
    result = {
        "case_version": native["case_version"],
        "provenance": copy.deepcopy(native["provenance"]),
        "cuts": cuts,
    }
    result["provenance"]["scope"] = (
        "Finite geometric queries of the complete retained native crop. Original source "
        "stations select planes, not face ownership. No normal/highest-surface filtering "
        "or outside packing, native navigation, terrain or world admission."
    )
    return result, profiles, notes

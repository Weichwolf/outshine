"""Build camera-independent grouped geometry and an explicit whole-source remainder."""

import numpy as np
import shapely
from building_mass_groups import (
    GAP_M,
    HEIGHT_SPAN_M,
    MAXIMUM_HEIGHT_M,
    close_group,
    group_sources,
)
from building_mass_inputs import read_masses
from building_mass_mesh import extrude


def build(parent, cells):
    sources, frame, read_work = read_masses(parent, cells)
    groups, group_work = group_sources(sources)
    positions, faces, reports, membership, offsets, owners = [], [], [], [], [0], []
    failures, height_ranges, singleton_sources = [], [], []
    vertex_offset = 0
    for group in groups:
        identities = [s.source for s in group]
        if len(group) == 1:
            singleton_sources.extend(identities)
            continue
        try:
            domain, height, closing = close_group(group)
            mesh, packed = extrude(domain, height)
        except (ValueError, ArithmeticError, shapely.errors.GEOSException) as error:
            failures.append({"sources": identities, "reason": str(error)})
            continue
        reports.append({"sources": identities, **closing, **packed})
        if (
            vertex_offset + len(mesh["positions"]) > 65536
            or len(owners) + len(mesh["indices"]) > 131072
        ):
            raise ValueError("complete aggregate product exceeds geometry budget")
        positions.append(mesh["positions"])
        faces.append(mesh["indices"].astype(np.uint32) + vertex_offset)
        owners.extend([len(reports) - 1] * len(mesh["indices"]))
        vertex_offset += len(mesh["positions"])
        membership.extend(identities)
        offsets.append(len(membership))
        height_ranges.append(height)
    if not reports or vertex_offset > 65536:
        raise ValueError("nonempty bounded UInt16 grouped mesh required")
    merged = np.array(membership, np.uint32)
    remainder = np.setdiff1d(frame["selected_sources"], merged)
    if len(merged) != len(np.unique(merged)) or not np.array_equal(
        np.sort(np.r_[merged, remainder]), frame["selected_sources"]
    ):
        raise ValueError("every original source must belong to exactly one product")
    arrays = {
        **frame,
        "positions": np.vstack(positions),
        "indices": np.vstack(faces).astype(np.uint16),
        "triangle_groups": np.array(owners, np.uint16),
        "group_sources": merged,
        "group_offsets": np.array(offsets, np.uint32),
        "group_heights": np.array(height_ranges),
        "needs_sources": remainder,
    }
    return arrays, {
        "read": read_work,
        "group_work": group_work,
        "groups": reports,
        "declined_groups": failures,
        "singleton_sources": singleton_sources,
        "profile": {
            "gap_m": GAP_M,
            "height_range_m": HEIGHT_SPAN_M,
            "maximum_height_m": MAXIMUM_HEIGHT_M,
            "closing_radius_m": GAP_M / 2,
            "join_style": "mitre",
            "mitre_limit": 2,
            "simplification_tolerance_m": 0,
        },
        "array_bytes": {k: a.nbytes for k, a in arrays.items()},
        "all_array_bytes": sum(a.nbytes for a in arrays.values()),
        "scope": "Source-first geometric mass candidate only. Added inter-building space is not certified free of transport/functions; source footprint loss is measured, not an extensive or surface-distance guarantee. Geometric grouping and local Float32 closed-surface checks are not material/UV/metres/shading-normal/depth/caster/contact/LOD/device admission. Full unmerged/declined/unknown sources remain explicit local original needs.",
    }

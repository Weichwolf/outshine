"""Round tram axes inside their source corridor, away from fixed contacts."""

import numpy as np
import shapely

from .horizontal import corner
from .rail_hardware import RailRecipe
from .rail_routes import surface_rail_chains
from .route_tiles import geography
from .tram_rails import RAIL_HEAD_WIDTH_M, TRAM_BED_WIDTH_M

PREFERRED_RADIUS_M = 20.0
MINIMUM_RADIUS_M = 12.0
MAXIMUM_SHIFT_M = 0.45
CHORD_ERROR_M = 0.002
RAIL_ENVELOPE_M = RailRecipe().gauge_m + 2 * RAIL_HEAD_WIDTH_M


def _candidate_turns(xy, locked):
    edges = np.diff(xy, axis=0)
    spans = np.linalg.norm(edges, axis=1)
    if np.any(spans <= 1e-6):
        return
    headings = np.arctan2(edges[:, 1], edges[:, 0])
    angles = (np.diff(headings) + np.pi) % (2 * np.pi) - np.pi
    eligible = (~locked[1:-1]) & (abs(angles) > 1e-5) & (abs(angles) < np.pi / 2)
    tangent = np.tan(abs(angles) / 2)
    shift_per_radius = (1 / np.cos(abs(angles) / 2)) - 1
    radius = np.minimum(
        PREFERRED_RADIUS_M, MAXIMUM_SHIFT_M / np.maximum(shift_per_radius, 1e-9)
    )
    cut = np.r_[0.0, np.where(eligible, radius * tangent, 0.0), 0.0]
    required = cut[:-1] + cut[1:]
    available = (
        spans
        - CHORD_ERROR_M
        - np.where(locked[:-1] | locked[1:], TRAM_BED_WIDTH_M, 0.0)
    )
    scale = np.minimum(1, np.maximum(0, available) / np.maximum(required, 1e-9))
    radius *= np.minimum(scale[:-1], scale[1:])
    for index in np.flatnonzero(eligible & (radius >= MINIMUM_RADIUS_M)):
        turn, _ = corner(
            xy,
            index + 1,
            angles[index],
            headings[index],
            radius[index] * tangent[index],
            {"model": "circle", "radius_m": radius[index], "error_m": CHORD_ERROR_M},
        )
        yield index + 1, turn


def _track_candidates(chain, xy, contacts, obstacles):
    locked = np.asarray([node in contacts or node.cut is not None for node in chain])
    locked[[0, -1]] = True
    for index, turn in _candidate_turns(xy, locked):
        replaced = shapely.LineString([turn[0], xy[index], turn[-1]])
        if not obstacles.query(
            replaced, "dwithin", TRAM_BED_WIDTH_M / 2 + MAXIMUM_SHIFT_M
        ).size:
            yield index, (turn, replaced)


def _separated(turn, replaced, route, axes, tracks):
    arc = shapely.LineString(turn)
    neighbours = tracks.query(replaced, "dwithin", TRAM_BED_WIDTH_M + MAXIMUM_SHIFT_M)
    neighbours = neighbours[neighbours != route]
    prior = shapely.distance(replaced, axes[neighbours])
    clearance = shapely.distance(arc, tracks.geometries[neighbours])
    return np.all(prior > RAIL_ENVELOPE_M) and np.all(
        clearance >= np.minimum(prior, TRAM_BED_WIDTH_M) - 1e-7
    )


def tram_corners(graph, project, protected):
    chains = list(surface_rail_chains(graph, kind="transit", mode="tram"))
    courses = [
        project(*np.asarray([geography(node.point) for node in chain]).T)
        for chain in chains
    ]
    axes = np.asarray([shapely.LineString(xy) for xy in courses], dtype=object)
    obstacles = shapely.STRtree(protected)
    contacts = {pair.rail for pair in graph.contacts}
    contacts.update(portal.ground for portal in graph.portals)
    proposals = [
        dict(_track_candidates(chain, xy, contacts, obstacles))
        if len(chain) > 2 and axis.is_simple
        else {}
        for chain, xy, axis in zip(chains, courses, axes, strict=True)
    ]
    tracks = shapely.STRtree(
        [
            shapely.MultiLineString([xy, *(turn for turn, _ in options.values())])
            for xy, options in zip(courses, proposals, strict=True)
        ]
    )
    turns = {}
    for route, (chain, xy, options) in enumerate(
        zip(chains, courses, proposals, strict=True)
    ):
        candidates = {
            index: turn
            for index, (turn, replaced) in options.items()
            if _separated(turn, replaced, route, axes, tracks)
        }
        if (
            candidates
            and shapely.LineString(
                np.concatenate(
                    [candidates.get(i, xy[i : i + 1]) for i in range(len(xy))]
                )
            ).is_simple
        ):
            turns.update(
                (tuple(node.point for node in chain[i - 1 : i + 2]), turn)
                for i, turn in candidates.items()
            )
    return turns

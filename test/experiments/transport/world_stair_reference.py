"""Fixed World-port input through the existing indexed Python body operators."""

import math
from pathlib import Path

import numpy as np
import shapely
from stair_junction_body import build

FIXTURE = Path(__file__).with_name("data") / "world-stair-ports.json"
_PARTS = ("start_transition", "flight", "end_transition")


def prepare(capture):
    """Consume captured profile words and owned gate heights, without refitting."""
    source_stations = capture["stations_east_north_grade_node"]
    if any(len(row) != 4 for row in source_stations):
        raise ValueError("captured station layout required")
    stations = np.asarray([row[:3] for row in source_stations], dtype=float)
    gates = np.asarray([p["gate"] for p in capture["ports"]], dtype=float)
    positions = np.asarray(capture["reference_positions_east_up_south_m"]).reshape(
        -1, 3
    )
    indices = np.asarray(capture["reference_boundary_vertex_indices"])
    node_ids = capture["end_world_node_ids"]
    if (
        capture["profile"] != "simple"
        or not 2 <= len(stations) <= 4096
        or stations.shape[1:] != (3,)
        or gates.shape != (2, 8)
        or indices.shape != (2, 4)
        or indices.dtype.kind not in "iu"
        or indices.min() < 0
        or indices.max() >= len(positions)
        or len(node_ids) != 2
        or any(type(n) is not int or not 0 < n < 2**64 for n in node_ids)
        or not all(np.isfinite(a).all() for a in (stations, gates, positions))
        or not np.array_equal(positions, positions.astype(np.float32).astype(float))
    ):
        raise ValueError(
            "bounded captured World stations, ports and Float32 profiles required"
        )
    order = [0, 1] if gates[0, 2] < gates[1, 2] else [1, 0]
    gates, indices = gates[order], indices[order]
    if order[0] == 1:
        stations = stations[::-1]
    profiles = positions[indices][:, :, [0, 2, 1]].copy()
    profiles[:, :, 1] *= -1
    centres, heights = gates[:, :2], gates[:, 2]
    tangent = centres[1] - centres[0]
    length = float(np.linalg.norm(tangent))
    if not math.isfinite(length) or length <= 0:
        raise ValueError("positive World interval required")
    tangent /= length
    normal = np.array([-tangent[1], tangent[0]])
    local = stations[:, :2] - centres[0]
    along, across = local @ tangent, local @ normal
    width = float(capture["half_width_m"])
    if (
        not math.isfinite(width)
        or width <= 0
        or not np.array_equal(gates[:, 5], [width, width])
        or not np.array_equal(gates[:, 6:], [[0.1, 0.3], [0.1, 0.3]])
        or np.linalg.norm(gates[:, :2] - stations[[0, -1], :2], axis=1).max() > 0.05
        or np.max(abs(np.linalg.norm(gates[:, 3:5], axis=1) - 1)) > 1e-10
        or np.any((gates[:, 3:5] @ tangent) * [1, -1] < 0.99)
        or abs(across).max() > 0.05
        or np.any(np.diff(along) < 0)
        or np.any(along > length + 0.05)
    ):
        raise ValueError(
            "captured simple interval does not fit the native straight recipe"
        )
    landings = np.maximum(
        max(0.6, width), abs(profiles[:, :, 2] - heights[:, None]).max(axis=1) / 0.125
    )
    risers = math.ceil(float(heights[1] - heights[0]) / 0.179)
    if not 1 <= risers <= 64 or length - sum(landings) < 0.24 * risers:
        raise ValueError("native landing/run/rise recipe leaves no valid flight")
    start, end = float(landings[0]), float(length - landings[1])
    offsets = np.array([-width - 0.1, -width, width, width + 0.1])
    inner = np.array(
        [
            centres[0] + station * tangent + offsets[:, None] * normal
            for station in (start, end)
        ]
    )
    points = np.vstack(
        [
            profiles[0],
            np.c_[inner[0], np.full(4, heights[0])],
            np.c_[inner[1], np.full(4, heights[1])],
            profiles[1],
        ]
    )
    parts = []
    for row in range(3):
        faces = []
        for across in range(3):
            a, b = 4 * row + across, 4 * (row + 1) + across
            faces.extend([[a, b, a + 1], [a + 1, b, b + 1]])
        parts.append(np.asarray(faces))
    parent = f"capture:{capture['input_digest']}:way:{capture['source_way_index']}"
    rows = [
        {
            "source_id": f"{parent}:{role}",
            "parent_source_id": parent,
            "part_role": role,
            "origin": "generator_declaration",
        }
        for role in _PARTS
    ]
    words = (points[:, :2] - centres[0]) @ tangent
    words[4:8], words[8:12] = start, end
    plan = {
        "xy": points[:, :2],
        "parts": parts,
        "rows": rows,
        "stairs": [(1, 0, 1)],
        "nodes": inner.mean(axis=1),
        "stair_directions": {1: 1},
        "tread_partition_recipe": "indexed_band_remesh",
        "stair_lofts": [
            {
                "owner": 1,
                "source_id": rows[1]["source_id"],
                "origin_xy_m": centres[0],
                "tangent": tangent,
                "start_node": 0,
                "end_node": 1,
                "start_cut_m": start,
                "end_cut_m": end,
                "stations_m": words.tolist(),
            }
        ],
        "prepared_domains": [
            shapely.union_all(shapely.polygons(points[f, :2])) for f in parts
        ],
        "native_cut_context": {
            "profiles": [{"profile_xyz_m": p.tolist()} for p in profiles]
        },
    }
    return {
        "plan": plan,
        "heights": points[:, 2],
        "profiles_xyz_m": profiles,
        "captured_profile_indices": indices,
        "world_node_ids": [node_ids[i] for i in order],
        "capture": capture,
        "recipe": {
            "landing_lengths_m": landings.tolist(),
            "flight_run_m": end - start,
            "risers": risers,
            "maximum_original_axis_cross_deviation_m": float(abs(local @ normal).max()),
            "scope": "Existing native straight interval recipe; fixed owned gate heights and captured profiles, not a second height solution or source-role inference.",
        },
    }


def construct(capture):
    result = prepare(capture)
    body, data, geometry = build(result["plan"], result["heights"])
    lookup = {tuple(point): i for i, point in enumerate(body.vertices)}
    ports = []
    for end, (profile, node, indices) in enumerate(
        zip(
            result["profiles_xyz_m"],
            result["world_node_ids"],
            result["captured_profile_indices"],
            strict=True,
        )
    ):
        try:
            vertices = [lookup[tuple(point)] for point in profile]
        except KeyError as error:
            raise ValueError("body lost a captured exterior profile knot") from error
        ports.append(
            {
                "world_node_id": node,
                "source_id": result["plan"]["rows"][2 * end]["source_id"],
                "captured_vertex_ids": indices.tolist(),
                "body_vertex_ids": vertices,
            }
        )
    return dict(result, body=body, data=data, geometry=geometry, ports=ports)

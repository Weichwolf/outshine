"""Existing first-storey entrance selector on existing facade cells; no second grid."""

import numpy as np
import shapely
from outshine_ref.buildings.interiors.entrances import entrance
from outshine_ref.buildings.interiors.geometry import edges, rectangle


def cell_for(door, facades, polygon):
    centre, tangent = np.asarray(door["centre"]), np.asarray(door["tangent"])
    normal = np.array([tangent[1], -tangent[0]])
    matches = [
        f
        for f in facades
        if abs((centre - np.asarray(f["a"])) @ normal) < 1e-7
        and np.dot(np.asarray(f["b"]) - f["a"], tangent) > 0
        and 0 < (centre - np.asarray(f["a"])) @ tangent < f["width_m"]
    ]
    if len(matches) != 1:
        raise ValueError("Entrance has no unique existing facade owner")
    frame = matches[0]
    width = frame["width_m"] / frame["columns"]
    column = min(
        frame["columns"] - 1, int(((centre - np.asarray(frame["a"])) @ tangent) / width)
    )
    if width <= 1.10 + 2 * 0.055:
        raise ValueError("Existing cell cannot hold the door and casing")
    a = np.asarray(frame["a"]) + column * width * tangent
    b = a + width * tangent
    panel = shapely.Polygon([a, b, b - 0.24 * normal, a - 0.24 * normal])
    if panel.difference(polygon).area > 1e-8:
        raise ValueError("Door wall panel leaves the supplied footprint")
    return frame, column, width


def positions_for(polygon, facades):
    positions = {}
    for i, (a, b) in enumerate(edges(polygon, holes=False)):
        f = next(
            (
                f
                for f in facades
                if np.allclose([f["a"], f["b"]], [a, b], atol=1e-8, rtol=0)
            ),
            None,
        )
        if f is not None:
            cells = sorted(
                {round(t * (f["columns"] - 1)) for t in (0.0, 0.25, 0.5, 0.75, 1.0)}
            )
            positions[i] = tuple((c + 0.5) / f["columns"] for c in cells)
    return positions


class EntranceDetails:
    def __init__(self, rows, tree, states=None):
        self.rows, self.tree = rows, tree
        self.states = states or {}
        self.planned, self.rejected = [], []

    def clear(self, row, geometry, high):
        from provider_contract import attributes

        for i in self.tree.query(geometry, predicate="intersects"):
            other = self.rows[i]
            if other["id"] == row["id"]:
                continue
            p = attributes(other["properties"])
            if (
                p["minHeight"] < high
                and p["buildingHeight"] > 0
                and geometry.intersection(other["polygon"]).area > 1e-8
            ):
                return False
        return True

    def plan(self, row, parameters, facades, family):
        if family not in (
            "residential",
            "commercial",
            "mixed-use",
            "institutional",
        ) or row.get("clipped"):
            return None
        try:
            if parameters["minHeight"] != 0 or not facades:
                raise ValueError("No ground-datum exterior facade")
            floor = facades[0]["height_m"] / facades[0]["rows"]
            if floor - 0.18 < 0.16 + 2.15 + 0.055:
                raise ValueError(
                    "Existing first-storey interval leaves insufficient doorway headroom"
                )
            interior = row["polygon"].buffer(-0.24, join_style=2)
            clear = interior.buffer(-0.30, join_style=2)
            if (
                interior.geom_type != "Polygon"
                or clear.geom_type != "Polygon"
                or clear.is_empty
            ):
                raise ValueError("No connected provisional arrival space")

            def accept(door):
                try:
                    cell_for(door, facades, row["polygon"])
                except ValueError:
                    return False
                pocket = rectangle(door["centre"], door["tangent"], 1.40, -0.25, 1.84)
                return (
                    interior.covers(pocket)
                    and self.clear(row, door["landing"], 0.16 + door["height_m"])
                    and self.clear(row, pocket, 0.16 + door["height_m"])
                )

            door = entrance(
                row["polygon"],
                [interior],
                row.get("context", {}),
                fractions_by_edge=positions_for(row["polygon"], facades),
                accept=accept,
            )
            if door is None:
                raise ValueError(
                    "No existing-cell entrance with a clear landing and arrival pocket"
                )
            frame, column, width = cell_for(door, facades, row["polygon"])
            identifier = row["id"] + ":floor-0:entrance"
            angle = self.states.get(identifier, 0.0)
            if (
                isinstance(angle, (bool, str))
                or not np.isfinite(angle)
                or not 0 <= angle <= 90
            ):
                raise ValueError("Door state angle outside [0,90]")
            result = {
                "id": identifier,
                "door": door,
                "frame": frame,
                "column": column,
                "cell_width_m": width,
                "floor_m": floor,
                "interior": interior,
                "angle_deg": angle,
                "floor_z_m": 0.16,
                "wall_depth_m": 0.24,
            }
            self.planned.append(
                {
                    "id": identifier,
                    "source_id": row["id"],
                    "facade_id": frame["id"],
                    "cell": [column, 0],
                    "centre": door["centre"],
                    "tangent": door["tangent"],
                    "width_m": 1.10,
                    "height_m": door["height_m"],
                    "floor_z_m": 0.16,
                    "wall_depth_m": 0.24,
                    "landing_wkb": door["landing"].wkb_hex,
                    "arrival_space_wkb": interior.wkb_hex,
                    "origin": door["origin"],
                    "room_id": row["id"] + ":floor-0:room-0",
                    "scope": "Existing selector on existing cells; provisional open ground-floor arrival, upper-storey programme and external route unplanned",
                }
            )
            return result
        except (ValueError, shapely.errors.GEOSException) as error:
            self.rejected.append({"id": row["id"], "reason": str(error)})
            return None


def demand(entry, view):
    frame = entry["frame"]
    a = np.asarray(frame["a"])
    tangent = (np.asarray(frame["b"]) - a) / frame["width_m"]
    a = a + entry["column"] * entry["cell_width_m"] * tangent
    b = a + entry["cell_width_m"] * tangent
    points = np.array(
        [[*a, 0.0], [*b, 0.0], [*b, entry["floor_m"]], [*a, entry["floor_m"]]]
    )
    coordinates = (points - view.eye) @ view.basis.T
    distances = coordinates @ view.planes[:, :3].T + view.planes[:, 3]
    if np.any(
        distances.max(axis=0)
        + (view.movement_m + 0.24) * np.linalg.norm(view.planes[:, :3], axis=1)
        < 0
    ):
        return None
    nearest = float(coordinates[:, 2].min()) - view.movement_m - 0.24
    if nearest <= 0 or 1.10 * view.focal_px / nearest >= 6.0:
        return "through"
    return "compact" if 2.15 * view.focal_px / nearest >= 0.5 else None

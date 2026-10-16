"""Bounded optional near hall product; logical access remains independent of view/state."""

import numpy as np
import shapely
from facade_parts import append
from industrial_hall_details import products
from industrial_hall_plan import metadata, plan
from industrial_hall_shell import structure
from outshine_ref.buildings.roller_gates import lift


def demanded(value, view):
    region = shapely.from_wkb(value["footprint_wkb"])
    if value["floor_m"] <= view.eye[2] <= value["ceiling_m"] and region.covers(
        shapely.Point(view.eye[:2])
    ):
        return True
    a, b, c, d = region.bounds
    points = np.array(
        [
            [x, y, z]
            for x in (a, c)
            for y in (b, d)
            for z in (0, value["ceiling_m"] + 0.6)
        ]
    )
    q = (points - view.eye) @ view.basis.T
    distances = q @ view.planes[:, :3].T + view.planes[:, 3]
    margin = (view.movement_m + 0.30) * np.linalg.norm(view.planes[:, :3], axis=1)
    if np.any(distances.max(axis=0) + margin < 0):
        return False
    nearest = q[:, 2].min() - view.movement_m - 0.30
    return (
        nearest <= 0
        or min(value["gate"]["width_m"], value["gate"]["height_m"])
        * view.focal_px
        / nearest
        >= 8
    )


class HallDetails:
    def __init__(self, entrances, states=None, *, version="1"):
        if version not in ("1", "2"):
            raise ValueError("Hall recipe requires explicit supported version 1 or 2")
        self.version = version
        self.entrances = entrances
        self.states = states or {}
        self.planned, self.completed, self.rejected = [], [], []
        self.current = None

    def glazed_plan(self, row, parameters, value):
        from industrial_roof_choice import section_for
        from industrial_rooflights import plan as rooflights
        from tile_roof_envelopes import choose

        fields = rooflights(value, section_for(row, choose(row, parameters)))
        return value | {"rooflights": [dict(item) for item in fields]}

    def build(self, value, envelope, material, opened, view):
        if self.version == "2":
            from industrial_rooflights import cuts

            body, shell = structure(
                value,
                envelope,
                material,
                extra_cuts=cuts(value["rooflights"], material),
            )
        else:
            body, shell = structure(value, envelope, material)
        details, detailed = products(value, material, opened, view)
        if self.version == "2":
            from outshine_ref.buildings.rooflight_panels import closed

            details = [*details, *closed(value["rooflights"])]
        return append(body, details), shell, details, detailed

    def __call__(
        self, row, parameters, info, facades, entry, family, envelope, material, view
    ):
        self.current = None
        if family != "industrial":
            return None
        try:
            value = plan(row, parameters, info, facades, entry, self.entrances.clear)
            if value is None:
                return None
            if self.version == "2":
                value = self.glazed_plan(row, parameters, value)
            opened = self.states.get(value["gate"]["id"], 0.0)
            clear_height = lift(value["definition"], opened)
            logical = metadata(value)
            self.planned.append(logical)
            self.current = {
                "hall_plan": logical,
                "hall_render": "deferred",
                "hall_state": {"opened": opened, "clear_height_m": clear_height},
            }
            if self.version == "2":
                self.current["hall_recipe"] = "buildings.industrial-hall/2"
            if not demanded(value, view):
                return None
            if envelope is None:
                raise ValueError("Near hall requires its original typed roof envelope")
            result, shell, details, detailed = self.build(
                value, envelope, material, opened, view
            )
            self.completed.append(
                {
                    "id": row["id"],
                    "triangles": len(result["faces"]),
                    "shell_triangles": len(shell.mesh["faces"]),
                    "components": len(details),
                    "detailed_leaf": detailed,
                    "opened": opened,
                }
            )
            self.current |= {
                "hall_render": "constructed",
                "functional_status": "Source-bound single hall with real windows/personnel/loading openings and internal ramp; external route, controller and native integration unproved",
            }
            return result, info | self.current | {
                "rooms": 1,
                "room_ids": [[value["room_id"]]],
                "storeys": 1,
                "facade_recipe": family,
                "facade_plan": facades,
                "entrance_plan": self.entrances.planned[-1],
            }
        except (ValueError, shapely.errors.GEOSException) as error:
            self.rejected.append({"id": row["id"], "reason": str(error)})
            if self.current is not None:
                self.current |= {
                    "hall_render": "rejected",
                    "hall_rejection": str(error),
                }
            return None

    def report(self):
        return {
            "planned": self.planned,
            "completed": self.completed,
            "rejected": self.rejected,
        }

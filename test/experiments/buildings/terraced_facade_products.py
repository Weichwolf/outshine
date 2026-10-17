"""One coarse exterior and lazy closed near asset; no camera-dependent replanning."""

import math

import numpy as np
import shapely
from demand import displacement_error
from outshine_ref.buildings.modelling.prism_union import frozen
from terraced_facade_geometry import build_facade
from terraced_facade_surface import compact

EXTERIOR_RADIUS_M = 0.35
POSITION_LIMIT_PX = 0.5


class FacadeProducts:
    def __init__(self, prepared):
        self.prepared = prepared
        self.coarse = compact(prepared)
        self.near = None
        mass = prepared.mass
        self.source_polygon = shapely.from_wkb(mass.source_wkb)
        self.source_bounds = self.source_polygon.bounds
        xy = shapely.get_coordinates(
            shapely.union_all(
                [
                    self.source_polygon,
                    shapely.from_wkb(mass.entry.landing_wkb),
                ]
            )
        )
        lo, hi = xy.min(axis=0), xy.max(axis=0)
        self.bounds = frozen(
            np.array(
                [
                    (x, y, z)
                    for x in (lo[0], hi[0])
                    for y in (lo[1], hi[1])
                    for z in (mass.bottom_m, mass.top_m)
                ]
            )
        )

    def physical(self):
        if self.near is None:
            self.near = build_facade(self.prepared.mass, prepared=self.prepared)
        return self.near

    def select(self, view):
        mass = self.prepared.mass
        x0, y0, x1, y1 = self.source_bounds
        inside = (
            mass.bottom_m <= view.eye[2] <= mass.top_m
            and x0 <= view.eye[0] <= x1
            and y0 <= view.eye[1] <= y1
            and self.source_polygon.covers(shapely.Point(view.eye[:2]))
        )
        coordinates = (self.bounds - view.eye) @ view.basis.T
        margin = (view.movement_m + EXTERIOR_RADIUS_M) * np.linalg.norm(
            view.planes[:, :3], axis=1
        )
        visible = not np.any(
            (coordinates @ view.planes[:, :3].T + view.planes[:, 3]).max(axis=0)
            + margin
            < 0
        )
        error = (
            displacement_error(self.bounds, view, EXTERIOR_RADIUS_M)
            if visible
            else None
        )
        near = bool(
            inside or (visible and error > min(POSITION_LIMIT_PX, view.detail_error_px))
        )
        return {
            "mode": "near" if near else "compact",
            "possibly_visible": bool(visible),
            "inside_source": bool(inside),
            "exterior_position_bound_px": error
            if error is not None and math.isfinite(error)
            else None,
            "unbounded_position_error": error is not None and not math.isfinite(error),
            "scope": "Research exterior-position selection only; no radiance, transmission, shadow/reflection or native/async-frame admission. Physical asset lifetime is independent and remains caller-owned.",
        }

    def resolve(self, view):
        decision = self.select(view)
        return (
            self.physical() if decision["mode"] == "near" else self.coarse
        ), decision

"""Building pads and smooth aprons using the native earthwork profile."""

from dataclasses import dataclass

import numpy as np
import shapely

PLINTH_M = 0.5
SINK_M = 0.3
PAD_APRON_M = 6.0
MAX_EARTHWORK_M = 30.0
BATTER_RUN = 1.5
TRANSPORT_BLEND_M = 1.5


@dataclass(frozen=True)
class BuildingFoundation:
    footprint: shapely.Polygon
    platform_m: float
    sole_m: float
    apron_m: float

    @property
    def seat_m(self):
        return self.platform_m + PLINTH_M


def building_foundations(footprints, height):
    if not len(footprints):
        return []
    footprints = np.asarray(footprints)
    corners, owners = shapely.get_coordinates(
        shapely.get_exterior_ring(footprints), return_index=True
    )
    bounds = shapely.bounds(footprints).reshape(-1, 2, 2)
    dimensions = bounds[:, 1] - bounds[:, 0]
    large = np.flatnonzero(dimensions.max(axis=1) >= 20)
    if len(large):
        quarter = np.array([0.25, 0.5, 0.75])
        offsets = np.stack(np.meshgrid(quarter, quarter), axis=-1).reshape(9, 2)
        interior = bounds[large, :1] + dimensions[large, None] * offsets
        x, y = interior.transpose(2, 0, 1)
        inside = shapely.contains_xy(footprints[large, None], x, y)
        corners = np.concatenate((corners, interior[inside]))
        owners = np.concatenate(
            (owners, np.broadcast_to(large[:, None], inside.shape)[inside])
        )
    elevations = height(corners)
    low = np.full(len(footprints), np.inf)
    high = np.full(len(footprints), -np.inf)
    np.minimum.at(low, owners, elevations)
    np.maximum.at(high, owners, elevations)
    relief = high - low
    sole = low - np.where(relief > SINK_M, 2 * relief, SINK_M)
    apron = np.hypot(
        PAD_APRON_M, 1.875 * BATTER_RUN * np.minimum(relief, MAX_EARTHWORK_M)
    )
    return [
        BuildingFoundation(footprint, float(top), float(bottom), float(reach))
        for footprint, top, bottom, reach in zip(
            footprints, high, sole, apron, strict=True
        )
    ]


def smoothstep(fraction):
    t = np.clip(fraction, 0.0, 1.0)
    return t * t * t * (t * (6 * t - 15) + 10)


class BuildingStamps:
    def __init__(self, foundations, protected):
        self.footprints = np.asarray([f.footprint for f in foundations], dtype=object)
        self.platforms = np.asarray([f.platform_m for f in foundations])
        self.aprons = np.asarray([f.apron_m for f in foundations])
        self.protected = protected
        limits = shapely.bounds(self.footprints)
        extents = shapely.box(*(limits + self.aprons[:, None] * [-1, -1, 1, 1]).T)
        self.index = shapely.STRtree(extents)

    def breaks(self):
        active = self.aprons > np.hypot(PAD_APRON_M, 1.875 * BATTER_RUN * SINK_M)
        footprints = self.footprints[active]
        return np.concatenate(
            (
                shapely.boundary(footprints),
                shapely.boundary(
                    shapely.buffer(footprints, self.aprons[active] * 0.5, quad_segs=2)
                ),
            )
        )

    def heights(self, vertices):
        points = shapely.points(vertices[:, :2])
        original = vertices[:, 2]
        sample, stamp = self.index.query(points)
        distance = shapely.distance(points[sample], self.footprints[stamp])
        core = distance <= 1e-6
        platform = np.full(len(points), np.inf)
        np.minimum.at(platform, sample[core], self.platforms[stamp[core]])

        grade = ~core & (distance < self.aprons[stamp])
        fade = smoothstep(distance[grade] / self.aprons[stamp[grade]])
        weight = (1 - fade) / fade
        selected = sample[grade]
        correction = np.clip(
            self.platforms[stamp[grade]] - original[selected],
            -MAX_EARTHWORK_M,
            MAX_EARTHWORK_M,
        )
        total_weight = np.ones(len(points))
        displacement = np.zeros(len(points))
        np.add.at(total_weight, selected, weight)
        np.add.at(displacement, selected, weight * correction)
        updated = np.where(
            np.isfinite(platform), platform, original + displacement / total_weight
        )
        if self.protected.is_empty:
            return updated
        active = updated != original
        reserved_distance = shapely.distance(points[active], self.protected)
        updated[active] = original[active] + (
            updated[active] - original[active]
        ) * smoothstep(reserved_distance / TRANSPORT_BLEND_M)
        return updated

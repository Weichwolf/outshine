"""Material weights and plant exclusions for mapped mineral lake margins."""

from dataclasses import dataclass
from functools import cached_property

import numpy as np
import shapely

from outshine.vegetation.plant_sites import patch_density
from outshine.water.shore_grade import ShoreProtection, segment_index, smoothstep

MARGIN_REACH_M = 9.0
BARE_WIDTH_M = 7.0
SHORE_VIEW_M = 76.0
SHORE_TEXEL_M = 0.20
MINERAL_WET = ((0.024, 0.030, 0.029), 0.59)
MINERAL_CRUST = ((0.48, 0.46, 0.37), 0.94)
MINERAL_GRAVEL = ((0.049, 0.052, 0.047), 0.91)
CRUST_GRAIN = (13.0, -5.85)
SHORE_MATERIALS = frozenset(
    ("ground", "grass", "patagonian gravel soil", "regional soil")
)


@dataclass(frozen=True)
class ShoreAppearance:
    """Linear wetness, crust and gravel weights in RGB order."""

    pixels: np.ndarray
    transform: tuple[float, float, float, float]
    wet: tuple[tuple[float, float, float], float] = MINERAL_WET
    materials: frozenset[str] = SHORE_MATERIALS


@dataclass(frozen=True)
class MineralShore:
    contact: shapely.Geometry
    protection: ShoreProtection
    wet_sand: tuple[tuple[float, float, float], float] | None = None
    sand_material: str | None = None

    @cached_property
    def bare(self):
        if self.wet_sand is not None:
            return shapely.Polygon()
        return self.contact.buffer(BARE_WIDTH_M, quad_segs=4)

    def appearance(self, source, eye, seed):
        view = shapely.box(*(eye[:2] - SHORE_VIEW_M), *(eye[:2] + SHORE_VIEW_M))
        contact = self.contact.intersection(view.buffer(MARGIN_REACH_M, quad_segs=2))
        if contact.is_empty:
            return None
        strip = contact.buffer(MARGIN_REACH_M, quad_segs=4).intersection(view)
        if strip.is_empty:
            return None
        first, stop = np.asarray(strip.bounds).reshape(2, 2) / SHORE_TEXEL_M
        first = np.floor(first).astype(int) - 1
        stop = np.ceil(stop).astype(int) + 1
        x, y = [
            (np.arange(a, b) + 0.5) * SHORE_TEXEL_M
            for a, b in zip(first, stop, strict=True)
        ]
        xx, yy = np.meshgrid(x, y)
        mask = shapely.contains_xy(strip, xx, yy)
        points = shapely.points(xx[mask], yy[mask])
        index, _ = segment_index((contact,))
        if not len(index):
            return None
        rows, distance = index.query_nearest(
            points, max_distance=MARGIN_REACH_M, all_matches=False, return_distance=True
        )
        if not len(distance):
            return None
        chosen = rows[0]
        xy = shapely.get_coordinates(points[chosen])
        natural = self.protection.weight(points[chosen], xy, distance)
        edge = SHORE_VIEW_M - np.abs(xy - eye[:2]).max(axis=1)
        fade = smoothstep(np.clip(edge / 8, 0, 1)) * natural
        weights = (
            sand_shore_weights(source, xy, distance, seed)
            if self.wet_sand is not None
            else substrate_weights(source, xy, distance, seed)
        )
        pixels = np.zeros((len(y), len(x), 4), dtype=np.uint8)
        packed = np.rint(255 * fade[:, None] * np.column_stack(weights))
        pixels.reshape(-1, 4)[np.flatnonzero(mask)[chosen], :3] = packed
        pixels[..., 3] = 255
        span = SHORE_TEXEL_M * (stop - first)
        transform = (*(first * SHORE_TEXEL_M), *(1 / span))
        if self.wet_sand is not None:
            return ShoreAppearance(
                pixels, transform, self.wet_sand, frozenset((self.sand_material,))
            )
        return ShoreAppearance(pixels, transform)


def substrate_weights(source, xy, distance, seed):
    deposit = patch_density(source, xy, seed ^ 0x697A, scale=0.19)
    grains = patch_density(source, xy, seed ^ 0x20E1, scale=0.045)
    splash = distance - 0.66 * (deposit - 0.5)
    wet = 1 - smoothstep(np.clip((splash - 0.36) / 1.48, 0, 1))
    inner = smoothstep(np.clip((distance - 0.78) / 1.26, 0, 1))
    outer = 1 - smoothstep(np.clip((distance - 4.1) / 4.5, 0, 1))
    crust = smoothstep(np.clip((0.64 * deposit + 0.36 * grains - 0.30) / 0.40, 0, 1))
    gravel = outer * (0.84 + 0.15 * deposit)
    return wet, inner * outer * (0.08 + 0.85 * crust), gravel


def sand_shore_weights(source, xy, distance, seed):
    soak = patch_density(source, xy, seed ^ 0x73B2, scale=0.23)
    reach = 1.8 + 1.05 * soak
    wet = 1 - smoothstep(np.clip((distance - 0.12) / reach, 0, 1))
    dry = np.zeros_like(wet)
    return wet, dry, dry


def mineral_shore(grade, substrates, *, sand=None, sand_area=None):
    substrates = substrates or {}
    chosen = [
        contact
        for body, contact in zip(grade.bodies, grade.contacts, strict=True)
        if substrates.get((body.water_class, body.source_id)) == "carbonate-gravel"
    ]
    if chosen:
        return MineralShore(shapely.union_all(chosen), grade.protection)
    if sand is None or sand.wet is None or sand_area is None or not grade.contacts:
        return None
    contact = shapely.union_all(grade.contacts).intersection(sand_area.buffer(0.28))
    if contact.length == 0:
        return None
    return MineralShore(contact, grade.protection, (sand.wet, 0.68), sand.sand)

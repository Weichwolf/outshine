"""Real window cuts and reusable closed opening frames in the existing core scope."""

from dataclasses import dataclass, replace

import numpy as np
import shapely

from outshine_ref.buildings.modelling import PrismVolume
from outshine_ref.buildings.modelling.prism_cut import subtract_prisms
from terraced_mass_frame import local_plan, placed, placed_cut
from terraced_mass_rooms import build
from terraced_facade_plan import candidates
from terraced_facade_details import details


@dataclass(frozen=True)
class FacadeAsset:
    floors: object
    facade: object
    cuts: tuple
    frames: object


def build_facade(mass):
    local = local_plan(mass)
    floors = build(local)
    facade = candidates(local)
    cuts = tuple(
        PrismVolume(
            w.identifier,
            shapely.from_wkb(w.cutter_wkb),
            w.bottom_m,
            w.bottom_m + w.height_m,
            12,
        )
        for w in facade.windows
    )
    shell = subtract_prisms(
        local.identifier + "/shell", floors.shell, cuts, max_triangles=65536
    )
    frames = details(local, facade)
    points = np.array(frames["vertices"], copy=True)
    points[:, :2] = (
        np.asarray(mass.core.origin_xy)
        + points[:, :1] * np.asarray(mass.core.across_xy)
        + points[:, 1:2] * np.asarray(mass.core.along_xy)
    )
    frames = frames | {"vertices": points}
    current = replace(floors, shell=shell)
    world = replace(
        current,
        shell=placed(current.shell, mass.core),
        stairs=placed(current.stairs, mass.core),
        landing=placed(current.landing, mass.core),
        cuts=tuple(placed_cut(c, mass.core) for c in current.cuts),
    )
    world_cuts = tuple(placed_cut(c, mass.core) for c in cuts)
    across, along = np.asarray(mass.core.across_xy), np.asarray(mass.core.along_xy)
    windows = tuple(
        replace(
            w,
            centre=tuple(
                np.asarray(mass.core.origin_xy)
                + w.centre[0] * across
                + w.centre[1] * along
            ),
            tangent=tuple(w.tangent[0] * across + w.tangent[1] * along),
            cutter_wkb=c.polygon.wkb,
        )
        for w, c in zip(facade.windows, world_cuts, strict=True)
    )
    return FacadeAsset(world, replace(facade, windows=windows), world_cuts, frames)

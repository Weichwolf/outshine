"""Real window cuts and reusable closed opening frames in the existing core scope."""

from dataclasses import dataclass, replace

import shapely
from outshine_ref.buildings.modelling import PrismVolume
from outshine_ref.buildings.modelling.prism_cut import subtract_prisms
from terraced_facade_details import details
from terraced_facade_prepare import positions, prepare, world_facade
from terraced_mass_frame import placed, placed_cut
from terraced_mass_rooms import build


@dataclass(frozen=True)
class FacadeAsset:
    floors: object
    facade: object
    cuts: tuple
    frames: object


def build_facade(mass, *, prepared=None, portals=()):
    prepared = prepare(mass) if prepared is None else prepared
    if prepared.mass != mass:
        raise ValueError("Prepared facade belongs to a different source mass")
    local, facade = prepared.local, prepared.facade
    floors = build(local)
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
    cuts += tuple(
        PrismVolume(
            p.identifier,
            shapely.from_wkb(p.cutter_wkb),
            p.bottom_m,
            p.bottom_m + p.height_m,
            12,
        )
        for p in portals
    )
    shell = subtract_prisms(
        local.identifier + "/shell", floors.shell, cuts, max_triangles=65536
    )
    frames = details(local, facade, portals=portals)
    frames = frames | {"vertices": positions(frames["vertices"], mass.core)}
    current = replace(floors, shell=shell)
    world = replace(
        current,
        shell=placed(current.shell, mass.core),
        stairs=placed(current.stairs, mass.core),
        landing=placed(current.landing, mass.core),
        cuts=tuple(placed_cut(c, mass.core) for c in current.cuts),
    )
    world_cuts = tuple(placed_cut(c, mass.core) for c in cuts)
    return FacadeAsset(
        world,
        world_facade(prepared, world_cuts[: len(facade.windows)]),
        world_cuts,
        frames,
    )

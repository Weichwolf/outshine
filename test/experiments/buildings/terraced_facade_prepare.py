"""One immutable logical blueprint before any floor, Boolean or detail expansion."""

from dataclasses import dataclass, replace

import numpy as np

from terraced_facade_plan import candidates
from terraced_mass_frame import local_plan


@dataclass(frozen=True)
class PreparedFacade:
    mass: object
    local: object
    facade: object


def prepare(mass):
    local = local_plan(mass)
    return PreparedFacade(mass, local, candidates(local))


def positions(values, core):
    points = np.array(values, copy=True)
    points[:, :2] = (
        np.asarray(core.origin_xy)
        + points[:, :1] * np.asarray(core.across_xy)
        + points[:, 1:2] * np.asarray(core.along_xy)
    )
    return points


def world_facade(prepared, cuts):
    core, facade = prepared.mass.core, prepared.facade
    across, along = np.asarray(core.across_xy), np.asarray(core.along_xy)
    windows = tuple(
        replace(
            w,
            centre=tuple(
                np.asarray(core.origin_xy) + w.centre[0] * across + w.centre[1] * along
            ),
            tangent=tuple(w.tangent[0] * across + w.tangent[1] * along),
            cutter_wkb=c.polygon.wkb,
        )
        for w, c in zip(facade.windows, cuts, strict=True)
    )
    return replace(facade, windows=windows)

"""Inspect the installed fixed-field inset; no duplicated clipping implementation."""

from dataclasses import dataclass

import numpy as np

from outshine_ref.buildings.modelling.hip_inset import half_plane, prepare_hip_inset
from outshine_ref.buildings.modelling.hip_surface import prepare_even


@dataclass(frozen=True)
class InsetPatch:
    polygon: object
    gradient: tuple
    intercept_m: float
    owner: int
    precision_m: float


def inset(plan, value, mode, overhang, even=False):
    field = prepare_even(plan,mode) if even else plan
    prepared = prepare_hip_inset(field,value,mode,overhang)
    factor = field.factor(value,mode)
    patches = tuple(InsetPatch(p.polygon,tuple(np.asarray(p.gradient)*factor),p.intercept_m*factor,
                              p.facet,prepared.precision_m) for p in prepared.facets)
    return patches,prepared.eave_level_m

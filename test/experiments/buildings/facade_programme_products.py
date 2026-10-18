"""Explicit replacement reference: retain real openings, replace only owned trim."""

from copy import copy
from dataclasses import replace

import numpy as np
from bay_facade_mesh import expand
from facade_programme_plan import ATTIC, FIN, MAIN, PLINTH
from terraced_crown_geometry import capless
from terraced_facade_probe import facade_draw
from terraced_facade_surface import joined


def palette():
    values = {
        PLINTH: (0.25, 0.28, 0.29, 0.90, 0.0),
        MAIN: (0.63, 0.58, 0.48, 0.86, 0.0),
        ATTIC: (0.20, 0.24, 0.25, 0.68, 0.18),
        FIN: (0.32, 0.23, 0.13, 0.52, 0.65),
    }
    return {
        str(k): dict(
            zip(
                ("color.r", "color.g", "color.b", "roughness", "metallic"),
                v,
                strict=True,
            )
        )
        for k, v in values.items()
    }


def opening_frames(frames):
    # Role9 belongs only to the legacy accents in this specific component.
    keep = frames["materials"] != 9
    removed = set(frames["parts"][~keep])
    if removed & set(frames["parts"][keep]):
        raise ValueError("Legacy trim shares a closed component with a source opening")
    vertices, inverse = np.unique(frames["faces"][keep], return_inverse=True)
    return {
        "vertices": frames["vertices"][vertices],
        "faces": inverse.reshape(-1, 3).astype(np.uint32),
        "materials": frames["materials"][keep],
        "parts": frames["parts"][keep],
    }


def with_closed_layers(frames, planned):
    product, owners = expand(planned, mode="closed")
    offset = int(frames["parts"].max()) + 1 if len(frames["parts"]) else 0
    return {
        "vertices": np.r_[frames["vertices"], product["vertices"]],
        "faces": np.r_[frames["faces"], product["faces"] + len(frames["vertices"])],
        "materials": np.r_[frames["materials"], product["materials"]],
        "parts": np.r_[frames["parts"], owners + offset],
    }


class ProgrammeFacade:
    def __init__(self, source, planned):
        self.source, self.planned = source, planned
        self.near = None
        self.base = None

    def openings(self):
        if self.base is None:
            original = self.source.physical()
            self.base = replace(original, frames=opening_frames(original.frames))
        return self.base

    def physical(self):
        if not self.planned.cells:
            return self.source.physical()
        if self.near is None:
            base = self.openings()
            self.near = replace(
                base, frames=with_closed_layers(base.frames, self.planned)
            )
        return self.near


class ProgrammeProducts:
    def __init__(self, source, planned):
        if source.plan.body.identifier != planned.source_id:
            raise ValueError("Facade programme belongs to a different source body")
        self.source, self.plan = source, planned
        self.body = copy(source.body)
        self.body.facade = ProgrammeFacade(source.body.facade, planned)

    def draw(self, *, body_near=True, roof_near=True, selected_ids=None):
        if selected_ids is not None:
            selected_ids = frozenset(selected_ids)
            if not selected_ids <= {c.identifier for c in self.plan.cells}:
                raise ValueError("Programme selection contains a foreign component")
        if not self.plan.cells:
            return joined(self.source.parts(body_near, roof_near))
        facade = self.body.facade
        if body_near:
            body = facade_draw(facade.openings(), self.source.plan.body)
        else:
            original = facade.source.coarse.mesh
            keep = original["materials"] != 9
            body = dict(
                original,
                faces=original["faces"][keep],
                materials=original["materials"][keep],
            )
        pieces = (body, *self.body.additions)
        if not body_near:
            pieces += (self.body.portal_frames,)
        body = joined(pieces)
        if self.source.plan.crown is not None:
            body = capless(body, self.source.plan.body.top_m)
        addition, _owners = expand(
            self.plan, selected_ids, mode="outer" if body_near else "front"
        )
        pieces = (body, addition)
        if self.source.plan.crown is not None:
            if roof_near:
                self.source.physical_roof()
            pieces += (self.source.roof_skin if roof_near else self.source.coarse_roof,)
        return joined(pieces)

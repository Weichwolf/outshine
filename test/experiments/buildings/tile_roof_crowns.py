"""Prepare immutable crown products once, then choose only their draw representation."""

from dataclasses import dataclass, replace
from types import MappingProxyType

import numpy as np
import shapely
from clad_facade_mesh import CladFacades
from demand import displacement_error
from recipe_hall_operators import sealed
from roof_crown_choice import prepare
from roof_crown_mesh import envelope


@dataclass(frozen=True)
class Asset:
    plan: object
    full: object
    coarse: object


def frozen_envelope(value):
    return replace(
        value,
        mesh=sealed(value.mesh),
        vertex_roots=sealed(value.vertex_roots),
        source_triangles=sealed(value.source_triangles),
        surface_roles=sealed(value.surface_roles),
    )


def prepared(crown):
    return Asset(
        crown,
        frozen_envelope(envelope(crown)),
        frozen_envelope(envelope(crown, coarse=True)),
    )


def error(crown, view):
    p = shapely.from_wkb(crown.footprint_wkb)
    a, b, c, d = p.bounds
    points = np.array(
        [(x, y, z) for x in (a, c) for y in (b, d) for z in (0.0, crown.top_m)]
    )
    return displacement_error(points, view, crown.top_m - crown.deck_m)


def description(crown):
    return {
        "kind": crown.kind,
        "footprint_wkb": crown.footprint_wkb.hex(),
        "top_m": crown.top_m,
        "deck_m": crown.deck_m,
        "guard_m": crown.guard_m,
        "service_reason": crown.service_reason,
        "origin": "procedural source-contained roof programme; not supplied geometry/use",
        "regions": [
            {
                "name": r.name,
                "footprint_wkb": r.footprint_wkb.hex(),
                "low_m": r.low_m,
                "high_m": r.high_m,
                "material": r.material,
            }
            for r in crown.regions
        ],
    }


class CrownRoofs:
    def __init__(self, rows, view, previous):
        self.view, self.previous = view, previous
        self.sources = {r["id"]: r for r in rows if r["layer"] == "building"}
        self.assets, self.rejected, self.completed = {}, [], []
        self.current_envelope = None
        for identifier, row in self.sources.items():
            try:
                crown, reason = prepare(row)
                if crown is not None:
                    self.assets[identifier] = prepared(crown)
                else:
                    self.rejected.append({"id": identifier, "reason": reason})
            except (
                ValueError,
                NotImplementedError,
                shapely.errors.GEOSException,
            ) as failure:
                self.rejected.append({"id": identifier, "reason": str(failure)})

    def __call__(self, row, parameters, material):
        asset = self.assets.get(row["id"])
        if asset is None:
            result = self.previous(row, parameters, material)
            self.current_envelope = self.previous.current_envelope
            return result
        if row is not self.sources[row["id"]]:
            raise ValueError(
                "Crown draw requires its immutable prepared source snapshot"
            )
        crown = asset.plan
        projected = error(crown, self.view)
        mode = "coarse" if projected <= 0.5 else "full"
        self.current_envelope = getattr(asset, mode)
        mesh = dict(self.current_envelope.mesh)
        for key in ("parts", "groups", "face_tag_mask"):
            mesh.pop(key, None)
        mesh["materials"] = mesh["materials"].copy()
        mesh["materials"][mesh["materials"] == 1] = material
        info = {
            "roof_kind": "crown",
            "roof_rise_m": crown.top_m - crown.deck_m,
            "eave_m": crown.deck_m,
            "roof_crown_plan": description(crown),
            "roof_crown_product": mode,
            "roof_projected_error_px": projected,
            "functional_status": "body exterior; roof/service access and rooms unplanned",
        }
        self.completed.append({"id": row["id"], "product": mode, "error_px": projected})
        return mesh, info


class CrownFacades(CladFacades):
    def __init__(self, rows, view, **kwargs):
        rows = tuple(rows)
        if any(not isinstance(r, MappingProxyType) for r in rows):
            rows = sealed(rows)
        self.rows = rows
        super().__init__(rows, view, **kwargs)
        self.base = CrownRoofs(rows, view, self.base)

    def report(self):
        return super().report() | {
            "roof_crowns": self.base.completed,
            "roof_crown_rejected": self.base.rejected,
        }

"""Prepared wall relief on complete hall assets; source/state products remain shared."""

from dataclasses import dataclass

import numpy as np
from demand import displacement_error
from facade_parts import append
from industrial_relief_mesh import products
from industrial_relief_plan import choose
from prepared_hall_tile import RetainedDetails
from recipe_hall_operators import sealed


@dataclass(frozen=True, slots=True)
class PreparedRelief:
    plan: object
    full: object
    outer: object
    wall_bounds: object


def prepare(plan):
    full, outer = products(plan)
    low, bottom, high, top = plan.region.bounds
    local = np.array([[low, bottom], [high, bottom], [high, top], [low, top]])
    xy = np.asarray(plan.origin) + local[:, :1] * plan.tangent
    return PreparedRelief(plan, full, outer, sealed(np.c_[xy, local[:, 1]]))


def demanded(part, view):
    return (
        displacement_error(part.wall_bounds, view, part.plan.depth_m)
        > view.detail_error_px
    )


class ArticulatedDetails(RetainedDetails):
    def __init__(self, prepared, view, *, max_triangles=4096, **kwargs):
        super().__init__(prepared, view, **kwargs)
        self.reliefs, self.relief_rejected, self.relief_draws = {}, [], []
        for identifier, asset in prepared.assets.items():
            plans, rejected = choose(
                asset,
                self.source_rows[identifier],
                self.ordinary.facades.entrances.clear,
                max_triangles=max_triangles,
            )
            self.reliefs[identifier] = tuple(prepare(plan) for plan in plans)
            self.relief_rejected.extend(rejected)

    def draw_parts(self, identifier):
        return tuple(
            p.outer for p in self.reliefs[identifier] if demanded(p, self.view)
        )

    def __call__(self, row, parameters, material):
        result = super().__call__(row, parameters, material)
        if row["id"] not in self.reliefs:
            return result
        mesh, info = result
        chunks = self.draw_parts(row["id"])
        record = {
            "id": row["id"],
            "planned_facades": len(self.reliefs[row["id"]]),
            "rendered_facades": len(chunks),
            "added_triangles": sum(len(p["faces"]) for p in chunks),
            "kinds": sorted({p.plan.kind for p in self.reliefs[row["id"]]}),
            "physical_triangles": sum(
                len(p.full["faces"]) for p in self.reliefs[row["id"]]
            ),
            "origin": "procedural industrial grouping on unchanged source/window axes",
            "limits": "Known source-wing/neighbor clearance only; distance bound covers geometry, not shadow/radiance; transport/night/native admission unproved",
        }
        self.relief_draws.append(record)
        return (append(mesh, chunks) if chunks else mesh), info | {
            "industrial_exterior": record
        }

    def report(self):
        return super().report() | {
            "industrial_relief": self.relief_draws,
            "industrial_relief_rejected": self.relief_rejected,
        }

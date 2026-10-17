"""Optional hierarchical facade scopes on the existing ordinary/crown pipeline."""

from dataclasses import asdict

import shapely
from facade_axes import layout
from elevation_choice import choose
from elevation_details import ElevationBands, ElevationFrames, recesses
from elevation_mesh import compiled
from tile_facades import recipe_for
from tile_roof_crowns import CrownFacades
from tile_roof_envelopes import choose as roof_choice


class ElevationFacades(CrownFacades):
    def __init__(self, rows, view, **kwargs):
        super().__init__(rows, view, **kwargs)
        self.elevations, self.elevation_rejected, self.staged_entries = {}, [], {}
        self.source_elevations = {}
        self.frames = ElevationFrames(self.entrances)
        self.bands = ElevationBands(self.bands, self.source_elevations)

    def surface(self, mesh, row, parameters, rhythm, material):
        projected, facades, identifier = super().surface(
            mesh, row, parameters, rhythm, material
        )
        family = recipe_for(row, parameters)["family"]
        entry = self.entrances.plan(row, parameters, facades, family)
        self.staged_entries[row["id"]] = entry
        logical = parameters
        if "roof_crown_plan" not in parameters:
            roof = roof_choice(row, parameters)
            if roof.kind not in ("flat", "hip"):
                self.elevation_rejected.append(
                    {
                        "id": row["id"],
                        "reason": "existing gabled/special roof owner retained",
                    }
                )
                return projected, facades, identifier
            logical = parameters | {"eave_m": roof.eave_m}
        source_frames = layout(
            row,
            logical,
            (rhythm["width"], rhythm["height"]),
            rhythm["axis_m"],
            rhythm["floor_m"],
        )
        plans, reason = choose(
            row, logical, source_frames, family, entry, self.entrances
        )
        if not plans:
            self.elevation_rejected.append({"id": row["id"], "reason": reason})
            return projected, facades, identifier
        try:
            rebuilt, scoped = compiled(
                projected, facades, plans, self.binding, material
            )
        except (ValueError, shapely.errors.GEOSException) as failure:
            self.elevation_rejected.append({"id": row["id"], "reason": str(failure)})
            return projected, facades, identifier
        self.elevations[row["id"]] = plans
        self.source_elevations[row["id"]] = (facades, tuple(p.facade_id for p in plans))
        return rebuilt, scoped, identifier

    def plan_entry(self, row, parameters, facades, family):
        return self.staged_entries[row["id"]]

    def attach_recesses(self, *args, **kwargs):
        return recesses(*args, **kwargs)

    def __call__(self, row, parameters, material):
        mesh, info = super().__call__(row, parameters, material)
        plans = self.elevations.get(row["id"])
        if plans:
            info = info | {
                "facade_architecture_plan": [asdict(p) for p in plans],
                "facade_architecture_origin": "procedural source-stable vertical scopes; existing entry/projection owner retained",
            }
        return mesh, info

    def report(self):
        return super().report() | {
            "elevation_programmes": {
                key: [asdict(p) for p in plans]
                for key, plans in self.elevations.items()
            },
            "elevation_rejected": self.elevation_rejected,
        }

"""Optional hierarchical facade scopes on the existing ordinary/crown pipeline."""

from dataclasses import asdict, dataclass

import shapely
from elevation_choice import choose
from elevation_details import ElevationBands, ElevationFrames, recesses
from elevation_mesh import compiled
from facade_axes import layout
from tile_facades import recipe_for
from tile_roof_crowns import CrownFacades
from tile_roof_envelopes import choose as roof_choice


@dataclass(frozen=True)
class RoofFacadePlan:
    frames: tuple
    entrance: object
    elevations: tuple
    reason: str
    entrance_records: tuple


class ElevationFacades(CrownFacades):
    def __init__(self, rows, view, **kwargs):
        super().__init__(rows, view, **kwargs)
        self.elevations, self.elevation_rejected, self.staged_entries = {}, [], {}
        self.source_elevations = {}
        self.roof_staged_elevations = {}
        self.frames = ElevationFrames(self.entrances)
        self.bands = ElevationBands(self.bands, self.source_elevations)

    def stage_roof_facades(self, row, parameters):
        if row["id"] in self.roof_staged_elevations:
            raise ValueError("Source roof already has an unconsumed facade plan")
        recipe = recipe_for(row, parameters)
        rhythm, family = recipe["parameters"], recipe["family"]
        facades = layout(
            row,
            parameters,
            (rhythm["width"], rhythm["height"]),
            rhythm["axis_m"],
            rhythm["floor_m"],
        )
        counts = len(self.entrances.planned), len(self.entrances.rejected)
        try:
            entry = self.entrances.plan(row, parameters, facades, family)
            plans, reason = choose(
                row, parameters, facades, family, entry, self.entrances
            )
            upper = {p.facade_id: p.frames()[-1] for p in plans}
            frames = tuple(upper.get(f["id"], f) for f in facades)
            self.roof_staged_elevations[row["id"]] = RoofFacadePlan(
                tuple(facades), entry, plans, reason, self._entrance_records(counts)
            )
            return frames
        except BaseException:
            self.roof_staged_elevations.pop(row["id"], None)
            self._discard_entrances(self._entrance_records(counts))
            raise

    def _entrance_records(self, counts):
        return tuple(
            tuple(records[count:])
            for records, count in zip(
                (self.entrances.planned, self.entrances.rejected), counts, strict=True
            )
        )

    def _discard_entrances(self, owned):
        for records, selected in zip(
            (self.entrances.planned, self.entrances.rejected), owned, strict=True
        ):
            for record in selected:
                index = next(
                    (i for i, value in enumerate(records) if value is record), None
                )
                if index is not None:
                    del records[index]

    def discard_roof_facades(self, identifier):
        staged = self.roof_staged_elevations.pop(identifier, None)
        if staged is not None:
            self._discard_entrances(staged.entrance_records)

    def surface(self, mesh, row, parameters, rhythm, material):
        projected, facades, identifier = super().surface(
            mesh, row, parameters, rhythm, material
        )
        family = recipe_for(row, parameters)["family"]
        staged = self.roof_staged_elevations.pop(row["id"], None)
        if staged is None:
            entry = self.entrances.plan(row, parameters, facades, family)
            self.staged_entries[row["id"]] = entry
        else:
            entry = staged.entrance
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
        if staged is None:
            plans, reason = choose(
                row, logical, source_frames, family, entry, self.entrances
            )
        else:
            plans, reason = staged.elevations, staged.reason
            if tuple(source_frames) != staged.frames or tuple(facades) != staged.frames:
                raise ValueError(
                    "Physical facade differs from the staged roof facade plan"
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
            expected = {f["id"] for p in plans for f in p.frames()}
            actual = {
                f["id"]
                for f in info.get("facade_plan", ())
                if "architecture_scope" in f
            }
            if actual != expected:
                self.elevation_rejected.append(
                    {
                        "id": row["id"],
                        "reason": "later facade failure retained original exterior; scoped programme not applied",
                    }
                )
                self.elevations.pop(row["id"])
                self.source_elevations.pop(row["id"])
                return mesh, info
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

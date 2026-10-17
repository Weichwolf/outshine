"""Retain classic entry/projection owners, apply scoped windows to other elevations."""

from collections import defaultdict

from clad_facade_mesh import CladFrames
from tile_facade_recess import attach, attach_recesses


class ElevationFrames(CladFrames):
    def programmes(self, row, parameters, facades, family, entry):
        return super().programmes(
            row,
            parameters,
            [f for f in facades if "architecture_scope" not in f],
            family,
            entry,
        )


class ElevationBands:
    def __init__(self, previous, sources):
        self.previous = previous
        self.sources = sources

    def __getattr__(self, name):
        return getattr(self.previous, name)

    def __call__(
        self, mesh, row, parameters, facades, view, material, family, **kwargs
    ):
        original, replaced = self.sources.get(row["id"], (facades, ()))
        kwargs["skip_facades"] = (*kwargs.get("skip_facades", ()), *replaced)
        return self.previous(
            mesh, row, parameters, original, view, material, family, **kwargs
        )


def recesses(mesh, row, parameters, facades, view, glass, wall, **kwargs):
    classic = [f for f in facades if "architecture_scope" not in f]
    zones = defaultdict(list)
    for f in facades:
        if "architecture_scope" in f:
            zones[f["base_m"]].append(f)
    if not zones:
        return attach(mesh, row, parameters, facades, view, glass, wall, **kwargs)
    logical = sum(f["opening_count"] for f in facades)
    if logical > kwargs.get("max_cells", 4096):
        raise ValueError("Scoped source exceeds its complete logical window budget")
    mesh, records, rejected = attach(
        mesh, row, parameters, classic, view, glass, wall, **kwargs
    )
    for bottom, frames in zones.items():
        top = frames[0]["top_m"]
        if any(
            f["top_m"] != top
            or f["scope_wall_material"] != frames[0]["scope_wall_material"]
            for f in frames
        ):
            raise ValueError("Scope group lacks common height/material semantics")
        mesh, actual, failures = attach_recesses(
            mesh,
            row,
            parameters
            | {"minHeight": bottom, "buildingHeight": top, "bodyHeight": top - bottom},
            frames,
            view,
            glass,
            frames[0]["scope_wall_material"],
            plain_wall_material=kwargs["plain_wall_material"],
            max_cells=sum(f["opening_count"] for f in frames),
        )
        records.extend(actual)
        rejected.extend(failures)
    return mesh, records, rejected

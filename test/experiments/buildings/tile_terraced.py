"""Scoped admitted mass replacements over unchanged previous whole-city source owners."""

import numpy as np
from terraced_facade_probe import materials
from terraced_mass_census import source_identity
from terraced_mass_plan import plan as mass_plan
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan

COMPONENT_MATERIALS = {9: 60000, 10: 60001, 11: 60002}


def definitions():
    values = materials()
    return {
        str(target): values[str(source)]
        for source, target in COMPONENT_MATERIALS.items()
    }


def bound(mesh, material):
    roles = mesh["materials"]
    output = roles.copy()
    output[roles == 12] = material
    for role, target in COMPONENT_MATERIALS.items():
        output[roles == role] = target
    return mesh | {"materials": output}


class TerracedDetails:
    def __init__(self, previous, records):
        self.previous, self.records = previous, records
        self.owner = previous.owner
        self.sources = self.owner.source_rows
        self.assets, self.completed = {}, []
        self.full_near = False

    def __call__(self, row, parameters, material):
        identifier = row["id"]
        reference = self.records.get(identifier)
        if reference is None:
            return self.previous(row, parameters, material)
        if row is not self.sources[identifier] or any(
            reference[k] != v for k, v in source_identity(row).items()
        ):
            raise ValueError("Terraced draw requires its unchanged prepared source")
        if identifier not in self.assets:
            self.assets[identifier] = TerraceProducts(
                plan(mass_plan(row, reference["programme"]))
            )
        asset = self.assets[identifier]
        decision = asset.facade.select(self.owner.view)
        near = self.full_near or decision["mode"] == "near"
        mesh = bound(asset.draw(near), material)
        mass = asset.plan.prepared.mass
        if not np.array_equal(
            [mesh["vertices"][:, 2].min(), mesh["vertices"][:, 2].max()],
            [mass.bottom_m, mass.top_m],
        ):
            raise ValueError("Terraced draw changed the supplied source height")
        self.completed.append(
            {
                "id": identifier,
                "programme": mass.kind,
                "mode": "near" if near else "compact",
                "normal_demand": decision,
                "triangles": len(mesh["faces"]),
                "physical_reference": reference["checks"]["scope"],
                "portals": [p.identifier for p in asset.plan.portals],
                "unserved_terraces": [
                    t.identifier for t in asset.plan.terraces if t.reason
                ],
            }
        )
        return mesh, {
            "form": "source-height-contained terraced mass with simple open floors and optional guarded portals",
            "programme": mass.kind,
            "product": "near" if near else "compact",
            "functional_status": reference["checks"]["scope"],
        }

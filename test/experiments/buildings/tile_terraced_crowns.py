"""Coupled highest roofs over unchanged whole-window source owners, offline joins."""

import numpy as np
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import ROOF_MATERIAL, plan
from terraced_facade_surface import joined
from terraced_mass_census import source_identity
from tile_terraced import bound, definitions


def crown_definitions():
    return definitions() | {
        str(ROOF_MATERIAL): {
            "color.r": 0.26,
            "color.g": 0.29,
            "color.b": 0.30,
            "roughness": 0.84,
            "metallic": 0.0,
        }
    }


class CrownedDetails:
    def __init__(self, previous, records):
        if any(
            r.get("passed") is not True
            or r.get("full_body_checked") is not True
            or r.get("roof_components_passed") is not True
            for r in records.values()
        ):
            raise ValueError("Crowned city needs both independent roof and body checks")
        self.previous, self.records = previous, records
        self.owner = previous.owner
        self.assets, self.completed = {}, []
        self.full_near = False

    def __call__(self, row, parameters, material):
        identifier = row["id"]
        reference = self.records.get(identifier)
        if reference is None:
            return self.previous(row, parameters, material)
        if row is not self.owner.source_rows[identifier] or any(
            reference[k] != v for k, v in source_identity(row).items()
        ):
            raise ValueError("Crowned draw requires its unchanged prepared source")
        if identifier not in self.assets:
            self.assets[identifier] = RoofedProducts(plan(row, reference["programme"]))
        asset = self.assets[identifier]
        pieces, decision = asset.resolve(self.owner.view)
        if self.full_near:
            pieces = asset.parts(True, True)
        mesh = bound(joined(pieces), material)
        mass = asset.plan.body
        if not np.array_equal(
            [mesh["vertices"][:, 2].min(), mesh["vertices"][:, 2].max()],
            [mass.bottom_m, asset.plan.source_top_m],
        ):
            raise ValueError("Crowned draw changed its source height interval")
        self.completed.append(
            {
                "id": identifier,
                "programme": mass.kind,
                "mode": "near" if self.full_near else decision["mode"],
                "normal_demand": decision,
                "triangles": len(mesh["faces"]),
                "physical_reference": reference["checks"]["scope"],
                "roof_kind": asset.plan.crown.kind if asset.plan.crown else "bare",
                "body_expanded": asset.body.facade.near is not None,
                "roof_expanded": asset.roof_components is not None,
                "portals": [p.identifier for p in asset.body.plan.portals],
                "unserved_terraces": [
                    t.identifier for t in asset.body.plan.terraces if t.reason
                ],
            }
        )
        return mesh, {
            "form": "source-height-contained terraced body and highest roof",
            "programme": mass.kind,
            "product": self.completed[-1]["mode"],
            "functional_status": reference["checks"]["scope"],
        }

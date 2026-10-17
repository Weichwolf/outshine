"""Prepare valid hall assets once, then compose all source rows with retained references."""

from dataclasses import dataclass
from types import MappingProxyType

from facade_parts import append
from industrial_hall_plan import metadata
from industrial_roof_choice import KINDS
from prepared_hall import HallInstance, prepare_asset
from provider_contract import attributes
from recipe_hall_operators import inputs_for, sealed
from recipe_rooflight_operators import prepare
from tile_roof_envelopes import choose
from tile_stepped import SteppedDetails


@dataclass(frozen=True, slots=True)
class PreparedHalls:
    rows: tuple
    assets: object
    rejected: tuple


def prepare_halls(rows):
    rows = sealed(tuple(rows))
    if len({r["id"] for r in rows}) != len(rows):
        raise ValueError("Prepared source batch requires unique identities")
    graph = prepare()
    assets, rejected = {}, []
    for row in rows:
        if row["layer"] != "building":
            continue
        try:
            p = attributes(row["properties"])
            if choose(row, p).kind not in KINDS or row["polygon"].area <= 800:
                continue
            neighbors = [
                r for r in rows if r["id"] != row["id"] and r["layer"] == "building"
            ]
            run = graph.evaluate(
                inputs_for(row, neighbors),
                outputs=("programme", "rooflights", "shell", "panels"),
            )
            assets[row["id"]] = prepare_asset(run)
        except ValueError as error:
            rejected.append({"id": row["id"], "reason": str(error)})
    return PreparedHalls(rows, MappingProxyType(assets), sealed(rejected))


class RetainedDetails:
    def __init__(
        self,
        prepared,
        view,
        *,
        opened=0.0,
        staff_angle=0.0,
        reference_full_fields=False,
    ):
        self.prepared = prepared
        self.view, self.opened, self.staff_angle = view, opened, staff_angle
        self.reference_full_fields = reference_full_fields
        self.source_rows = {r["id"]: r for r in prepared.rows}
        self.instances = {
            key: HallInstance(asset) for key, asset in prepared.assets.items()
        }
        self.ordinary = SteppedDetails(prepared.rows, view)
        self.completed = []

    def __call__(self, row, parameters, material):
        if row is not self.source_rows.get(row["id"]):
            raise ValueError(
                "Retained scene must use its immutable prepared source snapshot"
            )
        instance = self.instances.get(row["id"])
        if instance is None:
            return self.ordinary(row, parameters, material)
        draw = instance.update(
            self.view, opened=self.opened, staff_angle=self.staff_angle
        )
        parts = draw.render_parts
        if self.reference_full_fields and len(draw.sheet_mask):
            parts = parts[: -len(draw.sheet_mask)] + instance.asset.full_panels
        raw = append(draw.body, parts)
        materials = raw["materials"].copy()
        materials[materials == 1] = material
        logical = metadata(instance.asset.plan) | {
            "rooflights": [
                dict(p) for p in instance.asset.run.outputs["rooflights"].value
            ]
        }
        record = {
            "id": row["id"],
            "fields": len(draw.sheet_mask),
            "sheet_fields": 0
            if self.reference_full_fields
            else int(draw.sheet_mask.sum()),
            "triangles": len(raw["faces"]),
            "generated_moving_components": instance.generated_components,
        }
        self.completed.append(record)
        return dict(raw, materials=materials), {
            "hall_recipe": "buildings.industrial-hall/2",
            "hall_render": "retained",
            "hall_plan": logical,
            "room_ids": [[instance.asset.plan["room_id"]]],
            "rooms": 1,
            "storeys": 1,
            "facade_recipe": "industrial",
            "hall_state": {"opened": self.opened, "staff_angle_deg": self.staff_angle},
            "retained_product": record,
            "functional_status": "Retained full source shell and independent physical detail; cached full/sheet draw parts, original room/access identities. External route/controller/night/native integration unproved",
        }

    def report(self):
        return {
            "prepared_halls": len(self.instances),
            "prepared_rejected": self.prepared.rejected,
            "completed": self.completed,
            "ordinary": self.ordinary.report(),
        }

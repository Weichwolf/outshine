"""Stable source-selected podium assets; camera demand chooses prepared draw products."""

import shapely
from podium_plan import PodiumUnavailable
from podium_products import bound, prepare
from provider_contract import attributes, seed_for
from recipe_hall_operators import sealed
from stepped_facade_details import demand, opening_records
from tile_room_details import numeric_properties
from tile_stepped import SteppedDetails


def eligible(row):
    if row["layer"] != "building" or row.get("clipped", False):
        return False
    p = attributes(numeric_properties(row))
    if any(
        key.startswith("roof") and value is not None
        for key, value in row["properties"].items()
    ):
        return False
    return (
        not p["hide3d"]
        and p["minHeight"] == 0
        and 9 <= p["buildingHeight"] <= 35
        and 180 < row["polygon"].area < 1600
        and len(shapely.get_coordinates(row["polygon"])) <= 48
        and row.get("context", {}).get("site_use", {}).get("value")
        in ("residential", "commercial")
    )


class PodiumDetails(SteppedDetails):
    def __init__(self, rows, view, *, progress=None):
        self.rows = sealed(tuple(rows))
        super().__init__(self.rows, view)
        self.source_rows = {r["id"]: r for r in self.rows if r["layer"] == "building"}
        self.assets, self.podium_rejected, self.podium_requested = {}, [], []
        for row in self.source_rows.values():
            try:
                if not eligible(row) or seed_for(row["id"] + "/podium") % 4:
                    continue
                self.podium_requested.append(row["id"])
                first = (
                    "podium"
                    if seed_for(row["id"] + "/podium-kind") % 2 == 0
                    else "wing"
                )
                for kind in (first, "wing" if first == "podium" else "podium"):
                    try:
                        self.assets[row["id"]] = prepare(row, kind)
                        break
                    except PodiumUnavailable as error:
                        self.podium_rejected.append(
                            {
                                "id": row["id"],
                                "kind": kind,
                                "reason": str(error),
                                "attempts": error.attempts,
                            }
                        )
                if progress is not None:
                    progress(len(self.podium_requested), len(self.assets), row["id"])
            except (ValueError, AssertionError, shapely.errors.GEOSException) as error:
                self.podium_rejected.append({"id": row["id"], "reason": str(error)})
        self.selected |= self.assets.keys()

    def __call__(self, row, parameters, material):
        asset = self.assets.get(row["id"])
        if asset is None:
            return super().__call__(row, parameters, material)
        if row is not self.source_rows[row["id"]]:
            raise ValueError(
                "Podium draw requires its immutable prepared source snapshot"
            )
        plan = asset.plan
        key = (material, 1.0, 1.0)
        if key not in self.facades.palette:
            identifier = 32768 + len(self.facades.palette)
            self.facades.palette[key] = identifier
            self.facades.bindings[str(identifier)] = {
                "wall_material": material,
                "fractions": [1.0, 1.0],
                "filtered": True,
            }
        resolved, choice = demand(plan, self.view)
        mesh = bound(
            asset.near_draw if resolved else asset.compact_draw,
            material,
            self.facades.palette[key],
        )
        self.plans[row["id"]] = plan
        self.completed.append(
            {
                "id": row["id"],
                "storeys": plan["storeys"],
                "room_ids": plan["storey_room_ids"],
                "form_recipe": plan["form_recipe"],
                "terrace_access": plan["terrace_access"],
                "access": asset.physical[1],
                "terrace_mesh_paths": asset.physical[2],
                "facade_presentation": choice
                | {"mode": "near" if resolved else "compact"},
                "openings": opening_records(plan)
                if resolved
                else asset.compact_openings,
                "triangles": len(mesh["faces"]),
                "scope": "Prepared source mass and sampled physical access; no general cache/native/controller admission",
            }
        )
        return mesh, {
            "form": "source-contained podium and repeated upper-storey mass",
            "form_recipe": plan["form_recipe"],
            "room_ids": plan["storey_room_ids"],
            "roof_origin": "procedural source-height-contained flat terraces/parapets",
        }

    def report(self):
        return super().report() | {
            "podium_requested": self.podium_requested,
            "podium_assets": sorted(self.assets),
            "podium_rejected": self.podium_rejected,
            "scope": "Source-selected prepared podium assets plus existing legacy fallback. New mass choices are camera-independent; legacy membership remains its prior experiment policy.",
        }

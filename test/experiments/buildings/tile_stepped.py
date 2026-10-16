"""Bounded real stepped controls retain source identity and complete outside/inside plans."""

import numpy as np
import shapely
from provider_contract import attributes
from stepped_facade_details import demand as facade_demand
from stepped_facade_details import opening_records, present
from stepped_form_profiles import profile_for
from stepped_openings import apply as opening_patches
from stepped_source import exterior, plan_and_near
from tile_facades import FacadeDetails
from tile_room_details import numeric_properties


def candidates(rows, view, limit=8):
    ranked = []
    for row in rows:
        if row["layer"] != "building" or row["clipped"]:
            continue
        try:
            p = attributes(numeric_properties(row))
            polygon = row["polygon"]
            if (
                p["hide3d"]
                or p["minHeight"] != 0.0
                or not 9 <= p["buildingHeight"] <= 35
                or not 180 < polygon.area < 1600
            ):
                continue
            if row.get("context", {}).get("site_use", {}).get("value") not in (
                "residential",
                "commercial",
            ):
                continue
            if len(shapely.get_coordinates(polygon)) > 48:
                continue
            x0, y0, x1, y1 = polygon.bounds
            points = np.array(
                [
                    (x, y, z)
                    for x in (x0, x1)
                    for y in (y0, y1)
                    for z in (0.0, p["buildingHeight"])
                ]
            )
            q = (points - view.eye) @ view.basis.T
            if q[:, 2].min() <= 0:
                continue
            uv = q[:, :2] * view.focal_px / q[:, 2:3] + [
                view.width / 2,
                view.height / 2,
            ]
            low = np.maximum(uv.min(axis=0), 0.0)
            high = np.minimum(uv.max(axis=0), [view.width, view.height])
            if np.any(high <= low):
                continue
            ranked.append((float(np.prod(high - low)), row["id"]))
        except ValueError:
            continue
    return {
        identifier
        for score, identifier in sorted(ranked, key=lambda item: (-item[0], item[1]))[
            :limit
        ]
    }


class SteppedDetails:
    def __init__(self, rows, view):
        self.facades = FacadeDetails(rows, view)
        self.selected = candidates(rows, view)
        self.completed = []
        self.rejected = []
        self.plans = {}
        self.view = view

    def __call__(self, row, parameters, material):
        if row["id"] in self.selected:
            try:
                plan, (detail, proof, paths, _light) = plan_and_near(
                    row, profile_for(row["id"])
                )
                self.plans[row["id"]] = plan
                key = (material, 1.0, 1.0)
                if key not in self.facades.palette:
                    identifier = 32768 + len(self.facades.palette)
                    self.facades.palette[key] = identifier
                    self.facades.bindings[str(identifier)] = {
                        "wall_material": material,
                        "fractions": [1.0, 1.0],
                        "filtered": True,
                    }
                try:
                    resolved, presentation = facade_demand(plan, self.view)
                    if resolved:
                        mesh = present(detail, material, self.facades.palette[key])
                        openings = opening_records(plan)
                        comparison = "validated-near-assembly"
                    else:
                        mesh, _envelope, _guard, comparison = exterior(plan, material)
                        mesh, openings = opening_patches(
                            plan, mesh, self.facades.palette[key]
                        )
                    presentation["mode"] = "near" if resolved else "compact"
                except (ValueError, shapely.errors.GEOSException) as error:
                    mesh, _envelope, _guard, comparison = exterior(plan, material)
                    mesh, openings = opening_patches(
                        plan, mesh, self.facades.palette[key]
                    )
                    presentation = {"mode": "compact-fallback", "reason": str(error)}
                record = {
                    "id": row["id"],
                    "storeys": plan["storeys"],
                    "room_ids": plan["storey_room_ids"],
                    "form_recipe": plan["form_recipe"],
                    "terrace_access": plan["terrace_access"],
                    "access": proof,
                    "terrace_mesh_paths": paths,
                    "exterior_method": comparison,
                    "facade_presentation": presentation,
                    "openings": openings,
                    "triangles": len(mesh["faces"]),
                    "scope": "Complete stepped outer product from usable generic room/core plan; actual native player/frame and use programme remain unproved",
                }
                self.completed.append(record)
                return mesh, {
                    "form": "Stepped source-storey envelope with connected generic room plan",
                    "form_recipe": plan["form_recipe"],
                    "room_ids": plan["storey_room_ids"],
                    "roof_origin": "procedural stepped flat roof/parapet within source height",
                    "functional_status": record["scope"],
                }
            except (ValueError, AssertionError, shapely.errors.GEOSException) as error:
                self.rejected.append({"id": row["id"], "reason": str(error)})
        return self.facades(row, parameters, material)

    def report(self):
        return {
            "selected": len(self.selected),
            "completed": self.completed,
            "rejected": self.rejected,
            "scope": "At most eight source-derived stepped prototype plans per tile; no completed architecture-family or native LOD quota",
        }

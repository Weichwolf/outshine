"""Context-scoped compact facade plans on existing complete source envelopes."""

import hashlib
import time
from collections import Counter

import mapbox_vector_tile
import numpy as np
from context import attach
from facade_axes import layout
from recipes import choose
from samples import SOURCES
from tile_roof_envelopes import OrdinaryDetails


def contexts(rows, receipt):
    r = receipt["receipt"]
    path = SOURCES / (hashlib.sha256(r["url"].encode()).hexdigest() + ".pbf")
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != r["sha256"]:
        raise ValueError("Facade context provider bytes changed")
    layers = mapbox_vector_tile.decode(data, default_options={"y_coord_down": True})
    span = receipt["span_m"]
    bodies = [r for r in rows if r["layer"] == "building"]

    def project(points, extent):
        return (points / extent - 0.5) * [span, -span]

    return attach(bodies, layers, project, r["sha256"])


def project(mesh, row, parameters, fractions, axis_m, floor_m):
    triangles = mesh["vertices"][mesh["faces"]]
    vectors = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals = vectors / np.linalg.norm(vectors, axis=1, keepdims=True)
    walls = abs(normals[:, 2]) < 1e-8
    bottom = parameters["minHeight"]
    uv = mesh["uv"][mesh["faces"]].copy()
    extent = np.zeros((len(triangles), 3, 3))
    assigned = np.zeros(len(triangles), dtype=bool)
    facades = []
    for frame in layout(row, parameters, fractions, axis_m, floor_m):
        a, b = np.asarray(frame["a"]), np.asarray(frame["b"])
        length, height = frame["width_m"], frame["height_m"]
        columns, rows = frame["columns"], frame["rows"]
        tangent = (b - a) / length
        normal = np.array([tangent[1], -tangent[0]])
        distance = (triangles[:, :, :2] - a) @ normal
        along = (triangles[:, :, :2] - a) @ tangent
        selected = (
            walls
            & ((normals[:, :2] @ normal) > 0.999999)
            & (abs(distance).max(axis=1) < 1e-7)
            & (along.min(axis=1) >= -1e-7)
            & (along.max(axis=1) <= length + 1e-7)
        )
        if not selected.any():
            continue
        if np.any(assigned & selected):
            raise ValueError("Facade triangle has multiple source boundary owners")
        uv[selected, :, 0] = along[selected] / (length / columns)
        uv[selected, :, 1] = (triangles[selected, :, 2] - bottom) / (height / rows)
        extent[selected] = [columns, rows, 0]
        assigned |= selected
        facades.append(frame | {"id": row["id"] + f"/facade-{len(facades)}"})
    if np.any(walls & ~assigned):
        raise ValueError("Exterior wall lacks a source facade frame")
    if not assigned.any():
        raise ValueError("No positive exterior facade in complete source envelope")
    result = {
        key: value.copy()
        for key, value in mesh.items()
        if key not in ("vertices", "faces", "uv", "normals")
    }
    result.update(
        vertices=triangles.reshape(-1, 3),
        faces=np.arange(triangles.size // 3, dtype=np.uint32).reshape(-1, 3),
        normals=mesh["normals"][mesh["faces"]].reshape(-1, 3),
        uv=uv.reshape(-1, 2),
        facade_extent=extent.reshape(-1, 3),
    )
    if not np.array_equal(result["vertices"][result["faces"]], triangles):
        raise ValueError("Facade binding changed original source triangles")
    return result, assigned, facades


def recipe_for(row, parameters):
    return choose(
        row["properties"],
        row.get("context", {}),
        row["polygon"],
        parameters["buildingHeight"],
        parameters["minHeight"],
    )


class FacadeDetails:
    def __init__(self, rows, view, *, door_states=None, gate_states=None):
        from facade_balconies import BalconyDetails
        from facade_entrances import EntranceDetails
        from facade_frame_geometry import FrameDetails
        from facade_shopfronts import ShopfrontDetails
        from facade_window_bays import WindowBayDetails
        from tile_facade_bands import BandDetails
        from tile_industrial_halls import HallDetails

        self.base = OrdinaryDetails(rows, view)
        self.palette = {}
        self.bindings = {}
        self.completed = []
        self.rejected = []
        self.view = view
        self.recesses = []
        self.recess_rejected = []
        self.bands = BandDetails(rows)
        self.entrances = EntranceDetails(self.bands.rows, self.bands.tree, door_states)
        self.halls = HallDetails(self.entrances, gate_states)
        self.frames = FrameDetails(self.entrances)
        self.entrance_geometry = []
        self.shopfronts = ShopfrontDetails(self.entrances)
        self.shopfront_geometry = []
        self.balconies = BalconyDetails(self.entrances, door_states)
        self.balcony_geometry = []
        self.bays = WindowBayDetails(self.entrances)
        self.bay_geometry = []

    def __call__(self, row, parameters, material):
        before = self.base(row, parameters, material)
        if before is None:
            from tile_buildings import prism

            before = prism(row, parameters, material)
        mesh, info = before
        if "room_ids" in info:
            return before
        began = time.perf_counter()
        recipe = recipe_for(row, parameters)
        family = recipe["family"]
        rhythm = recipe["parameters"]
        if family in ("unknown", "utility", "garage"):
            return before
        try:
            projected, facades, identifier = self.surface(
                mesh, row, parameters | info, rhythm, material
            )
            entry = self.entrances.plan(row, parameters, facades, family)
            hall = self.halls(
                row,
                parameters,
                info,
                facades,
                entry,
                family,
                self.base.current_envelope,
                material,
                self.view,
            )
            if hall is not None:
                return hall
            shopfront = self.shopfronts.plan(row, entry)
            bays, balconies, programme = self.projections(row, entry, family)
            from tile_facade_recess import attach as recesses

            projected, details, rejected = recesses(
                projected,
                row,
                parameters | info,
                facades,
                self.view,
                lambda: self.binding(material, (1.0, 1.0)),
                identifier,
                plain_wall_material=material,
                entrance=entry,
                shopfront=shopfront,
                shop_material=lambda fractions: self.binding(material, fractions),
                balconies=balconies,
                bays=bays,
            )
            self.recesses.extend(
                r
                for r in details
                if r.get("kind")
                not in ("entrance", "shopfront", "balcony", "window_bay")
            )
            self.entrance_geometry.extend(
                r for r in details if r.get("kind") == "entrance"
            )
            self.shopfront_geometry.extend(
                r for r in details if r.get("kind") == "shopfront"
            )
            self.recess_rejected.extend(rejected)
            self.balcony_geometry.extend(
                r for r in details if r.get("kind") == "balcony"
            )
            self.bay_geometry.extend(
                r for r in details if r.get("kind") == "window_bay"
            )
            frame_parts, replaced_frames = self.frames.products(
                row, parameters, facades, self.view, material, family, entry
            )
            projected = self.bands(
                projected,
                row,
                parameters,
                facades,
                self.view,
                material,
                family,
                entrance=entry,
                skip_facades=replaced_frames,
                additions=frame_parts,
            )
            record = {
                "id": row["id"],
                "family": family,
                "origin": recipe["origin"],
                "reason": recipe["reason"],
                "facade_programme": programme,
                "facades": facades,
                "source_properties": row["properties"],
                "site_use": recipe["site_use"],
                "build_ms": (time.perf_counter() - began) * 1000,
                "scope": "Procedural opening/material estimate on complete exterior; full near room/door programme not admitted",
            }
            self.completed.append(record)
            return projected, info | (
                self.halls.current or {}
            ) | self.frames.logical_info(len(replaced_frames)) | {
                "facade_recipe": family,
                "facade_origin": recipe["origin"],
                "facade_programme": programme,
                "facade_plan": facades,
                "entrance_plan": self.entrances.planned[-1] if entry else None,
                "shopfront_plan": self.shopfronts.planned[-1] if shopfront else None,
                "balcony_plan": self.balconies.planned[-1] if balconies else None,
                "window_bay_plan": self.bays.planned[-1] if bays else None,
                "functional_status": "Exterior facade placeholder, source use unproven; actual rooms remain separate unresolved programme",
            }
        except ValueError as error:
            self.rejected.append({"id": row["id"], "reason": str(error)})
            return before

    def projections(self, row, entry, family):
        from provider_contract import seed_for

        variant = seed_for(row["id"]) % 3
        bays = (
            self.bays.plan(row, entry, family, (1.0, 0.65)[variant])
            if variant < 2
            else None
        )
        balconies = self.balconies.plan(row, entry, family) if bays is None else None
        programme = (
            ("rectangular-bay-stack" if variant == 0 else "chamfered-bay-stack")
            if bays
            else "balcony-stacks"
            if balconies
            else "ordinary-repeat"
        )
        return bays, balconies, programme

    def binding(self, material, fractions):
        key = (material, *fractions)
        if key not in self.palette:
            identifier = 32768 + len(self.palette)
            self.palette[key] = identifier
            self.bindings[str(identifier)] = {
                "wall_material": material,
                "fractions": list(fractions),
                "filtered": True,
            }
        return self.palette[key]

    def surface(self, mesh, row, parameters, rhythm, material):
        fractions = (rhythm["width"], rhythm["height"])
        projected, selected, facades = project(
            mesh, row, parameters, fractions, rhythm["axis_m"], rhythm["floor_m"]
        )
        identifier = self.binding(material, fractions)
        projected["materials"][selected] = identifier
        return projected, facades, identifier

    def report(self):
        return {
            "completed": self.completed,
            "rejected": self.rejected,
            "families": dict(Counter(r["family"] for r in self.completed)),
            "recesses": self.recesses,
            "recess_rejected": self.recess_rejected,
            "bands": self.bands.completed,
            "band_rejected": self.bands.rejected,
            "frames": self.frames.completed,
            "halls": self.halls.report(),
            "frame_rejected": self.frames.rejected,
            "entrances": self.entrances.planned,
            "entrance_geometry": self.entrance_geometry,
            "entrance_rejected": self.entrances.rejected,
            "shopfronts": self.shopfronts.planned,
            "shopfront_geometry": self.shopfront_geometry,
            "shopfront_rejected": self.shopfronts.rejected,
            "balconies": self.balconies.planned,
            "balcony_geometry": self.balcony_geometry,
            "balcony_rejected": self.balconies.rejected,
            "window_bays": self.bays.planned,
            "window_bay_geometry": self.bay_geometry,
            "window_bay_rejected": self.bays.rejected,
            "scope": "Context-scoped facades, exclusive balcony/window-bay programmes, bounded recesses and neighbor-free bands; zero completed architecture families; existing constructed room controls untouched",
        }

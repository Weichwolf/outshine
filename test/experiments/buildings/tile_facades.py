"""Context-scoped compact facade plans on existing complete source envelopes."""

import hashlib
import math
import time
from collections import Counter

import mapbox_vector_tile
import numpy as np
import shapely
from context import attach
from outshine_ref.buildings.facades import FacadeRepeat
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
    top = float(parameters.get("eave_m", parameters["buildingHeight"]))
    bottom = parameters["minHeight"]
    height = top - bottom
    rows = max(1, math.floor(height / floor_m + 0.5))
    uv = mesh["uv"][mesh["faces"]].copy()
    extent = np.zeros((len(triangles), 3, 3))
    assigned = np.zeros(len(triangles), dtype=bool)
    facades = []
    polygon = shapely.orient_polygons(row["polygon"])
    for ring in (polygon.exterior, *polygon.interiors):
        for a, b in zip(list(ring.coords)[:-1], list(ring.coords)[1:], strict=True):
            a, b = np.asarray(a), np.asarray(b)
            delta = b - a
            length = float(np.linalg.norm(delta))
            if length == 0:
                continue
            tangent = delta / length
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
            columns = max(1, math.floor(length / axis_m + 0.5))
            repeat = FacadeRepeat(length, height, columns, rows, *fractions)
            uv[selected, :, 0] = along[selected] / repeat.cell_size_m[0]
            uv[selected, :, 1] = (
                triangles[selected, :, 2] - bottom
            ) / repeat.cell_size_m[1]
            extent[selected] = [columns, rows, 0]
            assigned |= selected
            facades.append(
                {
                    "id": row["id"] + f"/facade-{len(facades)}",
                    "a": a.tolist(),
                    "b": b.tolist(),
                    "width_m": length,
                    "height_m": height,
                    "columns": columns,
                    "rows": rows,
                    "fractions": list(fractions),
                    "opening_count": repeat.opening_count,
                }
            )
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


class FacadeDetails:
    def __init__(self, rows, view, *, door_states=None):
        from facade_balconies import BalconyDetails
        from facade_entrances import EntranceDetails
        from facade_shopfronts import ShopfrontDetails
        from tile_facade_bands import BandDetails

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
        self.entrance_geometry = []
        self.shopfronts = ShopfrontDetails(self.entrances)
        self.shopfront_geometry = []
        self.balconies = BalconyDetails(self.entrances, door_states)
        self.balcony_geometry = []

    def __call__(self, row, parameters, material):
        before = self.base(row, parameters, material)
        if before is None:
            from tile_buildings import prism

            before = prism(row, parameters, material)
        mesh, info = before
        if "room_ids" in info:
            return before
        began = time.perf_counter()
        recipe = choose(
            row["properties"],
            row.get("context", {}),
            row["polygon"],
            parameters["buildingHeight"],
            parameters["minHeight"],
        )
        family = recipe["family"]
        rhythm = recipe["parameters"]
        if family in ("unknown", "utility", "garage"):
            return before
        try:
            fractions = (rhythm["width"], rhythm["height"])
            projected, selected, facades = project(
                mesh,
                row,
                parameters | info,
                fractions,
                rhythm["axis_m"],
                rhythm["floor_m"],
            )
            identifier = self.binding(material, fractions)
            projected["materials"][selected] = identifier
            entry = self.entrances.plan(row, parameters, facades, family)
            shopfront = self.shopfronts.plan(row, entry)
            balconies = self.balconies.plan(row, entry, family)
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
            )
            self.recesses.extend(
                r
                for r in details
                if r.get("kind") not in ("entrance", "shopfront", "balcony")
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
            projected = self.bands(
                projected,
                row,
                parameters,
                facades,
                self.view,
                material,
                family,
                entrance=entry,
            )
            record = {
                "id": row["id"],
                "family": family,
                "origin": recipe["origin"],
                "reason": recipe["reason"],
                "facades": facades,
                "source_properties": row["properties"],
                "site_use": recipe["site_use"],
                "build_ms": (time.perf_counter() - began) * 1000,
                "scope": "Procedural opening/material estimate on complete exterior; full near room/door programme not admitted",
            }
            self.completed.append(record)
            return projected, info | {
                "facade_recipe": family,
                "facade_origin": recipe["origin"],
                "facade_plan": facades,
                "entrance_plan": self.entrances.planned[-1] if entry else None,
                "shopfront_plan": self.shopfronts.planned[-1] if shopfront else None,
                "balcony_plan": self.balconies.planned[-1] if balconies else None,
                "functional_status": "Exterior facade placeholder, source use unproven; actual rooms remain separate unresolved programme",
            }
        except ValueError as error:
            self.rejected.append({"id": row["id"], "reason": str(error)})
            return before

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

    def report(self):
        return {
            "completed": self.completed,
            "rejected": self.rejected,
            "families": dict(Counter(r["family"] for r in self.completed)),
            "recesses": self.recesses,
            "recess_rejected": self.recess_rejected,
            "bands": self.bands.completed,
            "band_rejected": self.bands.rejected,
            "entrances": self.entrances.planned,
            "entrance_geometry": self.entrance_geometry,
            "entrance_rejected": self.entrances.rejected,
            "shopfronts": self.shopfronts.planned,
            "shopfront_geometry": self.shopfront_geometry,
            "shopfront_rejected": self.shopfronts.rejected,
            "balconies": self.balconies.planned,
            "balcony_geometry": self.balcony_geometry,
            "balcony_rejected": self.balconies.rejected,
            "scope": "Context-scoped facades, bounded demanded recesses and neighbor-free profile bands; zero completed architecture families; existing constructed room controls untouched",
        }

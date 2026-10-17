"""Source-stable scalar roof choices and bounded exterior envelopes for small ordinary bodies."""

import math
import time
from collections import Counter
from dataclasses import dataclass

import numpy as np
import shapely
from broken_hip import envelope as profiled_envelope
from broken_hip import recipe as roof_profile
from building_layout import frame
from outshine_ref.buildings.modelling import (
    Shape,
    SkeletonBudgetExceeded,
    cga_to_enu,
)
from outshine_ref.buildings.structures import classify_structure
from provider_contract import attributes, number
from shapely.affinity import translate
from tile_buildings import prism
from tile_room_details import RoomDetails, numeric_properties


@dataclass(frozen=True)
class RoofChoice:
    kind: str
    rise_m: float
    eave_m: float
    ridge_degrees: float
    origin: str
    reason: str
    edge_index: int = 0


def legacy_choice(row, parameters):
    polygon = row["polygon"]
    top = parameters["buildingHeight"]
    height = parameters["bodyHeight"]
    supplied = row["properties"].get("roof:shape", row["properties"].get("roof_shape"))
    if supplied is not None:
        requested = {
            "flat": "flat",
            "gabled": "ridge",
            "gable": "ridge",
            "ridge": "ridge",
            "skillion": "shed",
            "shed": "shed",
        }.get(str(supplied).lower())
        if requested != "flat":
            return RoofChoice(
                "flat",
                0.0,
                top,
                0.0,
                "supplied unsupported in this tile prototype",
                "supplied roof retained in source attributes; explicit sloped-roof/room attachment remains outside this small-body estimate",
            )
        return RoofChoice("flat", 0.0, top, 0.0, "supplied", "explicit flat roof")
    if (
        row["clipped"]
        or not 10.0 < polygon.area < 70.0
        or not 3.5 <= height <= 12.0
        or parameters["minHeight"] != 0.0
    ):
        return RoofChoice(
            "flat",
            0.0,
            top,
            0.0,
            "procedural default",
            "current small-body roof scope; larger complete room controls retain flat roofs",
        )
    if sum(len(r.coords) - 1 for r in (polygon.exterior, *polygon.interiors)) > 32:
        return RoofChoice(
            "flat",
            0.0,
            top,
            0.0,
            "procedural default",
            "small-body boundary exceeds the 32-corner experiment scope",
        )
    points = np.asarray(polygon.minimum_rotated_rectangle.exterior.coords)[:4]
    lengths = np.linalg.norm(np.roll(points, -1, axis=0) - points, axis=1)
    width = float(lengths.min())
    long = float(lengths.max())
    if width < 2.5:
        return RoofChoice(
            "flat",
            0.0,
            top,
            0.0,
            "procedural default",
            "insufficient contained width for a small-house roof estimate",
        )
    transform, _ = frame(polygon)
    axis = np.array(transform[:2])
    angle = math.degrees(math.atan2(axis[1], axis[0]))
    kind = "ridge" if long / width >= 1.4 else "shed"
    rise = min(1.6, width * 0.22, height - 2.64)
    ring = np.asarray(shapely.orient_polygons(polygon).exterior.coords)
    edges = np.diff(ring, axis=0)
    edge_index = int(
        np.argmax(abs((edges / np.linalg.norm(edges, axis=1, keepdims=True)) @ axis))
    )
    return RoofChoice(
        kind,
        rise,
        top - rise,
        angle,
        "procedural size/height estimate",
        "small ordinary source envelope; shape/use unproven, no specific architecture-family match",
        edge_index,
    )


def choose(row, parameters):
    from industrial_roof_choice import extend as industrial
    from residential_roof_choice import extend
    from urban_roof_choice import extend as urban

    return industrial(
        row,
        parameters,
        urban(row, parameters, extend(row, parameters, legacy_choice(row, parameters))),
    )


def normalise_hip(mesh, choice):
    heights = mesh["vertices"][:, 2]
    extent = float(heights.max())
    if abs(extent - choice.rise_m) > 1e-7:
        raise ValueError("Hip overlay exceeded the roof height precision contract")
    factor = choice.rise_m / extent
    mesh["vertices"][:, 2] *= factor
    if "normals" in mesh:
        mesh["normals"][:, 2] /= factor
        mesh["normals"] /= np.linalg.norm(mesh["normals"], axis=1, keepdims=True)


def roof_surface(row, choice):
    from industrial_roof_choice import KINDS, section_for

    if choice.kind in KINDS:
        from outshine_ref.buildings.section_roofs import section_surface

        return section_surface(row["polygon"], section_for(row, choice)), None, None
    polygon = row["polygon"]
    x0, y0, x1, y1 = polygon.bounds
    origin = np.array([(x0 + x1) / 2, (y0 + y1) / 2])
    source = Shape.from_polygon(
        translate(polygon, *-origin), 0.0, material=9
    ).rotate_scope(0.0, choice.ridge_degrees, 0.0)
    if choice.kind == "hip":
        shape = source.roof_hip(choice.rise_m, "byHeight")
    elif choice.kind == "ridge":
        shape = source.roof_ridge(choice.rise_m, "byHeight")
    else:
        shape = source.roof_shed(
            choice.rise_m, index=choice.edge_index, mode="byHeight"
        )
    mesh = shape.emit()
    mesh["vertices"] = cga_to_enu(mesh["vertices"])
    mesh["vertices"][:, :2] += origin
    if "normals" in mesh:
        mesh["normals"] = cga_to_enu(mesh["normals"])
    if choice.kind == "hip":
        normalise_hip(mesh, choice)
    mesh["vertices"][:, 2] += choice.eave_m
    known_top = number(
        row["properties"].get(
            "height",
            row["properties"].get("render_height", choice.eave_m + choice.rise_m),
        )
    )
    tolerance = 16 * np.finfo(float).eps * max(1.0, abs(known_top))
    for height in (choice.eave_m, known_top):
        values = mesh["vertices"][:, 2]
        values[abs(values - height) <= tolerance] = height
    selected = np.asarray(
        ["roof.top" in face.tags for face in shape.geometry.polygons]
    )[mesh["parts"]]
    top = {
        key: value[selected]
        if key in ("faces", "materials", "groups", "parts", "face_tag_mask")
        else value
        for key, value in mesh.items()
    }
    return top, mesh, shape


def projected_error(row, parameters, choice, view):
    x0, y0, x1, y1 = row["polygon"].bounds
    points = np.array(
        [
            (x, y, z)
            for x in (x0, x1)
            for y in (y0, y1)
            for z in (parameters["minHeight"], parameters["buildingHeight"])
        ]
    )
    depth = float(np.min((points - view.eye) @ view.basis[2]))
    factor = math.sqrt(
        1
        + (view.width / (2 * view.focal_px)) ** 2
        + (view.height / (2 * view.focal_px)) ** 2
    )
    return (
        choice.rise_m * (view.focal_px * factor + 0.5) / depth
        if depth > 0
        else float("inf")
    )


def pitched_candidates(rows, view, limit=16):
    ranked = []
    industry = shapely.union_all(
        [
            r["polygon"]
            for r in rows
            if r["layer"] == "landuse" and r["properties"].get("class") == "industrial"
        ]
    )
    for row in rows:
        if row["layer"] != "building" or row["clipped"]:
            continue
        try:
            p = attributes(
                {
                    k: v
                    for k, v in row["properties"].items()
                    if k not in ("colour", "color", "building:colour")
                }
            )
            choice = choose(row, p)
            if p["hide3d"] or choice.kind not in ("ridge", "shed"):
                continue
            coverage = min(
                1.0, row["polygon"].intersection(industry).area / row["polygon"].area
            )
            if (
                classify_structure(
                    row["polygon"],
                    row["properties"],
                    coordinate_step_m=row["coordinate_step_m"],
                    industrial_coverage=coverage,
                    clipped=row["clipped"],
                ).kind
                != "ordinary"
            ):
                continue
            x0, y0, x1, y1 = row["polygon"].bounds
            points = np.array(
                [
                    (x, y, z)
                    for x in (x0, x1)
                    for y in (y0, y1)
                    for z in (p["minHeight"], p["buildingHeight"])
                ]
            )
            projected = (points - view.eye) @ view.basis.T
            if (
                projected[:, 2].min() <= 0
                or 1.2 * view.focal_px / projected[:, 2].max() < 3.0
            ):
                continue
            uv = projected[:, :2] * view.focal_px / projected[:, 2:3] + [
                view.width / 2,
                view.height / 2,
            ]
            low = np.maximum(uv.min(axis=0), 0)
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


class OrdinaryDetails:
    def __init__(self, rows, view):
        from industrial_roof_choice import KINDS

        self.rooms = RoomDetails(rows, view)
        self.view = view
        self.choices = Counter()
        self.completed = []
        self.rejected = []
        self.coarse = []
        self.pitched = pitched_candidates(rows, view)
        self.industrial_rooms = {
            row["id"]
            for row in rows
            if row["layer"] == "building"
            and row["id"] in self.rooms.identifiers
            and choose(row, attributes(numeric_properties(row))).kind in KINDS
        }
        self.pitched.update(self.industrial_rooms)
        self.room_completed = []
        self.room_rejected = []
        self.current_envelope = None

    def __call__(self, row, parameters, material):
        self.current_envelope = None
        choice = choose(row, parameters)
        self.choices[choice.kind] += 1
        if choice.kind == "flat":
            return self.rooms(row, parameters, material)
        try:
            return self.complete(row, parameters, material, choice)
        except (
            ValueError,
            NotImplementedError,
            SkeletonBudgetExceeded,
            shapely.errors.GEOSException,
        ) as error:
            self.rejected.append(
                {"id": row["id"], "kind": choice.kind, "reason": str(error)}
            )
            if row["id"] in self.industrial_rooms:
                return self.rooms(row, parameters, material)
            return None

    def complete(self, row, parameters, material, choice):
        from industrial_roof_choice import KINDS
        from industrial_roof_choice import metadata as section_metadata

        error = projected_error(row, parameters, choice, self.view)
        profile = roof_profile(row, parameters, choice)
        metadata = {
            "roof_kind": choice.kind,
            "roof_rise_m": choice.rise_m,
            "eave_m": choice.eave_m,
            "ridge_degrees": choice.ridge_degrees,
            "selected_eave_edge": choice.edge_index,
            "roof_origin": choice.origin,
            "roof_reason": choice.reason,
            "projected_geometry_error_px": error if math.isfinite(error) else None,
            "functional_status": "Exterior heightfield envelope; room/use/access programme and native integration unplanned",
        }
        if profile is not None:
            metadata["roof_profile"] = profile | {"status": "deferred"}
        if choice.kind in KINDS:
            metadata["roof_section"] = section_metadata(row, choice)
        if error <= 0.5:
            mesh, info = prism(row, parameters, material)
            self.coarse.append(dict(id=row["id"], **metadata))
            return mesh, info | metadata | {
                "roof_product": "direct subpixel heightfield approximation; logical roof choice retained"
            }
        start = time.perf_counter()
        top, _, _ = roof_surface(row, choice)
        if row["id"] in self.pitched:
            from pitched_room_details import complete

            try:
                mesh, info, _, _ = complete(row, parameters, material, choice, top)
                self.room_completed.append(dict(id=row["id"], **info))
                self.completed.append(
                    dict(
                        id=row["id"],
                        triangles=len(mesh["faces"]),
                        build_ms=(time.perf_counter() - start) * 1000,
                        **metadata,
                    )
                )
                return mesh, metadata | info
            except ValueError as error:
                self.room_rejected.append({"id": row["id"], "reason": str(error)})
                if row["id"] in self.industrial_rooms:
                    return self.rooms(row, parameters, material)
        envelope, profile_report = profiled_envelope(
            top, parameters, material, choice, profile
        )
        if profile_report is not None:
            metadata["roof_profile"] = profile_report
        mesh = dict(envelope.mesh)
        for key in ("parts", "groups", "face_tag_mask"):
            mesh.pop(key)
        region = shapely.from_wkb(envelope.footprint_wkb)
        if region.symmetric_difference(row["polygon"]).area > 1e-7:
            raise ValueError(
                "Roof envelope changed its source footprint or filled a courtyard"
            )
        self.current_envelope = envelope
        self.completed.append(
            dict(
                id=row["id"],
                triangles=len(mesh["faces"]),
                build_ms=(time.perf_counter() - start) * 1000,
                **metadata,
            )
        )
        return mesh, metadata | {
            "form": "Closed roof heightfield envelope",
            "triangles": len(mesh["faces"]),
        }

    def report(self):
        return {
            "choices": dict(self.choices),
            "completed": self.completed,
            "coarse": self.coarse,
            "rejected": self.rejected,
            "room_selected": len(self.pitched),
            "room_completed": self.room_completed,
            "room_rejected": self.room_rejected,
            "scope": "Small ordinary, bounded residential and urban hip envelopes plus at most sixteen visible constructive small-roof/room controls. Urban roof forms are inferred, not measured. No architecture-family/native LOD admission",
        }

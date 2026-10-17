"""Source-stable exterior mass/storey plans with simple reserved vertical access."""

import math
from dataclasses import dataclass

import numpy as np
import shapely

from building_layout import AGENT_RADIUS_M, WALL_M, rectangle
from facade_axes import layout
from facade_entrances import cell_for, positions_for
from outshine_ref.buildings.interiors.entrances import entrance
from provider_contract import attributes, number, seed_for
from recipes import choose
from terraced_mass_core import Core, reserve
from terraced_mass_profiles import proposals


@dataclass(frozen=True)
class Entry:
    identifier: str
    facade_id: str
    column: int
    centre: tuple
    tangent: tuple
    opening_wkb: bytes
    landing_wkb: bytes
    origin: str


@dataclass(frozen=True)
class Storey:
    identifier: str
    bottom_m: float
    top_m: float
    footprint_wkb: bytes
    terrace_wkb: bytes


@dataclass(frozen=True)
class Mass:
    identifier: str
    kind: str
    source_wkb: bytes
    bottom_m: float
    top_m: float
    storeys: tuple
    core: Core
    entry: Entry
    choice: tuple
    source_levels: bool
    candidate_count: int


def floors(row, parameters):
    recipe = choose(
        row["properties"],
        row.get("context", {}),
        row["polygon"],
        parameters["buildingHeight"],
        parameters["minHeight"],
    )
    height = parameters["bodyHeight"]
    supplied = next(
        (
            row["properties"][k]
            for k in ("building:levels", "levels")
            if k in row["properties"]
        ),
        None,
    )
    count = (
        number(supplied)
        if supplied is not None
        else max(1, math.floor(height / recipe["parameters"]["floor_m"] + 0.5))
    )
    if not float(count).is_integer() or not 3 <= count <= 32 or height / count < 2.64:
        raise ValueError("Terraced mass requires3..32 safe supplied/derived storeys")
    return int(count), supplied is not None, recipe


def entrance_for(row, parameters, recipe, rise):
    polygon = row["polygon"]
    interior = polygon.buffer(-WALL_M, join_style="mitre").intersection(polygon)
    if interior.geom_type != "Polygon" or interior.is_empty:
        raise ValueError("No connected provisional open ground floor")
    facades = layout(
        row,
        parameters,
        (recipe["parameters"]["width"], recipe["parameters"]["height"]),
        recipe["parameters"]["axis_m"],
        rise,
    )

    def accepted(door):
        try:
            cell_for(door, facades, polygon)
            pocket = rectangle(door["centre"], door["tangent"], 1.40, -0.25, 1.84)
            return interior.covers(pocket)
        except ValueError:
            return False

    door = entrance(
        polygon,
        [interior],
        row.get("context", {}),
        fractions_by_edge=positions_for(polygon, facades),
        accept=accepted,
    )
    if door is None:
        raise ValueError("No source-cell entrance for the simple ground floor")
    facade, column, _width = cell_for(door, facades, polygon)
    entry = Entry(
        row["id"] + "/entrance",
        facade["id"],
        column,
        tuple(door["centre"]),
        tuple(door["tangent"]),
        door["polygon"].wkb,
        door["landing"].wkb,
        door["origin"],
    )
    return door, entry


def valid_profiles(values):
    previous = values[0]
    for index, current in enumerate(values):
        if (
            current.geom_type != "Polygon"
            or current.is_empty
            or not current.is_valid
            or current.area < 24
        ):
            raise ValueError(
                "Mass profile is empty, disconnected, invalid or undersized"
            )
        if current.difference(previous).area > 1e-8:
            raise ValueError("Upper mass leaves its lower supporting footprint")
        clear = current.buffer(-(WALL_M + AGENT_RADIUS_M), join_style="mitre")
        if clear.geom_type != "Polygon" or clear.is_empty:
            raise ValueError("Mass profile has disconnected actor-clear interior")
        if index and previous.difference(current).area < 3.0:
            raise ValueError("Mass step has no useful three-square-metre terrace")
        previous = current


def plan(row, kind):
    polygon = row["polygon"]
    if not isinstance(row.get("id"), str) or not row["id"]:
        raise ValueError("Mass requires a stable source identifier")
    if (
        not isinstance(polygon, shapely.Polygon)
        or polygon.has_z
        or not polygon.is_valid
        or polygon.is_empty
        or not np.isfinite(shapely.get_coordinates(polygon)).all()
    ):
        raise ValueError("Mass requires a finite valid XY source polygon")
    if row.get("clipped") or len(shapely.get_coordinates(polygon)) > 128:
        raise ValueError("Mass source is clipped or exceeds128 contour vertices")
    parameters = attributes(row["properties"])
    if parameters["hide3d"] or parameters["minHeight"] != 0:
        raise ValueError("Mass source must be a visible ground-supported building")
    count, supplied, recipe = floors(row, parameters)
    rise = parameters["bodyHeight"] / count
    door, entry = entrance_for(row, parameters, recipe, rise)
    rejected = []
    for number_, (profiles, choice) in enumerate(
        proposals(polygon, kind, seed_for(row["id"])), 1
    ):
        try:
            valid_profiles(profiles)
            core, _cost = reserve(
                row["id"], polygon, profiles, door, count, rise, parameters["minHeight"]
            )
            first = max(1, count // 3)
            last = count - max(1, count // 3)
            values = tuple(
                profiles[0 if i < first else 1 if i < last else 2] for i in range(count)
            )
            storeys = tuple(
                Storey(
                    row["id"] + f"/storey/{i}",
                    i * rise,
                    parameters["buildingHeight"] if i == count - 1 else (i + 1) * rise,
                    current.wkb,
                    (values[i - 1].difference(current) if i else shapely.Polygon()).wkb,
                )
                for i, current in enumerate(values)
            )
            return Mass(
                row["id"],
                kind,
                polygon.wkb,
                parameters["minHeight"],
                parameters["buildingHeight"],
                storeys,
                core,
                entry,
                choice,
                supplied,
                number_,
            )
        except ValueError as error:
            rejected.append(str(error))
    raise ValueError(
        "No source-contained mass/core programme: "
        + (rejected[-1] if rejected else "no geometric proposal")
    )

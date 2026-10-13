"""Sequence prepared spatial plans without IO, rendering or generator-specific policy."""

from dataclasses import dataclass
from typing import Callable

from . import buildings, transport, vegetation
from .contracts import BuildingFootprint, FootprintAdjustment, SourceInputs


def adjusted_footprints(
    originals: tuple[BuildingFootprint, ...],
    adjustments: tuple[FootprintAdjustment, ...],
) -> tuple[BuildingFootprint, ...]:
    by_key = {p.key: p for p in originals}
    changes = {}
    for change in adjustments:
        key = change.original.key
        if key in changes or key not in by_key:
            raise ValueError("footprint changes must name distinct existing source parts")
        source = by_key[key]
        if (source.heights != change.original.heights
                or not source.geometry.equals_exact(change.original.geometry, tolerance=0)):
            raise ValueError("footprint change refers to a different source state")
        changes[key] = change.adjusted
    output = tuple(part for source in originals for part in changes.get(source.key, (source,)))
    if len({part.key for part in output}) != len(output):
        raise ValueError("footprint changes collide with retained part identities")
    return output


@dataclass(frozen=True, slots=True)
class PreparedWorld:
    sources: SourceInputs
    footprints: tuple[BuildingFootprint, ...]
    transport: transport.Plan
    buildings: buildings.Plan
    vegetation: vegetation.Plan


def prepare(
    sources: SourceInputs,
    transport_planner: Callable[[SourceInputs], transport.Plan],
    building_planner: Callable[[buildings.Inputs], buildings.Plan],
    vegetation_planner: Callable[[vegetation.Inputs], vegetation.Plan],
) -> PreparedWorld:
    roads = transport_planner(sources)
    if not isinstance(roads, transport.Plan) or roads.frame != sources.frame:
        raise ValueError("transport must return a plan in the source frame")
    footprints = adjusted_footprints(sources.buildings, roads.adjustments)
    houses = building_planner(buildings.Inputs(sources, footprints, roads.reservations, roads.hints))
    if not isinstance(houses, buildings.Plan) or houses.frame != sources.frame:
        raise ValueError("buildings must return a plan in the source frame")
    plants = vegetation_planner(vegetation.Inputs(
        sources, roads.reservations + houses.reservations, roads.hints + houses.hints,
    ))
    if not isinstance(plants, vegetation.Plan) or plants.frame != sources.frame:
        raise ValueError("vegetation must return a plan in the source frame")
    return PreparedWorld(sources, footprints, roads, houses, plants)


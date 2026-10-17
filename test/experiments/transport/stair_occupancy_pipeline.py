"""Exercise existing plan sequencing with actual footprints and clearance probes."""

import numpy as np
import shapely
from outshine_ref import buildings, transport, vegetation
from outshine_ref.contracts import (
    BuildingFootprint,
    HeightInterval,
    Reservation,
    SourceInputs,
)
from outshine_ref.pipeline import prepare
from stair_occupancy_handoff import OccupancyQuery


def probes(assets, sample):
    centers = np.concatenate(
        [
            a.data["body_vertices"][a.data["top_faces"][~a.data["ground"]]].mean(axis=1)
            for a in assets
        ]
    )
    chosen = np.unique(
        np.linspace(0, len(centers) - 1, min(128, len(centers)), dtype=int)
    )
    result, omitted = [], []
    for i in chosen:
        xy = centers[i, :2]
        try:
            base = float(sample(xy[None])[0])
        except ValueError as error:
            omitted.append({"probe": int(i), "reason": str(error)})
            continue
        result.append(
            BuildingFootprint(
                f"diagnostic-low-cover-probe:{i}",
                shapely.box(*(xy - 0.1), *(xy + 0.1)),
                HeightInterval(base, base + 1.2),
            )
        )
    return tuple(result), omitted


def handoff(
    frame, originals, reservations, changes, vegetation_probes, *, transport_sources=()
):
    inputs = SourceInputs(frame, transport=transport_sources, buildings=originals)
    seen = {}

    def building_plan(value):
        seen["footprints"] = value.footprints
        seen["building_reservations"] = value.reservations
        # Occupancy only: these are diagnostic consumer callbacks, not building recipes.
        return buildings.Plan(
            frame,
            tuple(
                Reservation(p.source_id, p.geometry, p.heights, "building")
                for p in value.footprints
            ),
        )

    def vegetation_plan(value):
        seen["vegetation_reservations"] = value.reservations
        query = OccupancyQuery(value.reservations)
        seen["probe_blocked"] = tuple(
            query.conflict(p)[0].area > 0 for p in vegetation_probes
        )
        return vegetation.Plan(frame)

    world = prepare(
        inputs,
        lambda _: transport.Plan(frame, changes, reservations),
        building_plan,
        vegetation_plan,
    )
    if world.sources is not inputs or seen["footprints"] is not world.footprints:
        raise ValueError(
            "shared pipeline did not retain original inputs/adjusted footprints"
        )
    if seen["building_reservations"] is not world.transport.reservations:
        raise ValueError("building handoff lost transport reservations")
    if any(
        p.source_id not in {o.source_id for o in originals} for p in world.footprints
    ):
        raise ValueError("shared pipeline introduced a synthetic source")
    return world, seen

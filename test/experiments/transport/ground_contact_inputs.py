"""Existing source-qualified three-arm product and its original bound DEM."""

import json
import time
from dataclasses import dataclass

import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from outshine_ref.transport.canonical_ports._geodesy import (
    ground_geographic_at,
    positions_in_frame,
)
from outshine_ref.transport.canonical_ports._ground_recipe import NETWORK_RECIPE
from outshine_ref.transport.canonical_ports._ground_terminals import (
    derive_scoped_ground_terminals,
)
from replay_branched_ground import INPUT

SEED = (5699117369358000712, 1)


@dataclass
class ContactContext:
    road: object
    anchor: tuple
    terrain: object
    road_dem_z: np.ndarray
    report: dict
    metadata: dict
    preparation_cpu_s: float

    def sample_z(self, xy):
        geographic = ground_geographic_at(self.anchor, xy)
        heights = np.asarray(self.terrain(geographic), dtype=float)
        if heights.shape != (len(xy),) or not np.isfinite(heights).all():
            raise ValueError("missing_or_invalid_bound_dem")
        return positions_in_frame(self.anchor, geographic, heights)[:, 2]


def load_context(*, seed=SEED, geometry_recipe=NETWORK_RECIPE):
    started = time.process_time()
    source, groups, ways, binding, reader = load_bridge_source_facts(INPUT)
    manifest = json.loads((INPUT / "manifest.json").read_text())
    terrain, terrain_binding = terrain_input(
        FIXTURE, {"terrain": manifest["terrain_reference"]}
    )
    prepared, reason, report = derive_scoped_ground_terminals(
        source,
        seed,
        groups,
        ways,
        terrain,
        source_binding=source.binding,
        candidate_binding=binding,
        terrain_binding=terrain_binding,
        maximum_prior_change_m=0.5,
        geometry_recipe=geometry_recipe,
    )
    if reason:
        raise ValueError("original_qualified_road_unavailable:" + reason)
    anchor, _field = prepared
    context = ContactContext(
        report["surface"],
        anchor,
        terrain,
        np.empty(0),
        report,
        {
            "source_binding": source.binding,
            "candidate_binding": binding,
            "terrain_binding": terrain_binding,
            "reader": reader,
            "seed": seed,
            "dem_height_convention": "Supplied geodetic DEM converted to scalar ENU up at the existing ellipsoid-ray query; local height-function convention, not a new original-grid/vertical-ray positional contract.",
        },
        0,
    )
    context.road_dem_z = context.sample_z(context.road.vertices[:, :2])
    context.preparation_cpu_s = time.process_time() - started
    return context

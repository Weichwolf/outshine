"""Explicitly analytical transport inputs with analytic or pinned real DEM heights."""

import math

import numpy as np
from canonical_port_native_inputs import interior_demands, load
from outshine_ref.transport.canonical_ports import (
    PortDemand,
    PortNode,
    PortSource,
    PortSpan,
)


def long_bridge():
    radius = 6378137.0
    run = [-21000, *np.linspace(-20000, 30000, 41), 31000]
    nodes = tuple(
        PortNode(i + 1, (math.degrees(float(x) / radius), 0.0))
        for i, x in enumerate(run)
    )
    edges = (
        PortSpan((100, 0), (1, 2), 8, 0.05, False, False, {"origin": "analytical"}),
        *(
            PortSpan(
                (200 + i, 0),
                (i + 2, i + 3),
                8,
                0.05,
                True,
                False,
                {"origin": "analytical"},
                layer=1,
            )
            for i in range(40)
        ),
        PortSpan((300, 0), (43, 42), 8, 0.05, False, False, {"origin": "analytical"}),
    )
    source = PortSource(
        nodes, edges, (), {"dataset": "analytical-40-span-bridge", "revision": "1"}
    )
    cuts = interior_demands(source, edges[1:-1], 18)
    cuts = tuple(d for d in cuts if abs(d.anchor_longitude_latitude[0]) < 0.002)
    cuts += (PortDemand((216, 0), 0, (0.0, 0.0), (18, 0, 131072, 131072)),)

    def terrain(points):
        station = np.deg2rad(points[:, 0]) * radius
        return 2000 * np.clip((station + 20000) / 50000, 0, 1) ** 4

    binding = {
        "sources": [{"dataset": "analytical-abutments", "revision": "1"}],
        "source_datum": "analytic model, not observed earth elevations",
        "height_mapping": "analytic model geodetic metres/1",
    }
    return source, cuts, terrain, binding


def bound_dem_bridge():
    _native, terrain, binding, _manifest = load()
    latitudes = (48.240, 48.241, 48.243, 48.245, 48.249, 48.250)
    nodes = tuple(
        PortNode(i + 1, (16.38, latitude)) for i, latitude in enumerate(latitudes)
    )
    edges = tuple(
        PortSpan(
            (100 + i, 0),
            (i + 1, i + 2),
            3.5,
            0.12,
            i in (1, 2, 3),
            False,
            {"origin": "analytically normalized meridian, not recovered MVT topology"},
            layer=1 if i in (1, 2, 3) else 0,
        )
        for i in range(5)
    )
    source = PortSource(
        nodes,
        edges,
        (),
        {
            "dataset": "analytical-meridian-with-bound-earth-DEM",
            "revision": "1",
            "scope": "Analytical transport topology and actual pinned Mapterhorn DEM; no actual native bridge source qualification",
        },
    )
    return source, interior_demands(source, edges[1:4], 18), terrain, binding


CASES = {"long": long_bridge, "bound-dem": bound_dem_bridge}

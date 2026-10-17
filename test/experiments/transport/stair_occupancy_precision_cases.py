"""Repeatable difficult domains and actual saved-facet/query bands; no geometry solving."""

import json
import math
import random
from pathlib import Path

import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from stair_occupancy_geometry import AffineOccupancy, plane_value
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_inputs import actual_sources, buildings, full_tile_sampler
from stair_occupancy_verify import read


def controls():
    rows = []
    for x in (0, 1000, 500000):
        domain = shapely.box(x, 0, x + 1, 1)
        row = AffineOccupancy(
            f"translated:{x}", domain, "control", (x, 0), (100, 1, 0), (101, 1, 0)
        )
        for name, upper in (
            ("positive", math.nextafter(100.0, 101.0)),
            ("touch", 100.0),
            ("below", math.nextafter(100.0, 99.0)),
        ):
            rows.append(
                (
                    row.owner_id + ":" + name,
                    "control",
                    row,
                    HeightInterval(0, upper),
                    None,
                    (),
                )
            )
    u = shapely.Polygon(
        [(0, 0), (4, 0), (4, 4), (3, 4), (3, 1), (1, 1), (1, 4), (0, 4)]
    )
    hole = shapely.Polygon(
        shapely.box(0, 0, 6, 6).exterior.coords,
        [shapely.box(1, 1, 2, 2).exterior.coords],
    )
    for name, domain, lower, upper, heights, origin in (
        ("u_split", u, (100, 0, -1), (103, 0, -1), HeightInterval(90, 98), (0, 0)),
        ("hole", hole, (100, 1, 0), (103, 1, 0), HeightInterval(90, 104), (0, 0)),
        (
            "diagonal_sliver",
            shapely.Polygon([(1001, 1000), (1000, 1001), (1001, 1001)]),
            (100, 1, 1),
            (103, 1, 1),
            HeightInterval(90, math.nextafter(101.0, 102.0)),
            (1000, 1000),
        ),
    ):
        row = AffineOccupancy(name, domain, "control", origin, lower, upper)
        rows.append((name, "control", row, heights, None, ()))
    rng = random.Random(568)
    for i in range(64):
        cells = [
            shapely.box(x, y, x + 1, y + 1)
            for x in range(6)
            for y in range(6)
            if rng.random() > 0.35
        ]
        domain = shapely.union_all(cells)
        gx, gy = rng.randint(-8, 8) / 4, rng.randint(-8, 8) / 4
        row = AffineOccupancy(
            f"grid:{i}", domain, "control", (0, 0), (100, gx, gy), (103, gx, gy)
        )
        rows.append(
            (
                row.owner_id,
                "control",
                row,
                HeightInterval(90, rng.randint(96, 108)),
                None,
                tuple(cells),
            )
        )
    return rows


def actual_cases(products, *, facet_limit=8192):
    report = json.loads((products / "comparison.json").read_text())
    rows, _data = read(products / "volumes.json", report)
    context, _sources, _old_sample, _layer = actual_sources()
    original, _ledger, _receipt = buildings(
        context, full_tile_sampler(Path(report["dem"]["path"]), context)
    )
    probes = tuple(
        BuildingFootprint(
            p["id"],
            shapely.from_wkb(p["geometry_wkb"]),
            HeightInterval(*p["heights_m"]),
        )
        for p in json.loads((products / "probes.json").read_text())
    )
    query, cases = OccupancyQuery(rows), []
    for footprint in original + probes:
        for i in sorted(query.tree.query(footprint.geometry, predicate="intersects")):
            row = rows[i]
            name = footprint.source_id + "|" + row.owner_id
            cases.append(
                (name, "actual_height_query", row, footprint.heights, footprint, ())
            )
    for row in rows[:facet_limit]:
        lower = [
            plane_value(row.lower_plane_m, row.origin_xy_m, xy)
            for xy in shapely.get_coordinates(row.geometry)
        ]
        upper = float((min(lower) + max(lower)) / 2)
        heights = HeightInterval(row.heights.lower_m - 1, upper)
        cases.append(
            (row.owner_id, "actual_facet_controlled_slice", row, heights, None, ())
        )
    return cases, len(rows), len(original) + len(probes)

"""Select reproducible infrastructure cases from the pinned provider payload."""

import argparse
import csv
import json
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from functools import cached_property
from pathlib import Path

import numpy as np
import shapely
from outshine.places.sources import (
    acquire,
    covering_tiles,
    decode_vector,
    tile_geography,
)
from outshine.terrain.ground_surface import ROAD_WIDTH
from shapely.geometry import shape

HERE = Path(__file__).resolve().parent
SNAPSHOT = "20260927_080001_pt"
RADIUS_M = 780
CASE_RADIUS_M = 148
LAYERS = ("transportation", "aeroway", "waterway", "water")
CONTINENTS = {"AF", "AS", "EU", "NA", "SA", "OC"}
CHECKS = {
    "road": (
        "road continuity",
        "direction and access",
        "grade",
        "shoulders and surfacing",
    ),
    "path": ("walkable continuity", "grade", "ground contact and surfacing"),
    "steps": (
        "flight and landing continuity",
        "usable treads",
        "supports and surfacing",
    ),
    "junction": ("required approaches", "grade and levels", "pavement transitions"),
    "crossing": (
        "road and rail continuity",
        "shared grade",
        "gauge and flangeways",
        "surfacing",
    ),
    "rail": ("track continuity", "grade and gauge", "ballast and terrain contacts"),
    "turnout": (
        "track branch continuity",
        "grade and gauge",
        "switch and ballast geometry",
    ),
    "station": (
        "platform access",
        "public approach continuity",
        "clearance and materials",
    ),
    "cableway": ("alignment continuity", "supports and clearance", "station access"),
    "bridge": (
        "connected approaches",
        "deck grade and clearance",
        "abutments and materials",
    ),
    "tunnel": ("connected approaches", "bore grade and clearance", "portal materials"),
    "port": ("landside and water contacts", "quay continuity", "surface transitions"),
    "waterway": (
        "channel continuity",
        "shore contacts",
        "crossing and terrain separation",
    ),
    "runway": (
        "surface and profile continuity",
        "markings",
        "terrain and shoulder contacts",
    ),
    "taxiway": (
        "airfield route continuity",
        "usable widths",
        "markings and surface contacts",
    ),
}


@dataclass(frozen=True)
class Window:
    continent: str
    split: str
    area: str
    latitude: float
    longitude: float
    preferred: tuple[str, str]

    @cached_property
    def origin(self):
        return np.array([self.longitude, self.latitude])

    @cached_property
    def metres(self):
        return np.array([111319.49 * np.cos(np.deg2rad(self.latitude)), 111132.0])

    def geographic(self, point):
        longitude, latitude = self.origin + point / self.metres
        return [round(latitude, 8), round(longitude, 8)]


@dataclass(frozen=True)
class Feature:
    family: str
    geometry: shapely.Geometry
    tile: tuple[int, int, int]
    layer: str
    properties: dict
    feature_id: int | None

    def evidence(self):
        return [
            "/".join(map(str, self.tile)),
            self.layer,
            self.feature_id,
            self.properties,
        ]


@dataclass(frozen=True)
class Candidate:
    family: str
    xy: np.ndarray
    features: tuple[Feature, ...]
    links: tuple[tuple[Feature, np.ndarray, np.ndarray], ...]


def load_survey(path=HERE / "survey.csv"):
    with path.open() as stream:
        windows = [
            Window(
                row["continent"],
                row["split"],
                row["area"],
                float(row["latitude"]),
                float(row["longitude"]),
                tuple(row["preferred"].split("|")),
            )
            for row in csv.DictReader(stream)
        ]
    counts = Counter((w.continent, w.split) for w in windows)
    expected = {
        (c, s): n for c in CONTINENTS for s, n in (("development", 20), ("holdout", 5))
    }
    if counts != expected or len({w.area for w in windows}) != len(windows):
        raise ValueError(
            "each continent needs 20 development and 5 separate holdout windows"
        )
    return windows


def family(layer, props):
    kind = props.get("class")
    detail = props.get("subclass")
    if layer == "aeroway":
        if kind in {"runway", "helipad"}:
            return "runway"
        if kind in {"taxiway", "apron", "gate"}:
            return "taxiway"
    if layer == "waterway":
        return "waterway"
    if layer == "water" and kind == "dock":
        return "port"
    if layer != "transportation":
        return None
    if props.get("brunnel") in {"bridge", "tunnel"}:
        return props["brunnel"]
    if kind == "bridge":
        return "bridge"
    if kind in {"pier", "ferry"}:
        return "port"
    if detail == "steps":
        return "steps"
    if detail in {"station", "platform", "platform_access"}:
        return "station"
    if kind == "aerialway":
        return "cableway"
    if kind in {"rail", "transit"}:
        return "rail"
    if kind == "path":
        return "path"
    if kind in ROAD_WIDTH:
        return "road"
    return None


def extract(window, tile, decoded):
    for layer, data in decoded.items():

        def local(points, extent=data["extent"]):
            lat, lon = tile_geography(points, extent, tile)
            return (np.c_[lon, lat] - window.origin) * window.metres

        classified = [
            (kind, record)
            for record in data["features"]
            if (kind := family(layer, record["properties"])) is not None
        ]
        parts, parents = shapely.get_parts(
            [shape(record["geometry"]) for _, record in classified], return_index=True
        )
        valid = np.isin(shapely.get_type_id(parts), [1, 3])
        parts = shapely.transform(parts[valid], local)
        local_parts = shapely.dwithin(parts, shapely.Point(0, 0), RADIUS_M)
        for part, index in zip(
            parts[local_parts], parents[valid][local_parts], strict=True
        ):
            kind, record = classified[index]
            yield Feature(
                kind, part, tile, layer, record["properties"], record.get("id")
            )


def line_anchor(line, point):
    inset = min(30.0, line.length / 4)
    station = np.clip(line.project(point), inset, line.length - inset)
    xy = shapely.get_coordinates(
        shapely.line_interpolate_point(line, station + np.array([-inset, 0, inset]))
    )
    return xy[1], xy[0], xy[-1]


def geometric_candidates(features):
    centre = shapely.Point(0, 0)
    for feature in features:
        geometry = feature.geometry
        if geometry.geom_type == "LineString" and geometry.length >= 24:
            xy, start, end = line_anchor(geometry, centre)
            yield Candidate(feature.family, xy, (feature,), ((feature, start, end),))
        elif geometry.geom_type == "Polygon" and geometry.area >= 100:
            xy = np.array(shapely.get_coordinates(geometry.representative_point())[0])
            yield Candidate(feature.family, xy, (feature,), ())


def shared_nodes(features):
    nodes = defaultdict(list)
    for feature in features:
        geometry = feature.geometry
        if (
            feature.family not in {"road", "rail"}
            or geometry.geom_type != "LineString"
            or feature.properties.get("brunnel")
            or feature.properties.get("layer", 0) != 0
        ):
            continue
        for point in np.asarray(geometry.coords):
            if point @ point < RADIUS_M**2:
                nodes[tuple(np.round(point, 4))].append(feature)
    for key, entries in nodes.items():
        roads = [f for f in entries if f.family == "road"]
        rail = [f for f in entries if f.family == "rail"]
        if not roads and not rail:
            continue
        point = shapely.Point(key)
        links = {}
        for feature in entries:
            line = feature.geometry
            station = line.project(point)
            for sign, available in ((-1, station), (1, line.length - station)):
                if available < 8:
                    continue
                endpoint = np.asarray(
                    line.interpolate(station + sign * min(30, available)).coords[0]
                )
                dx, dy = endpoint - key
                directions = 72 if feature.family == "rail" else 24
                angle = (
                    round(np.arctan2(dy, dx) * directions / (2 * np.pi)) % directions
                )
                anchor = np.asarray(key)
                start, end = (anchor, endpoint) if sign > 0 else (endpoint, anchor)
                links.setdefault((feature.family, angle), (feature, start, end))
        road_links = [link for (kind, _), link in links.items() if kind == "road"]
        rail_links = [link for (kind, _), link in links.items() if kind == "rail"]
        if rail and len(road_links) >= 2 and len(rail_links) >= 2:
            kind, legs = "crossing", road_links[:2] + rail_links[:2]
        elif len(road_links) >= 3:
            kind, legs = "junction", road_links
        elif len(rail_links) >= 3:
            kind, legs = "turnout", rail_links
        else:
            continue
        yield Candidate(kind, np.asarray(key), tuple(v[0] for v in legs), tuple(legs))


def cameras(window, candidate):
    if candidate.links:
        direction = candidate.links[0][2] - candidate.links[0][1]
        direction /= np.linalg.norm(direction)
    else:
        direction = np.array([0.0, 1.0])
    lateral = direction[::-1] * [-1, 1]
    zoom = 2.4 if candidate.family in {"runway", "taxiway", "port"} else 1.0
    views = []
    opposite_offset = 0 if candidate.family == "crossing" else -15
    for along, across in ((-20, 9), (16, opposite_offset)):
        eye = candidate.xy + zoom * (direction * along + lateral * across)
        dx, dy = candidate.xy - eye
        views.append(
            {
                "at": window.geographic(eye),
                "heading": round(float(np.rad2deg(np.arctan2(dx, dy)) % 360), 2),
                "eye_m": 1.7,
                "pitch": -3,
            }
        )
    return views


def case_row(window, candidate, preferred, serial):
    evidence = list(
        dict.fromkeys(
            json.dumps(f.evidence(), sort_keys=True) for f in candidate.features
        )
    )
    scene_time = datetime(2026, 6 if window.latitude >= 0 else 12, 21, 10, tzinfo=UTC)
    scene_time -= timedelta(hours=round(window.longitude / 15))
    return {
        "id": f"{window.continent.lower()}-{window.area}-{serial}",
        "continent": window.continent,
        "split": window.split,
        "family": candidate.family,
        "preferred": preferred,
        "at": window.geographic(candidate.xy),
        "radius_m": CASE_RADIUS_M,
        "evidence": [json.loads(row) for row in evidence],
        "links": [
            [
                feature.family,
                window.geographic(start),
                window.geographic(end),
                feature.properties.get("oneway", 0),
                feature.properties.get("access"),
                feature.properties.get("layer", 0),
            ]
            for feature, start, end in candidate.links
        ],
        "checks": candidate.family,
        "views": cameras(window, candidate),
        "utc": scene_time.isoformat(),
        "weather": "clear",
    }


def collect(window, vectors):
    tiles = covering_tiles(window.latitude, window.longitude, RADIUS_M, 14)
    features = [
        feature for tile in tiles for feature in extract(window, tile, vectors[tile])
    ]
    candidates = list(geometric_candidates(features))
    if {"junction", "crossing", "turnout"}.intersection(window.preferred):
        candidates.extend(shared_nodes(features))
    candidates = [c for c in candidates if np.linalg.norm(c.xy) <= RADIUS_M]
    selected = []
    for serial, preferred in enumerate(window.preferred, 1):
        remaining = [
            c
            for c in candidates
            if all(np.linalg.norm(c.xy - position) >= 55 for position in selected)
        ]
        if not remaining:
            raise ValueError(f"{window.area} has insufficient visible source geometry")
        candidate = min(
            remaining,
            key=lambda c: (
                c.family != preferred,
                c.family in {"waterway", "path"} and c.family not in window.preferred,
                np.linalg.norm(c.xy),
                c.family,
                tuple(c.xy),
            ),
        )
        selected.append(candidate.xy)
        yield case_row(window, candidate, preferred, serial)


def fetch_tiles(kind, tiles, cache):
    def get(tile):
        zxy = "/".join(map(str, tile))
        url = (
            f"https://tiles.openfreemap.org/planet/{SNAPSHOT}/{zxy}.pbf"
            if kind == "vector"
            else f"https://tiles.mapterhorn.com/{zxy}.webp"
        )
        return tile, acquire((kind, tile, url), cache, fetch=True, timeout_s=24)

    with ThreadPoolExecutor(max_workers=6) as executor:
        return dict(executor.map(get, sorted(tiles)))


def pinned(receipt):
    return {k: receipt[k] for k in ("tile", "url", "sha256", "bytes") if k in receipt}


def case_tiles(case, zoom):
    return covering_tiles(*case["at"], case["radius_m"], zoom)


def build_catalog(windows, cache):
    tiles = {
        t
        for w in windows
        for t in covering_tiles(w.latitude, w.longitude, RADIUS_M, 14)
    }
    receipt = fetch_tiles("vector", tiles, cache)
    decoded = {
        tile: decode_vector(item, layers=LAYERS) for tile, item in receipt.items()
    }
    cases = [case for window in windows for case in collect(window, decoded)]
    local_tiles = {tile for case in cases for tile in case_tiles(case, 14)}
    receipt.update(fetch_tiles("vector", local_tiles - tiles, cache))
    elevation = fetch_tiles(
        "elevation",
        {tile for case in cases for tile in case_tiles(case, 12)},
        cache,
    )
    for case in cases:
        if any(elevation[tile].get("missing") for tile in case_tiles(case, 12)):
            raise ValueError(f"missing elevation tiles for {case['id']}")
        case["tiles"] = sorted(
            {
                "/".join(map(str, tile))
                for zoom in (12, 14)
                for tile in case_tiles(case, zoom)
            }
            | {entry[0] for entry in case["evidence"]}
        )
    return {
        "version": 1,
        "source_snapshot": SNAPSHOT,
        "checks": CHECKS,
        "cases": cases,
        "sources": {
            "vector": [pinned(receipt[tile]) for tile in sorted(receipt)],
            "elevation": [pinned(elevation[tile]) for tile in sorted(elevation)],
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=Path("build/reference-sources"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    catalog = build_catalog(load_survey(), args.cache)
    with args.output.open("w") as stream:
        json.dump(
            {key: value for key, value in catalog.items() if key != "cases"},
            stream,
            indent=2,
        )
        stream.seek(stream.tell() - 1)
        stream.write(',\n  "cases": [\n')
        stream.write(
            ",\n".join(
                "    " + json.dumps(case, separators=(",", ":"))
                for case in catalog["cases"]
            )
        )
        stream.write("\n  ]\n}\n")
    print(Counter((case["continent"], case["split"]) for case in catalog["cases"]))
    print(Counter(case["family"] for case in catalog["cases"]))


if __name__ == "__main__":
    main()

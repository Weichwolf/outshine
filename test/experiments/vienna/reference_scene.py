"""Pinned generator products and geodetic pedestrian cameras shared by both GPU paths."""

import json
import math
from datetime import datetime
from functools import cached_property, partial

import numpy as np
import shapely
from astral import Observer
from astral.sun import azimuth, elevation
from bank_vegetation import bank_plants
from bridge_approach import APPROACH_SEARCH, bridge_approaches
from bridges import plan_water_crossings
from catalog import HERE, ROOT
from facade import FacadeView
from grass_cover import blades
from horizon import terrain_horizon
from meadow import meadow_cover
from outshine_ref.places.height import Elevation
from regional import (
    beyond_region,
    bridgehead_sources,
    regional_meshes,
    regional_sources,
    regional_transport,
    tile_rect,
)
from river_control import RiverControl, control_surfaces
from scene_mesh import assemble, terrain_grid
from source_scene import LocalScene
from understory import woodland_cover
from viaducts import local_viaducts
from water_bed import nearshore_bed
from water_surface import water_bodies
from woodland import habitat, tree_batches
from woodland_shrubs import woodland_shrubs

SUPPORT_MATERIALS = {
    "ground": ("ground", "grass"),
    "sidewalk": ("sidewalk", "paving", "unpaved path"),
    "street": ("sidewalk", "paving", "unpaved path", "road"),
}


def direction(bearing, altitude):
    yaw, pitch = np.deg2rad([bearing, altitude])
    return np.array(
        [np.sin(yaw) * np.cos(pitch), np.cos(yaw) * np.cos(pitch), np.sin(pitch)]
    )


def support_height(mesh, xy=(0.0, 0.0)):
    vertices, faces = mesh
    if not len(faces):
        return np.empty(0)
    tri = np.asarray(vertices)[np.asarray(faces, dtype=int)]
    a, b, c = tri.transpose(1, 0, 2)
    edge = b - a
    side = c - a
    cross = lambda p, q: p[:, 0] * q[:, 1] - p[:, 1] * q[:, 0]
    det = cross(edge, side)
    keep = abs(det) > 1e-12
    point = np.asarray(xy) - a[keep, :2]
    w = cross(point, side[keep]) / det[keep]
    v = cross(edge[keep], point) / det[keep]
    hits = (w >= -1e-7) & (v >= -1e-7) & (w + v <= 1 + 1e-7)
    return (a[keep, 2] + w * edge[keep, 2] + v * side[keep, 2])[hits]


def local_bridge_approaches(source, spans, domain, region):
    if not spans:
        return []
    features = []
    for receipt in region:
        if receipt["kind"] != "vector":
            continue
        nw, se = tile_rect(source, receipt["tile"])
        tile = shapely.box(*np.minimum(nw, se), *np.maximum(nw, se))
        if tile.intersects(domain):
            features.extend(
                (name, geom, tags)
                for name, geom, tags in source.metric_features(
                    receipt, ("transportation", "water")
                )
                if geom.intersects(domain)
            )
    water = shapely.union_all(
        [shapely.make_valid(geom) for name, geom, _ in features if name == "water"]
    )
    return bridge_approaches(spans, features, source.height, water)


def reference_water(source, transport, domain, radius, config):
    receipts = {
        row["url"]: row
        for row in [*source.receipts, *transport.receipts]
        if row["kind"] == "vector"
    }

    def features():
        for url, row in receipts.items():
            nw, se = tile_rect(source, row["tile"])
            bounds = shapely.box(*np.minimum(nw, se), *np.maximum(nw, se))
            for name, shape, tags in source.metric_features(
                row, ("water",), transport.nearby.get(url)
            ):
                yield name, shapely.make_valid(shape).intersection(bounds), tags

    dem = Elevation([*source.receipts, *transport.receipts])

    def regional_height(points):
        lon, lat = (points / source.metres + [source.lon, source.lat]).T
        return dem.sample(lat, lon) - source.datum

    samples = terrain_grid(source, radius)[0][:, :2]
    recipe = json.loads((HERE / "water_controls.json").read_text())
    if recipe["source_snapshot"] != config["source_snapshot"]:
        raise ValueError("Water controls require the pinned vector snapshot")
    return tuple(
        water_bodies(
            features(),
            domain,
            samples,
            source.height,
            regional_height,
            source=source,
            controls=tuple(
                RiverControl.from_recipe(control)
                for control in recipe["water_controls"]
            ),
        )
    )


class ReferenceScene:
    def __init__(self, place, config):
        cache = ROOT / "build/vienna-sources"
        self.source = LocalScene(place, config["source_snapshot"], cache, 250)
        domain = shapely.box(-250, -250, 250, 250)
        self.viaducts = local_viaducts(self.source.features, domain, self.source.height)
        water = shapely.union_all(
            [
                shapely.make_valid(geom)
                for name, geom, _ in self.source.features
                if name == "water"
            ]
        )
        crossings = list(
            plan_water_crossings(
                self.source.features, domain, water, self.source.height
            )
        )
        spans = [
            *((deck.footprint, deck) for deck in self.viaducts),
            *((crossing.footprint, crossing.span) for crossing in crossings),
        ]
        self.fov = place.get("vertical_fov_deg", config["vertical_fov_deg"])
        self.focal = 360 / math.tan(math.radians(self.fov) / 2)
        pins = json.loads((HERE / "sources.json").read_text())
        self.bridgeheads = partial(
            bridgehead_sources,
            self.source,
            snapshot=config["source_snapshot"],
            cache=cache,
            pins=pins,
        )
        transport_domain = shapely.union_all(
            [
                shapely.Point(end).buffer(APPROACH_SEARCH)
                for _, span in spans
                for end in (span.origin, span.origin + span.length * span.direction)
            ]
        )
        region = regional_sources(
            self.source,
            place,
            config["source_snapshot"],
            cache,
            self.fov,
            transport_domain=transport_domain,
            pins=pins,
        )
        self.far, self.faces, receipts = terrain_horizon(self.source, cache, 250)
        for row in self.source.receipts + receipts + region:
            if pins.get(row["url"]) != row["sha256"]:
                raise ValueError(f"Unpinned or changed Vienna source: {row['url']}")
        approaches = local_bridge_approaches(
            self.source, spans, transport_domain, region
        )
        self.transport = regional_transport(
            self.source,
            region,
            self.focal,
            250,
            viaducts=self.viaducts,
            local_approaches=approaches,
            connect_bridgeheads=self.bridgeheads,
        )
        self.water_plan = reference_water(
            self.source, self.transport, domain, 250, config
        )
        xy = place.get("offset_east_north_m", (0.0, 0.0))
        if "absolute_eye_m" in place:
            view_z = place["absolute_eye_m"] - self.source.datum
        else:
            view_z = self.source.height(np.array([xy]))[0] + config["eye_height_m"]
        self.local = assemble(
            self.source,
            250,
            self.viaducts,
            approaches,
            crossings=crossings,
            water_plan=self.water_plan,
            facade_view=FacadeView(tuple(xy), self.focal, view_z),
        )
        self.water_bed = nearshore_bed(self.local.shore, domain, source=self.source)
        self.look = direction(place["heading_deg"], place["pitch_deg"])
        observer = Observer(place["latitude"], place["longitude"])
        utc = datetime.fromisoformat(place.get("utc", config["utc"]))
        self.sun = direction(azimuth(observer, utc), elevation(observer, utc))
        if "absolute_eye_m" in place:
            self.support = "absolute station"
            z = view_z
        else:
            names = SUPPORT_MATERIALS.get(place["support"], (place["support"],))
            hits = [
                (h, n)
                for n in names
                if n in self.local
                for h in support_height(self.local[n], xy)
            ]
            if not hits:
                raise ValueError(f"{place['id']}: no walkable surface under camera")
            z, self.support = max(hits)
            z += config["eye_height_m"]
        self.eye = np.array([*xy, z])

    def surfaces(self):
        for name, (verts, faces) in self.local.items():
            yield name, verts, faces
        yield from control_surfaces(self.source, self.water_plan)
        bounds = []
        for products, tile in regional_meshes(
            self.source,
            self.transport,
            self.focal,
            250,
            self.water_plan,
            local_shore=self.local.shore.protection,
        ):
            yield from products
            bounds.append(tile)
        yield "distant terrain", *beyond_region(self.far, self.faces, bounds)

    @cached_property
    def woodland_floor(self):
        return habitat(self.source, stems=False)

    def grass_and_understory(self, seed):
        if "grass" in self.local:
            yield from blades(self.local["grass"], self.source, self.eye, seed)
        if "meadow" in self.local:
            yield from meadow_cover(self.local["meadow"], self.source, self.eye, seed)
        if "ground" in self.local:
            yield from woodland_cover(
                self.local["ground"], self.source, self.woodland_floor, self.eye, seed
            )
        yield from bank_plants(
            self.local, self.source, self.local.shore, self.eye, seed
        )

    def woody_plants(self, seed):
        yield from tree_batches(
            self.source,
            250,
            seed,
            self.focal,
            self.woodland_floor,
            eye_xy=self.eye[:2],
            soil=self.local,
        )
        if "ground" in self.local:
            yield from woodland_shrubs(
                self.local["ground"], self.source, self.woodland_floor, self.eye, seed
            )

"""Camera-driven world products shared by both reference renderers."""

import math
from datetime import datetime
from functools import cached_property, partial

import numpy as np
import shapely
from astral import Observer
from astral.sun import azimuth, elevation

from outshine.buildings.facade import FacadeView
from outshine.places.height import Elevation
from outshine.terrain.ground_surface import (
    CARRIAGEWAY_MATERIALS,
    MEADOW_MARGIN_M,
    MINERAL_TRAIL_SHOULDER_M,
    ROUTE_MATERIALS,
    SOIL_MATERIALS,
    TRAIL_SHOULDER_M,
    dirt_road_weights,
    margin_weights,
)
from outshine.terrain.horizon import terrain_horizon
from outshine.terrain.horizon_cover import distant_cover
from outshine.transport.bridge_approach import APPROACH_SEARCH, bridge_approaches
from outshine.transport.bridges import plan_water_crossings
from outshine.transport.viaducts import local_viaducts
from outshine.vegetation.atlas import VegetationAtlas
from outshine.vegetation.bank_vegetation import bank_plants
from outshine.vegetation.grass_cover import blades
from outshine.vegetation.landscape_cover import landscape_cover
from outshine.vegetation.meadow import meadow_cover
from outshine.vegetation.mojave_sites import mojave_yuccas
from outshine.vegetation.open_cover import (
    ACACIA_COMMIPHORA_BUSHLANDS,
    MOJAVE_DESERT,
    RegionalOpenCover,
    dry_savanna_season,
)
from outshine.vegetation.patagonian_shrub import PATAGONIAN_COMMUNITIES
from outshine.vegetation.regional_woody import savanna_woody, steppe_shrubs
from outshine.vegetation.understory import woodland_cover
from outshine.vegetation.woodland import habitat, tree_batches
from outshine.vegetation.woodland_shrubs import woodland_shrubs
from outshine.water.river_control import RiverControl, control_surfaces
from outshine.water.shore_substrate import mineral_shore
from outshine.water.water_bed import nearshore_bed
from outshine.water.water_surface import water_bodies
from outshine.weather.air_profile import aerosol_extinction, direct_sun_transmission
from outshine.weather.cloud_profile import cloud_profile
from outshine.world.mesh import assemble, terrain_grid
from outshine.world.regional import (
    beyond_region,
    bridgehead_sources,
    regional_meshes,
    regional_sources,
    regional_stands,
    regional_transport,
    skyline_meshes,
    skyline_sources,
    tile_rect,
)
from outshine.world.sources import LocalScene

SUPPORT_MATERIALS = {
    "ground": ("ground", "forest floor", "grass"),
    "sidewalk": ("sidewalk", "paving", "unpaved path"),
    "street": ("sidewalk", "paving", "unpaved path", *CARRIAGEWAY_MATERIALS),
    "road": CARRIAGEWAY_MATERIALS,
}
SAVANNA_SURFACES = {
    "ground": "dry savanna soil",
    "grass": "dry savanna soil",
    "unpaved road": "savanna unpaved road",
}
PATAGONIAN_SURFACES = {
    "ground": "patagonian gravel soil",
    "grass": "patagonian gravel soil",
    "unpaved path": "patagonian footpath",
    "unpaved road": "patagonian unpaved road",
}
MOJAVE_SURFACES = {
    "ground": "mojave gravelly sand",
    "grass": "mojave gravelly sand",
    "unpaved path": "mojave footpath",
    "unpaved road": "mojave unpaved road",
}


def soil_surfaces(sample, time):
    if sample is None:
        return {}
    soil = sample.soil
    if soil is None or soil.sand_g_kg is None:
        return {}
    if (
        sample.ecoregion_id == ACACIA_COMMIPHORA_BUSHLANDS
        and dry_savanna_season(time)
        and soil.clay_g_kg is not None
        and 450 <= soil.sand_g_kg <= 650
        and 200 <= soil.clay_g_kg <= 380
    ):
        return SAVANNA_SURFACES
    if (
        sample.potential_community in PATAGONIAN_COMMUNITIES
        and soil.coarse_fragments_pct is not None
        and 450 <= soil.sand_g_kg <= 750
        and soil.coarse_fragments_pct >= 5
    ):
        return PATAGONIAN_SURFACES
    if (
        sample.ecoregion_id == MOJAVE_DESERT
        and soil.sand_g_kg >= 490
        and soil.coarse_fragments_pct is not None
        and soil.coarse_fragments_pct >= 5
    ):
        return MOJAVE_SURFACES
    return {}


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


def reference_water(source, transport, domain, radius, controls):
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
    return tuple(
        water_bodies(
            features(),
            domain,
            samples,
            source.height,
            regional_height,
            source=source,
            controls=tuple(RiverControl.from_recipe(control) for control in controls),
        )
    )


class ReferenceScene:
    def __init__(self, place, config):
        cache = config["source_cache"]
        pins = config.get("source_pins")
        self.source = LocalScene(
            place, config["source_snapshot"], cache, 250, pins=pins
        )
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
        self.canopy_seed = config["seed"]
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
        self.terrain_cover_sources = (
            self.source,
            place,
            config["source_snapshot"],
            self.focal,
            cache,
            pins,
        )
        self.skyline = skyline_sources(
            self.source,
            place,
            config["source_snapshot"],
            cache,
            self.focal,
            region,
            pins,
        )
        self._base_receipts = (*self.source.receipts, *receipts, *region, *self.skyline)
        if pins is not None:
            for row in self._base_receipts:
                url = row["url"]
                if url not in pins or pins[url] != row.get("sha256"):
                    raise ValueError(f"Unpinned or changed reference source: {url}")
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
            self.source, self.transport, domain, 250, config.get("water_controls", ())
        )
        xy = place.get("offset_east_north_m", (0.0, 0.0))
        if "absolute_eye_m" in place:
            view_z = place["absolute_eye_m"] - self.source.datum
        else:
            view_z = self.source.height(np.array([xy]))[0] + config["eye_height_m"]
        utc = datetime.fromisoformat(place.get("utc", config["utc"]))
        atlas_path = config.get("vegetation_atlas")
        atlas = VegetationAtlas(atlas_path) if atlas_path is not None else None
        sample = atlas.sample(self.source.lat, self.source.lon) if atlas else None
        self.soil_surfaces = soil_surfaces(sample, utc)
        self.local = assemble(
            self.source,
            250,
            self.viaducts,
            approaches,
            crossings=crossings,
            water_plan=self.water_plan,
            facade_view=FacadeView(tuple(xy), self.focal, view_z),
            blend_mineral_trails=self.soil_surfaces is MOJAVE_SURFACES,
        )
        self.water_bed = nearshore_bed(self.local.shore, domain, source=self.source)
        self.look = direction(place["heading_deg"], place["pitch_deg"])
        observer = Observer(place["latitude"], place["longitude"])
        self.mineral_shore = (
            mineral_shore(self.local.shore, atlas.shore_substrates) if atlas else None
        )
        self.bare_shore = self.mineral_shore.bare if self.mineral_shore else None
        self.open_cover = (
            RegionalOpenCover(atlas, self.source, utc, exclusion=self.bare_shore)
            if atlas is not None
            else None
        )
        self.sun = direction(azimuth(observer, utc), elevation(observer, utc))
        self.weather = place.get("weather", config.get("weather", "clear"))
        self.aerosol_extinction = aerosol_extinction(self.weather)
        if "absolute_eye_m" in place:
            self.support = "absolute station"
            z = view_z
        else:
            support = place.get("support")
            names = (
                SUPPORT_MATERIALS.get(support, (support,))
                if support is not None
                else (*SOIL_MATERIALS, *ROUTE_MATERIALS, "bridge deck", "water")
            )
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
        self.clouds = cloud_profile(
            self.weather,
            place["longitude"],
            place["latitude"],
            xy,
            (utc - datetime.fromisoformat(config["utc"])).total_seconds(),
            config["seed"],
        )
        self.direct_sun_transmission = direct_sun_transmission(
            z + self.source.datum, self.sun[2], self.aerosol_extinction
        )

    def surfaces(self, *, crown_sites=None):
        for name, (verts, faces) in self.local.items():
            yield self.soil_surfaces.get(name, name), verts, faces
        yield from control_surfaces(self.source, self.water_plan)
        bounds = []
        for products, tile in regional_meshes(
            self.source,
            self.transport,
            self.focal,
            250,
            self.water_plan,
            crown_sites=crown_sites,
            seed=self.canopy_seed,
            local_shore=self.local.shore.protection,
            local_terrain=self.local.terrain_lattice,
            stands=self.stands if crown_sites is not None else None,
        ):
            yield from products
            bounds.append(tile)
        yield "distant terrain", *beyond_region(self.far, self.faces, bounds)
        yield from skyline_meshes(self.source, self.skyline, self.focal, 250)

    @cached_property
    def terrain_cover(self):
        return distant_cover(*self.terrain_cover_sources)

    @cached_property
    def landscape_cover(self):
        return landscape_cover(self.open_cover, self.eye, self.canopy_seed)

    @cached_property
    def shore_appearance(self):
        if self.mineral_shore is None:
            return None
        return self.mineral_shore.appearance(self.source, self.eye, self.canopy_seed)

    @property
    def source_receipts(self):
        """Report the terrain and vector inputs used by the reference scene."""
        return (
            *self._base_receipts,
            *self.transport.receipts,
            *self.terrain_cover.receipts,
        )

    @cached_property
    def woodland_floor(self):
        ground = habitat(self.source, stems=False)
        if ground is not None and self.bare_shore is not None:
            return ground.difference(self.bare_shore)
        return ground

    @cached_property
    def stands(self):
        return regional_stands(
            self.source, self.transport, self.focal, 250, self.canopy_seed
        )

    def grass_and_understory(self, seed):
        exclude = self.ground_cover_exclusion if self.open_cover else None
        if "grass" in self.local:
            yield from blades(
                self.local["grass"], self.source, self.eye, seed, exclude=exclude
            )
        if "meadow" in self.local:
            yield from meadow_cover(
                self.local["meadow"],
                self.source,
                self.eye,
                seed,
                focal_pixels=self.focal,
                woodland_edges=self.local.meadow_woodland_edges,
                exclude=exclude,
            )
        if self.open_cover is not None:
            yield from self.open_cover.batches(self.local, self.eye, seed)
        if "forest floor" in self.local:
            yield from woodland_cover(
                self.local["forest floor"],
                self.source,
                self.woodland_floor,
                self.eye,
                seed,
            )
        yield from bank_plants(
            self.local,
            self.source,
            self.local.shore,
            self.eye,
            seed,
            exclusion=self.bare_shore,
        )

    def ground_cover_exclusion(self, xy):
        exclusion = self.open_cover.replaces(xy)
        if self.bare_shore is not None:
            exclusion |= shapely.intersects_xy(self.bare_shore, *xy.T)
        return exclusion

    def ground_blend(self, name, vertices):
        if (
            name == "mojave footpath"
            and vertices is self.local.get("unpaved path", (None,))[0]
            and self.local.mineral_trail_edges is not None
        ):
            return margin_weights(
                self.local.mineral_trail_edges, vertices, MINERAL_TRAIL_SHOULDER_M
            )
        if name == self.soil_surfaces.get("unpaved road"):
            name = "unpaved road"
        if vertices is not self.local.get(name, (None,))[0]:
            return None
        if name == "unpaved road" and self.local.dirt_road_edges is not None:
            return dirt_road_weights(self.local.dirt_road_edges, vertices)
        if name == "meadow" and self.local.meadow_woodland_edges is not None:
            return margin_weights(
                self.local.meadow_woodland_edges, vertices, MEADOW_MARGIN_M
            )
        if name == "unpaved path" and self.local.trail_litter_edges is not None:
            return margin_weights(
                self.local.trail_litter_edges, vertices, TRAIL_SHOULDER_M
            )
        return None

    def woody_plants(self, seed, *, eye_route=None, skylight=False):
        if self.open_cover is not None:
            yield from savanna_woody(self.open_cover, self.local, self.eye[:2], seed)
            yield from steppe_shrubs(self.open_cover, self.local, self.eye[:2], seed)
            yield from mojave_yuccas(self.open_cover, self.local, self.eye[:2], seed)
        root_domain = shapely.union_all((self.woodland_floor, self.stands.verges))
        if self.bare_shore is not None:
            root_domain = root_domain.difference(self.bare_shore)
        yield from tree_batches(
            self.source,
            250,
            seed,
            self.focal,
            root_domain,
            eye_xy=self.eye[:2],
            soil=self.local,
            eye_route=eye_route,
            mature=self.stands.woodland,
            riverside=self.stands.riverside,
            skylight=skylight,
        )
        if "forest floor" in self.local:
            yield from woodland_shrubs(
                self.local["forest floor"],
                self.source,
                self.woodland_floor,
                self.eye,
                seed,
            )

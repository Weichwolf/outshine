"""Offline source layers and habitat evidence on a shared geographic grid."""

import hashlib
import io
import json
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path

import numpy as np
from shapely.geometry import Point

from ..terrain.land_cover import COVER_PRIORITY, cover_material
from .species_catalog import load_species_catalog, species_catalog_digest

TILE_CELLS = 240
CELLS_PER_DEGREE = 12_000
TILES_PER_DEGREE = CELLS_PER_DEGREE // TILE_CELLS
SOIL_LAYERS = ("sand", "silt", "clay", "cfvo", "soc")
NO_SOIL = -32768
COVER_CLASSES = {
    10: "wood",
    20: "scrub",
    30: "grass",
    40: "cropland",
    50: "settlement",
    60: "sparse",
    70: "ice",
    80: "water",
    90: "wetland",
    95: "mangrove",
    100: "moss and lichen",
}
MAPPED_CLASSES = frozenset(COVER_CLASSES.values()) | {
    "meadow",
    "orchard",
    "vineyard",
    "sand",
    "scree",
    "bare rock",
    "quarry",
}
CROP_SURFACES = {"green crop", "dry crop", "field soil"}
GROWTH_FORMS = {
    "wood": ("trees", "shrubs", "herbs", "graminoids"),
    "scrub": ("shrubs", "herbs", "graminoids"),
    "grass": ("graminoids", "herbs"),
    "meadow": ("graminoids", "herbs"),
    "wetland": ("wetland graminoids", "wetland herbs"),
    "mangrove": ("mangrove trees", "wetland herbs"),
    "moss and lichen": ("mosses and lichens",),
    "sparse": ("sparse herbs", "sparse shrubs"),
}
WOODED_BIOMES = {
    1: ("evergreen broadleaf trees", "lianas", "ferns", "shade herbs"),
    2: ("drought-deciduous trees", "dry woodland shrubs", "herbs"),
    3: ("conifers", "broadleaf trees", "herbs"),
    4: ("deciduous broadleaf trees", "shrubs", "herbs", "graminoids"),
    5: ("conifers", "broadleaf trees", "ferns", "shrubs"),
    6: ("boreal conifers", "dwarf shrubs", "mosses"),
    7: ("open woodland trees", "shrubs", "graminoids"),
    12: ("sclerophyll trees", "sclerophyll shrubs", "herbs"),
}
RASTER_DTYPES = {
    "cover": np.dtype("u1"),
    "ecoregion": np.dtype("<u2"),
    "botanical": np.dtype("<u2"),
    **dict.fromkeys(SOIL_LAYERS, np.dtype("<i2")),
}


def tile_bounds(key):
    x, y = key
    return (
        x / TILES_PER_DEGREE - 180,
        90 - (y + 1) / TILES_PER_DEGREE,
        (x + 1) / TILES_PER_DEGREE - 180,
        90 - y / TILES_PER_DEGREE,
    )


def grid_indices(latitude, longitude):
    latitude, longitude = np.broadcast_arrays(
        np.asarray(latitude, dtype=float), np.asarray(longitude, dtype=float)
    )
    if not (
        np.isfinite(latitude).all()
        and np.isfinite(longitude).all()
        and (np.abs(latitude) <= 90).all()
        and (np.abs(longitude) <= 180).all()
    ):
        raise ValueError("latitude and longitude outside the atlas grid")
    x = np.floor((longitude + 180) * CELLS_PER_DEGREE + 1e-8).astype(np.int32)
    y = np.minimum(
        np.floor((90 - latitude) * CELLS_PER_DEGREE + 1e-8).astype(np.int32),
        180 * CELLS_PER_DEGREE - 1,
    )
    return x % (360 * CELLS_PER_DEGREE), y


def grid_cell(latitude, longitude):
    x, y = map(int, grid_indices(latitude, longitude))
    return (x // TILE_CELLS, y // TILE_CELLS), (y % TILE_CELLS, x % TILE_CELLS)


def tile_name(key):
    return f"{key[0]}-{key[1]}"


@dataclass(frozen=True, slots=True)
class SoilPrior:
    sand_g_kg: int | None
    silt_g_kg: int | None
    clay_g_kg: int | None
    coarse_fragments_pct: float | None
    organic_carbon_g_kg: float | None


@dataclass(frozen=True, slots=True)
class HabitatSample:
    cover: str | None
    cover_source: str | None
    worldcover_code: int | None
    ecoregion_id: int | None
    biome_id: int | None
    botanical_region: str | None
    growth_forms: tuple[str, ...]
    native_candidates: tuple[str, ...]
    introduced_candidates: tuple[str, ...]
    soil: SoilPrior | None


def soil_prior(values):
    sand, silt, clay, fragments, carbon = (
        None if value == NO_SOIL else value for value in values
    )
    if all(value is None for value in (sand, silt, clay, fragments, carbon)):
        return None
    return SoilPrior(
        sand,
        silt,
        clay,
        None if fragments is None else fragments / 10,
        None if carbon is None else carbon / 10,
    )


def mapped_cover_at(features, xy):
    point = Point(xy)
    covers = set()
    for layer, geometry, props in features:
        if layer == "water":
            if geometry.covers(point):
                return "water"
        elif layer == "building":
            if geometry.covers(point):
                covers.add("settlement")
        elif layer in ("landcover", "landuse"):
            name = cover_material(layer, props)
            if name and geometry.covers(point):
                covers.add(name)
    if "settlement" in covers:
        return "settlement"
    name = max(covers, key=COVER_PRIORITY.__getitem__, default=None)
    return "cropland" if name in CROP_SURFACES else name


class VegetationAtlas:
    """Read versioned tiles and report evidence separately from planting decisions."""

    def __init__(self, root):
        self.root = Path(root)
        self.manifest = json.loads(
            (self.root / "atlas.json").read_text(encoding="utf-8")
        )
        if self.manifest["format"] != "outshine-vegetation-atlas-1":
            raise ValueError("unsupported vegetation atlas format")
        if (
            self.manifest["sources"]["species"]["catalog_sha256"]
            != species_catalog_digest()
        ):
            raise ValueError("atlas requires a matching species catalogue")
        self.species = load_species_catalog()
        self._tiles = lru_cache(maxsize=12)(self._load_tile)

    def _load_tile(self, name):
        record = self.manifest["tiles"].get(name)
        if record is None:
            return None
        digest = record["sha256"]
        data = (self.root / f"{name}-{digest}.npz").read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f"changed vegetation atlas tile {name}")
        with np.load(io.BytesIO(data), allow_pickle=False) as archive:
            layers = {layer: archive[layer] for layer in RASTER_DTYPES}
        if any(
            array.shape != (TILE_CELLS, TILE_CELLS)
            or array.dtype != RASTER_DTYPES[layer]
            for layer, array in layers.items()
        ):
            raise ValueError(f"invalid vegetation atlas tile {name}")
        return layers

    def land_cells(self, latitude, longitude):
        """Read cover codes and ecoregion IDs for broadcast geographic positions."""
        x, y = grid_indices(latitude, longitude)
        cover = np.zeros(x.shape, dtype=np.uint8)
        regions = np.zeros(x.shape, dtype=np.uint16)
        keys, owners = np.unique(
            np.column_stack((x.ravel(), y.ravel())) // TILE_CELLS,
            axis=0,
            return_inverse=True,
        )
        owners = owners.ravel()
        order = np.argsort(owners, kind="stable")
        stops = np.r_[0, np.cumsum(np.bincount(owners, minlength=len(keys)))]
        for (column, row), first, stop in zip(keys, stops[:-1], stops[1:], strict=True):
            layers = self._tiles(tile_name((column, row)))
            if layers is None:
                continue
            rows = order[first:stop]
            cells = y.ravel()[rows] % TILE_CELLS, x.ravel()[rows] % TILE_CELLS
            cover.ravel()[rows] = layers["cover"][cells]
            regions.ravel()[rows] = layers["ecoregion"][cells]
        return cover, regions

    def sample(self, latitude, longitude, *, mapped_cover=None):
        if mapped_cover is not None and mapped_cover not in MAPPED_CLASSES:
            raise ValueError(f"unknown mapped cover {mapped_cover!r}")
        key, cell = grid_cell(latitude, longitude)
        layers = self._tiles(tile_name(key))
        code = ecoregion = region = soil = None
        if layers is not None:
            code = int(layers["cover"][cell]) or None
            ecoregion = int(layers["ecoregion"][cell]) or None
            region_index = int(layers["botanical"][cell])
            region = self.manifest["botanical_regions"][region_index] or None
            soil = soil_prior(int(layers[layer][cell]) for layer in SOIL_LAYERS)
        cover = mapped_cover or COVER_CLASSES.get(code)
        source = "mapped" if mapped_cover else "worldcover" if cover else None
        regions = (region,) if region else ()
        ecoregion_info = self.manifest["ecoregions"].get(str(ecoregion))
        biome = int(ecoregion_info[1]) if ecoregion_info else None
        forms = GROWTH_FORMS.get(cover, ())
        if cover == "wood":
            forms = WOODED_BIOMES.get(biome, forms)
        return HabitatSample(
            cover,
            source,
            code,
            ecoregion,
            biome,
            region,
            forms,
            self.species.candidate_ids(regions),
            self.species.candidate_ids(regions, introduced=True),
            soil,
        )

    def sample_local(self, source, xy):
        """Join the regional layers with the Python scene's mapped footprint."""
        longitude, latitude = np.asarray(xy) / source.metres + [source.lon, source.lat]
        return self.sample(
            latitude, longitude, mapped_cover=mapped_cover_at(source.features, xy)
        )

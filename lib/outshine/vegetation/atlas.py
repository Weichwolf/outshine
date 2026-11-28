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
COMMUNITY_DTYPE = np.dtype("<u2")
OBSERVATION_DTYPE = np.dtype("<u2")
GEOLOGY_DTYPE = np.dtype("<u2")


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
class VegetationObservation:
    map_unit: str
    label: str
    height_class: int
    density_class: int


@dataclass(frozen=True, slots=True)
class GeologyObservation:
    map_unit: str
    label: str
    lithology: str


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
    potential_community: str | None = None
    observed_vegetation: VegetationObservation | None = None
    geology: GeologyObservation | None = None


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
        self.communities = tuple(self.manifest.get("potential_communities", ("",)))
        self.community_indices = {
            code: index for index, code in enumerate(self.communities) if code
        }
        self.observations = (
            None,
            *(
                VegetationObservation(**row)
                for row in self.manifest.get("observations", ())
            ),
        )
        self.observation_indices = {}
        for index, record in enumerate(self.observations[1:], 1):
            self.observation_indices.setdefault(record.map_unit, []).append(index)
        self.geology = (
            None,
            *(GeologyObservation(**row) for row in self.manifest.get("geology", ())),
        )
        self.geology_indices = {
            row.map_unit: index for index, row in enumerate(self.geology[1:], 1)
        }
        self._density_classes = np.array(
            [0, *(row.density_class for row in self.observations[1:])], dtype=np.uint8
        )
        self.shore_substrates = {
            tuple(row["water_key"]): row["profile"]
            for row in self.manifest.get("shore_substrates", ())
        }
        self._tiles = lru_cache(maxsize=12)(self._load_tile)

    def _load_tile(self, name):
        record = self.manifest["tiles"].get(name)
        if record is None:
            return None
        digest = record["sha256"]
        data = (self.root / f"{name}-{digest}.npz").read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f"changed vegetation atlas tile {name}")
        dtypes = RASTER_DTYPES
        if record.get("potential_community"):
            dtypes = dtypes | {"community": COMMUNITY_DTYPE}
        if record.get("observed_vegetation"):
            dtypes = dtypes | {"observation": OBSERVATION_DTYPE}
        if record.get("geology"):
            dtypes = dtypes | {"geology": GEOLOGY_DTYPE}
        with np.load(io.BytesIO(data), allow_pickle=False) as archive:
            layers = {layer: archive[layer] for layer in dtypes}
        if any(
            array.shape != (TILE_CELLS, TILE_CELLS) or array.dtype != dtypes[layer]
            for layer, array in layers.items()
        ):
            raise ValueError(f"invalid vegetation atlas tile {name}")
        if "community" in layers and layers["community"].max() >= len(self.communities):
            raise ValueError(
                f"unknown potential community in vegetation atlas tile {name}"
            )
        if "observation" in layers and layers["observation"].max() >= len(
            self.observations
        ):
            raise ValueError(f"unknown observed vegetation in atlas tile {name}")
        if "geology" in layers and layers["geology"].max() >= len(self.geology):
            raise ValueError(f"unknown geology in atlas tile {name}")
        return layers

    def land_cells(self, latitude, longitude):
        """Read cover codes and ecoregion IDs for broadcast geographic positions."""
        return self._cells(latitude, longitude, {"cover": "u1", "ecoregion": "<u2"})

    def community_mask(self, latitude, longitude, codes):
        """Select mapped potential communities. Missing coverage remains unknown."""
        accepted = [
            self.community_indices[code]
            for code in codes
            if code in self.community_indices
        ]
        if not accepted:
            return np.zeros(np.broadcast(latitude, longitude).shape, dtype=bool)
        (community,) = self._cells(latitude, longitude, {"community": COMMUNITY_DTYPE})
        return np.isin(community, accepted)

    def observed_cells(self, latitude, longitude):
        """Return indexed mapped observations and their source density classes."""
        (indices,) = self._cells(
            latitude, longitude, {"observation": OBSERVATION_DTYPE}
        )
        return indices, self._density_classes[indices]

    def observation_mask(self, indices, codes):
        return np.isin(
            indices,
            [
                index
                for code in codes
                for index in self.observation_indices.get(code, ())
            ],
        )

    def geology_cells(self, latitude, longitude):
        return self._cells(latitude, longitude, {"geology": GEOLOGY_DTYPE})[0]

    def geology_mask(self, indices, codes):
        return np.isin(
            indices,
            [
                self.geology_indices[code]
                for code in codes
                if code in self.geology_indices
            ],
        )

    def community_windows(self, bounds):
        """Report available tiles without reading their raster payloads."""
        return self.evidence_windows(bounds, "potential_community")

    def evidence_windows(self, bounds, field):
        west, south, east, north = bounds
        for name, record in self.manifest["tiles"].items():
            if not record.get(field):
                continue
            tile = tile_bounds(tuple(map(int, name.split("-"))))
            if (
                tile[0] < east
                and tile[2] > west
                and tile[1] < north
                and tile[3] > south
            ):
                yield tile

    def _cells(self, latitude, longitude, fields):
        x, y = grid_indices(latitude, longitude)
        values = tuple(np.zeros(x.shape, dtype=dtype) for dtype in fields.values())
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
            for name, result in zip(fields, values, strict=True):
                if name in layers:
                    result.ravel()[rows] = layers[name][cells]
        return values

    def sample(self, latitude, longitude, *, mapped_cover=None):
        if mapped_cover is not None and mapped_cover not in MAPPED_CLASSES:
            raise ValueError(f"unknown mapped cover {mapped_cover!r}")
        key, cell = grid_cell(latitude, longitude)
        layers = self._tiles(tile_name(key))
        code = ecoregion = region = soil = community = observation = geology = None
        if layers is not None:
            code = int(layers["cover"][cell]) or None
            ecoregion = int(layers["ecoregion"][cell]) or None
            region_index = int(layers["botanical"][cell])
            region = self.manifest["botanical_regions"][region_index] or None
            soil = soil_prior(int(layers[layer][cell]) for layer in SOIL_LAYERS)
            if "community" in layers:
                community = self.communities[layers["community"][cell]] or None
            if "observation" in layers:
                observation = self.observations[layers["observation"][cell]]
            if "geology" in layers:
                geology = self.geology[layers["geology"][cell]]
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
            community,
            observation,
            geology,
        )

    def sample_local(self, source, xy):
        """Join the regional layers with the Python scene's mapped footprint."""
        longitude, latitude = np.asarray(xy) / source.metres + [source.lon, source.lat]
        return self.sample(
            latitude, longitude, mapped_cover=mapped_cover_at(source.features, xy)
        )

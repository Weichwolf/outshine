"""Build the pilot's versioned atlas from cached or explicitly acquired layers."""

import argparse
import hashlib
import io
import json
import shutil
import sqlite3
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
from math import ceil, floor
from pathlib import Path
from tempfile import NamedTemporaryFile, TemporaryDirectory
from urllib.parse import urlencode
from urllib.request import urlopen
from zipfile import ZipFile
from zlib import crc32

import mapbox_vector_tile
import numpy as np
import rasterio
import shapefile
import shapely
from outshine.places.sources import tile_geography
from outshine.vegetation.atlas import (
    CELLS_PER_DEGREE,
    NO_SOIL,
    SOIL_LAYERS,
    TILE_CELLS,
    TILES_PER_DEGREE,
    grid_cell,
    tile_bounds,
    tile_name,
)
from outshine.vegetation.species_catalog import (
    load_species_catalog,
    species_catalog_digest,
)
from outshine.vegetation.stand_habitat import polygon_components
from rasterio.features import rasterize
from rasterio.transform import from_bounds
from rasterio.warp import (
    Resampling,
    reproject,
    transform,
    transform_bounds,
    transform_geom,
)
from rasterio.windows import Window
from shapely.geometry import box, shape

from ..vienna.catalog import ROOT, load_catalog
from .catalog import load_sites, site_views

RESOLVE = {
    "url": "https://storage.googleapis.com/teow2016/Ecoregions2017.zip",
    "release": "Ecoregions2017",
    "license": "CC-BY-4.0",
    "sha256": "be36d6209e443038d02e309f0447c6e7f2a62f5fe60c605ffe90d064952f2a60",
}
TDWG_REVISION = "f68afd99aa94adadaa3e0328e4aadda75ecc4215"
TDWG = {
    "url": f"https://raw.githubusercontent.com/tdwg/wgsrpd/{TDWG_REVISION}/geojson/level3.geojson",
    "release": TDWG_REVISION,
    "license": "TDWG metadata specifies no use constraints. Original boundaries © 1992–1997 ESRI, GMi, used with permission.",
    "sha256": "c172bcf6aba20e19477adc60aebf0023068f0175dca8480f0760b090dcf64840",
}
WORLDCOVER = {
    "url": "https://esa-worldcover.s3.eu-central-1.amazonaws.com/v200/2021/map/",
    "release": "2021 v200",
    "license": "CC-BY-4.0",
    "attribution": "© ESA WorldCover project / Contains modified Copernicus Sentinel data (2021) processed by ESA WorldCover consortium",
    "grid_degrees": 1 / CELLS_PER_DEGREE,
}
SOILGRID = {
    "url": "https://maps.isric.org/",
    "release": "SoilGrids 2.0, 0–5 cm, mean",
    "license": "CC-BY-4.0",
    "resolution_m": 250,
}
CHILE_COMMUNITIES = {
    "url": "https://geoportal.cl/geoportal/catalog/download/d9aca777-8e22-35ee-96a1-c8cfe7d8019a",
    "release": "Pisos vegetacionales de Luebert y Pliscoff 2017",
    "attribution": "Chile, Ministerio del Medio Ambiente",
    "license": "Public download from Geoportal de Chile; catalog entry specifies no license.",
    "sha256": "8234bfcee6a936b6811da379ac33ea254faabf4e040eda6cecd0675e84189f5d",
}
CHILE_EXTENT = box(-75.70, -55.99, -66.41, -17.49)
CHILE_CRS = "EPSG:5360"
JOTR_EXTENT = box(-116.20, 33.98, -116.14, 34.04)
JOTR_OBSERVATIONS = {
    "url": (
        "https://services2.arcgis.com/Uq9r85Potqm3MfRV/ArcGIS/rest/services/"
        "biosds730_fpu/FeatureServer/0/query?f=geojson&where=1%3d1"
        "&geometry=-116.20%2c33.98%2c-116.14%2c34.04&geometryType=esriGeometryEnvelope"
        "&inSR=4326&outSR=4326&outFields=OBJECTID%2cPI%2cHEIGHT%2cDENSITY%2cMapUnit"
        "&orderByFields=OBJECTID"
    ),
    "metadata_url": "https://filelib.wildlife.ca.gov/public/BDB/GIS/BIOS/metadata/DS0730.html",
    "release": "Vegetation - Joshua Tree National Park - 2012 [ds730], edition 2",
    "attribution": "National Park Service and Aerial Information Systems via CDFW BIOS",
    "license": "Public CDFW BIOS service. Source metadata specifies use limitations and disclaimers; no standard license named.",
    "sha256": "aefa240e85803e3d19bb6687729d09305623de8b76611da521bd6dec5cd41540",
    "bounds_wgs84": JOTR_EXTENT.bounds,
    "source_schema": {
        "map_unit": "PI",
        "height_class": "HEIGHT",
        "density_class": "DENSITY",
    },
}
JOTR_GEOLOGY = {
    "url": "https://irma.nps.gov/DataStore/DownloadFile/678003?Reference=2209598",
    "release": "NPS Geologic Resources Inventory JOTR GeoPackage 2022",
    "attribution": "National Park Service, after Powell, Matti and Cossette (USGS OFR 2015-1175)",
    "license": "Public NPS GeoPackage. See publication metadata for use limitations.",
    "metadata_url": "https://irma.nps.gov/DataStore/Reference/Profile/2209598",
    "sha256": "54ff10725a894f46fba612c191a71024c27d3d7c5ec0ad2bfff7e1b3b8f9cd4c",
    "bounds_wgs84": JOTR_EXTENT.bounds,
    "table": "JOTR_Geologic_Units",
    "crs": "EPSG:26911",
}
JAPAN_OBSERVATIONS = {
    "url": "https://www.biodic.go.jp/kiso/vg/tile/veg2024vector/{z}/{x}/{y}.pbf",
    "metadata_url": "https://www.biodic.go.jp/kiso/vg/vg_kiso.html",
    "license_url": "https://www.biodic.go.jp/copyright/terms_of_service.html",
    "release": "Actual Vegetation Map, veg2024",
    "attribution": "Biodiversity Center of Japan, Ministry of the Environment",
    "license": "Public Data License 1.0. Clipped and rasterized for the Outshine atlas.",
}
HOMOLOSINE = "+proj=igh +ellps=WGS84 +units=m"
SOIL_CRS = "http://www.opengis.net/def/crs/EPSG/0/152160"


def write_bytes(path, payload):
    with NamedTemporaryFile(dir=path.parent, delete=False) as stream:
        stream.write(payload)
        temporary = Path(stream.name)
    try:
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def cached_bytes(path, url, fetch):
    if path.is_file() or not fetch:
        return path.read_bytes()
    with urlopen(url, timeout=40) as response:
        payload = response.read()
    write_bytes(path, payload)
    return payload


def pinned_source(root, name, source, fetch):
    path = root / name
    if not path.is_file() and fetch:
        with (
            urlopen(source["url"], timeout=90) as response,
            NamedTemporaryFile(dir=root, delete=False) as stream,
        ):
            temporary = Path(stream.name)
            try:
                shutil.copyfileobj(response, stream)
                stream.flush()
                temporary.replace(path)
            finally:
                temporary.unlink(missing_ok=True)
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != source["sha256"]:
        raise ValueError(f"changed atlas source {path}")
    return path


def extract_shapes(archive, cache):
    with ZipFile(archive) as source:
        for suffix in ("dbf", "shp", "shx"):
            path = cache / f"Ecoregions2017.{suffix}"
            info = source.getinfo(path.name)
            if path.is_file():
                with path.open("rb") as stream:
                    checksum = 0
                    for block in iter(lambda: stream.read(1024**2), b""):
                        checksum = crc32(block, checksum)
                if checksum == info.CRC:
                    continue
            with (
                source.open(info) as packed,
                NamedTemporaryFile(dir=cache, delete=False) as stream,
            ):
                temporary = Path(stream.name)
                try:
                    shutil.copyfileobj(packed, stream)
                    stream.flush()
                    temporary.replace(path)
                finally:
                    temporary.unlink(missing_ok=True)


def worldcover_window(bounds, path, fetch):
    west, south, east, north = bounds
    latitude = 3 * floor((south + north) / 6)
    longitude = 3 * floor((west + east) / 6)
    name = f"{'N' if latitude >= 0 else 'S'}{abs(latitude):02d}"
    name += f"{'E' if longitude >= 0 else 'W'}{abs(longitude):03d}"
    uri = f"{WORLDCOVER['url']}ESA_WorldCover_10m_2021_v200_{name}_Map.tif"
    if fetch and not path.is_file():
        with (
            rasterio.Env(
                GDAL_DISABLE_READDIR_ON_OPEN="EMPTY_DIR", GDAL_HTTP_TIMEOUT="20"
            ),
            rasterio.open(uri) as raster,
        ):
            if (
                raster.crs is None
                or raster.crs.to_epsg() != 4326
                or not np.allclose(
                    raster.res, (1 / CELLS_PER_DEGREE,) * 2, rtol=0, atol=1e-12
                )
            ):
                raise ValueError(f"unexpected WorldCover grid {uri}")
            column = round((west - raster.bounds.left) * CELLS_PER_DEGREE)
            row = round((raster.bounds.top - north) * CELLS_PER_DEGREE)
            window = Window(column, row, TILE_CELLS, TILE_CELLS)
            values = raster.read(1, window=window)
        if values.shape != (TILE_CELLS, TILE_CELLS):
            raise ValueError(f"incomplete WorldCover window {bounds}")
        write_bytes(path, values.tobytes())
    payload = path.read_bytes()
    return np.frombuffer(payload, dtype="u1").reshape(TILE_CELLS, TILE_CELLS), {
        "url": uri,
        "window": bounds,
        "pixel_sha256": hashlib.sha256(payload).hexdigest(),
    }


def soil_window(bounds, layer, path, fetch):
    west, south, east, north = bounds
    x, y = transform(
        "EPSG:4326", HOMOLOSINE, [west, east] * 2, [south] * 2 + [north] * 2
    )
    lo_x, hi_x = 250 * floor(min(x) / 250) - 250, 250 * ceil(max(x) / 250) + 250
    lo_y, hi_y = 250 * floor(min(y) / 250) - 250, 250 * ceil(max(y) / 250) + 250
    query = [
        ("map", f"/map/{layer}.map"),
        ("SERVICE", "WCS"),
        ("VERSION", "2.0.1"),
        ("REQUEST", "GetCoverage"),
        ("COVERAGEID", f"{layer}_0-5cm_mean"),
        ("FORMAT", "GEOTIFF_INT16"),
        ("SUBSET", f"X({lo_x},{hi_x})"),
        ("SUBSET", f"Y({lo_y},{hi_y})"),
        ("SUBSETTINGCRS", SOIL_CRS),
        ("OUTPUTCRS", SOIL_CRS),
    ]
    url = SOILGRID["url"] + "mapserv?" + urlencode(query)
    payload = cached_bytes(path, url, fetch)
    target = np.full((TILE_CELLS, TILE_CELLS), NO_SOIL, dtype="<i2")
    with rasterio.MemoryFile(payload) as source, source.open() as raster:
        values = raster.read(1, masked=True).filled(NO_SOIL)
        values[values < 0] = NO_SOIL
        reproject(
            values,
            target,
            src_transform=raster.transform,
            src_crs=raster.crs,
            src_nodata=NO_SOIL,
            dst_transform=from_bounds(*bounds, TILE_CELLS, TILE_CELLS),
            dst_crs="EPSG:4326",
            dst_nodata=NO_SOIL,
            resampling=Resampling.nearest,
        )
    return target, {"url": url, "sha256": hashlib.sha256(payload).hexdigest()}


def polygon_layer(features, bounds):
    polygons = [
        (polygon, code)
        for geometry, code in features
        for polygon in polygon_components(geometry)
    ]
    if not polygons:
        return np.zeros((TILE_CELLS, TILE_CELLS), dtype="<u2")
    return rasterize(
        sorted(polygons, key=lambda feature: feature[1], reverse=True),
        out_shape=(TILE_CELLS, TILE_CELLS),
        transform=from_bounds(*bounds, TILE_CELLS, TILE_CELLS),
        dtype="uint16",
    )


def chile_communities(cache, keys, fetch):
    selection = [box(*tile_bounds(key)) for key in keys]
    relevant = [tile for tile in selection if tile.intersects(CHILE_EXTENT)]
    if not relevant:
        return [""], [], {}
    archive = pinned_source(
        cache, "pisos-vegetacionales-2017.zip", CHILE_COMMUNITIES, fetch
    )
    region = shapely.union_all(relevant)
    bounds = transform_bounds("EPSG:4326", CHILE_CRS, *region.bounds)
    polygons = []
    with (
        ZipFile(archive) as source,
        shapefile.Reader(
            shp=io.BytesIO(source.read("PisosVegetacionalesPliscoff2017.shp")),
            shx=io.BytesIO(source.read("PisosVegetacionalesPliscoff2017.shx")),
            dbf=io.BytesIO(source.read("PisosVegetacionalesPliscoff2017.dbf")),
            encoding="latin-1",
        ) as reader,
    ):
        for entry in reader.iterShapeRecords(bbox=bounds):
            code = entry.record["codigo"]
            if not code:
                continue
            projected = transform_geom(
                CHILE_CRS, "EPSG:4326", entry.shape.__geo_interface__
            )
            clipped = shapely.make_valid(shape(projected)).intersection(region)
            if not clipped.is_empty:
                polygons.append((clipped, f"CHL-2017:{code}"))
    codes = ["", *sorted({code for _, code in polygons})]
    indices = {code: index for index, code in enumerate(codes)}
    return (
        codes,
        [(polygon, indices[code]) for polygon, code in polygons],
        {"chile_potential_vegetation": CHILE_COMMUNITIES},
    )


def jotr_observations(cache, keys, fetch):
    window = shapely.union_all(
        [
            tile.intersection(JOTR_EXTENT)
            for key in keys
            if (tile := box(*tile_bounds(key))).intersects(JOTR_EXTENT)
        ]
    )
    if window.is_empty or window.area == 0:
        return [], [], {}
    file = pinned_source(
        cache, "jotr-hidden-valley-2012.geojson", JOTR_OBSERVATIONS, fetch
    )
    document = json.loads(file.read_bytes())
    if document.get("type") != "FeatureCollection" or document.get(
        "exceededTransferLimit"
    ):
        raise ValueError("incomplete Joshua Tree observation window")
    rows = []
    for feature in document["features"]:
        polygon = shapely.make_valid(shape(feature["geometry"])).intersection(window)
        if polygon.is_empty or polygon.area == 0:
            continue
        props = feature["properties"]
        row = (
            f"JOTR-2012:{props['PI']}",
            props["MapUnit"],
            props["HEIGHT"],
            props["DENSITY"],
        )
        rows.append((polygon, row))
    fields = ("map_unit", "label", "height_class", "density_class")
    records = sorted({row for _, row in rows})
    indices = {row: index for index, row in enumerate(records, 1)}
    return (
        [dict(zip(fields, row, strict=True)) for row in records],
        [(polygon, indices[row]) for polygon, row in rows],
        {"jotr_observed_vegetation": JOTR_OBSERVATIONS},
    )


def japan_vector_window(path, tile, window):
    layer = mapbox_vector_tile.decode(
        path.read_bytes(), default_options={"y_coord_down": True}
    )["veg2024"]
    for feature in layer["features"]:
        geometry = shapely.make_valid(shape(feature["geometry"]))
        lon_lat = shapely.transform(
            geometry,
            lambda xy: np.column_stack(tile_geography(xy, layer["extent"], tile)[::-1]),
        )
        clipped = lon_lat.intersection(window)
        if clipped.is_empty or clipped.area == 0:
            continue
        props = feature["properties"]
        year = props["作成年度"]
        yield (
            clipped,
            (
                f"JP-MOE-2024:{props['凡例コード']}",
                props["凡例名"],
                int(year) if year else 0,
            ),
        )


def japan_observations(cache, keys, fetch, offset):
    pins = json.loads(
        Path(__file__).with_name("japan-vegetation-pins.json").read_text()
    )
    windows = [box(*tile_bounds(key)) for key in keys]
    rows, used = [], {}
    for name, digest in pins.items():
        tile = tuple(map(int, name.split("-")))
        lat, lon = tile_geography([[0, 1], [1, 0]], 1, tile)
        footprint = box(lon[0], lat[0], lon[1], lat[1])
        window = shapely.union_all(
            [
                extent.intersection(footprint)
                for extent in windows
                if extent.intersects(footprint)
            ]
        )
        if window.is_empty or window.area == 0:
            continue
        z, x, y = tile
        folder = cache / "jp-veg2024"
        folder.mkdir(exist_ok=True)
        source = {
            "url": JAPAN_OBSERVATIONS["url"].format(z=z, x=x, y=y),
            "sha256": digest,
        }
        path = pinned_source(folder, f"{name}.pbf", source, fetch)
        used[name] = digest
        rows.extend(japan_vector_window(path, tile, window))
    records = sorted({row for _, row in rows})
    indices = {row: index for index, row in enumerate(records, offset + 1)}
    return (
        [
            {
                "map_unit": unit,
                "label": label,
                "height_class": 0,
                "density_class": 0,
                "map_year": year or None,
            }
            for unit, label, year in records
        ],
        [(polygon, indices[row]) for polygon, row in rows],
        {"japan_observed_vegetation": JAPAN_OBSERVATIONS | {"xyz_sha256": used}}
        if used
        else {},
    )


def geopackage_shape(blob):
    if blob[:2] != b"GP":
        raise ValueError("expected a GeoPackage geometry")
    envelope = (blob[3] >> 1) & 7
    if envelope > 4:
        raise ValueError("unsupported GeoPackage envelope")
    header = 8 + (0, 32, 48, 48, 64)[envelope]
    return shapely.from_wkb(blob[header:])


def jotr_geology(cache, keys, fetch):
    region = shapely.union_all(
        [box(*tile_bounds(key)).intersection(JOTR_EXTENT) for key in keys]
    )
    if region.is_empty or region.area == 0:
        return [], [], {}
    archive = pinned_source(cache, "jotr-geology-gpkg.zip", JOTR_GEOLOGY, fetch)
    crs = JOTR_GEOLOGY["crs"]
    west, south, east, north = transform_bounds("EPSG:4326", crs, *region.bounds)
    polygons = []
    with (
        TemporaryDirectory() as directory,
        ZipFile(archive) as package,
    ):
        path = Path(package.extract("jotr_geology.gpkg", directory))
        with closing(sqlite3.connect(f"{path.as_uri()}?mode=ro", uri=True)) as database:
            rows = database.execute(
                """
                select Unit_Symbol, Unit_Name, Major_Lithology
                from JOTR_Geologic_Unit_Information_Table_short_version
                """
            )
            units = {symbol: (name, lithology) for symbol, name, lithology in rows}
            rows = database.execute(
                """
                select Unit_Symbol, Shape from JOTR_Geologic_Units where OBJECTID in (
                    select id from rtree_JOTR_Geologic_Units_Shape
                    where minx < ? and maxx > ? and miny < ? and maxy > ?
                )
                """,
                (east, west, north, south),
            )
            for symbol, blob in rows:
                source = geopackage_shape(blob)
                projected = transform_geom(crs, "EPSG:4326", source.__geo_interface__)
                polygon = shapely.make_valid(shape(projected)).intersection(region)
                if polygon.area > 0:
                    polygons.append((polygon, symbol))
    symbols = sorted({symbol for _, symbol in polygons})
    indices = {symbol: index for index, symbol in enumerate(symbols, 1)}
    records = []
    for symbol in symbols:
        name, lithology = units[symbol]
        records.append(
            {"map_unit": f"JOTR-GRI:{symbol}", "label": name, "lithology": lithology}
        )
    return (
        records,
        [(polygon, indices[symbol]) for polygon, symbol in polygons],
        {"jotr_geology": JOTR_GEOLOGY},
    )


def build_tile(
    key,
    cache,
    output,
    fetch,
    resolve_path,
    botanical,
    communities,
    observations,
    geology,
):
    name = tile_name(key)
    bounds = tile_bounds(key)
    extent = box(*bounds)
    cover, worldcover = worldcover_window(bounds, cache / f"{name}-cover.u8", fetch)
    with shapefile.Reader(resolve_path, encoding="latin-1") as reader:
        region_polygons = []
        for entry in reader.iterShapeRecords(bbox=bounds):
            code = int(entry.record["ECO_ID"])
            if code > 0:
                polygon = shapely.make_valid(shape(entry.shape.__geo_interface__))
                region_polygons.append((polygon.intersection(extent), code))
    botanical_polygons = [
        (polygon.intersection(extent), index)
        for index, polygon in botanical
        if polygon.intersects(extent)
    ]
    layers = {
        "cover": cover,
        "ecoregion": polygon_layer(region_polygons, bounds),
        "botanical": polygon_layer(botanical_polygons, bounds),
    }
    flags = {}
    for layer, field, shapes in (
        ("community", "potential_community", communities),
        ("observation", "observed_vegetation", observations),
        ("geology", "geology", geology),
    ):
        clipped = [
            (polygon.intersection(extent), code)
            for polygon, code in shapes
            if polygon.intersects(extent)
        ]
        if clipped:
            layers[layer] = polygon_layer(clipped, bounds)
            flags[field] = True
    soil = {}
    water_or_ice = np.isin(cover, (70, 80))
    for layer in SOIL_LAYERS:
        values, soil[layer] = soil_window(
            bounds, layer, cache / f"{name}-{layer}.tif", fetch
        )
        layers[layer] = values
    # The WCS can return zero-filled pixels without a declared nodata value.
    no_texture = (layers["sand"] == 0) & (layers["silt"] == 0) & (layers["clay"] == 0)
    for layer in SOIL_LAYERS:
        layers[layer][water_or_ice | no_texture] = NO_SOIL
    stream = io.BytesIO()
    np.savez_compressed(stream, **layers)
    payload = stream.getvalue()
    digest = hashlib.sha256(payload).hexdigest()
    write_bytes(output / f"{name}-{digest}.npz", payload)
    return name, {"sha256": digest, "worldcover": worldcover, "soil": soil, **flags}


def covering_keys(positions):
    return sorted(
        {
            ((x + dx) % (360 * TILES_PER_DEGREE), y + dy)
            for latitude, longitude in positions
            for (x, y), _ in (grid_cell(latitude, longitude),)
            for dx in (-1, 0, 1)
            for dy in (-1, 0, 1)
            if 0 <= y + dy < 180 * TILES_PER_DEGREE
        }
    )


def site_keys(sites):
    vienna = load_catalog()
    return covering_keys(
        (view["latitude"], view["longitude"])
        for site in sites
        for view, _ in site_views(site, vienna)
    )


def pilot_keys():
    return site_keys(load_sites(pilot_only=True).values())


def build_atlas(cache, output, keys, *, fetch, pinned=None):
    cache.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    archive = pinned_source(cache, "Ecoregions2017.zip", RESOLVE, fetch)
    countries = pinned_source(cache, "wgsrpd-level3.geojson", TDWG, fetch)
    extract_shapes(archive, cache)
    resolve_path = cache / "Ecoregions2017.shp"
    with shapefile.Reader(resolve_path, encoding="latin-1") as source:
        ecoregions = {
            str(row["ECO_ID"]): [row["ECO_NAME"], row["BIOME_NUM"], row["REALM"]]
            for row in source.iterRecords()
        }
    features = json.loads(countries.read_text(encoding="utf-8"))["features"]
    codes = ["", *sorted({row["properties"]["LEVEL3_COD"] for row in features})]
    indices = {code: index for index, code in enumerate(codes)}
    botanical = [
        (
            indices[row["properties"]["LEVEL3_COD"]],
            shapely.make_valid(shape(row["geometry"])),
        )
        for row in features
    ]
    community_codes, communities, regional_sources = chile_communities(
        cache, keys, fetch
    )
    observation_records, observations, observation_sources = jotr_observations(
        cache, keys, fetch
    )
    japan_records, japan_shapes, japan_sources = japan_observations(
        cache, keys, fetch, len(observation_records)
    )
    observation_records.extend(japan_records)
    observations.extend(japan_shapes)
    observation_sources.update(japan_sources)
    geology_records, geology, geology_sources = jotr_geology(cache, keys, fetch)
    with ThreadPoolExecutor(max_workers=6) as pool:
        tiles = dict(
            pool.map(
                lambda key: build_tile(
                    key,
                    cache,
                    output,
                    fetch,
                    resolve_path,
                    botanical,
                    communities,
                    observations,
                    geology,
                ),
                keys,
            )
        )
    manifest = {
        "format": "outshine-vegetation-atlas-1",
        "sources": {
            "ecoregions": RESOLVE,
            "worldcover": WORLDCOVER,
            "botanical": TDWG,
            "soil": SOILGRID,
            "species": dict(load_species_catalog().document["source"])
            | {"catalog_sha256": species_catalog_digest()},
            **regional_sources,
            **observation_sources,
            **geology_sources,
        },
        "botanical_regions": codes,
        "potential_communities": community_codes,
        "observations": observation_records,
        "geology": geology_records,
        "shore_substrates": json.loads(
            Path(__file__).with_name("shore-substrates.json").read_text()
        ),
        "ecoregions": ecoregions,
        "tiles": tiles,
    }
    if pinned is not None:
        actual = source_pins(manifest)
        changed = sorted(
            key
            for key in actual.keys() | pinned.keys()
            if actual.get(key) != pinned.get(key)
        )
        if changed:
            raise ValueError(
                f"changed source windows in atlas tiles: {', '.join(changed)}"
            )
    write_bytes(
        output / "atlas.json",
        (
            json.dumps(manifest, sort_keys=True, ensure_ascii=False, indent=2) + "\n"
        ).encode(),
    )
    return manifest


def source_pins(manifest):
    return {
        name: hashlib.sha256(
            json.dumps(
                {"worldcover": tile["worldcover"], "soil": tile["soil"]}, sort_keys=True
            ).encode()
        ).hexdigest()
        for name, tile in manifest["tiles"].items()
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--cache", type=Path, default=ROOT / "build/vegetation-atlas/sources"
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument("--fetch", action="store_true")
    parser.add_argument("--check-pins", type=Path)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument(
        "--at", type=float, nargs=2, action="append", metavar=("LAT", "LON")
    )
    sites = load_sites()
    selection.add_argument("--site", action="append", choices=sites)
    args = parser.parse_args()
    pinned = json.loads(args.check_pins.read_text()) if args.check_pins else None
    if args.site:
        keys = site_keys(sites[site_id] for site_id in dict.fromkeys(args.site))
    elif args.at:
        keys = covering_keys(args.at)
    else:
        keys = pilot_keys()
    output = args.output or ROOT / "build/vegetation-atlas" / (
        "selection" if args.site or args.at else "pilot"
    )
    manifest = build_atlas(args.cache, output, keys, fetch=args.fetch, pinned=pinned)
    print(f"Built {len(manifest['tiles'])} atlas tiles")


if __name__ == "__main__":
    main()

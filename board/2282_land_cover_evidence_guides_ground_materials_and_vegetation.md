Type: feature
State: open
Architecture: ready
Priority: P2
Parent: 2169
Depends: 2211
Area: data, world, generators, render
Tags: land-cover, imagery, materials, vegetation, streaming

# Land-cover evidence guides ground materials and vegetation

## Boundary and source

Outshine once served aerial RGB: `tiles/src/tilesrc.cpp` fetched ArcGIS World
Imagery JPEGs, and `tiles/src/raster.cpp::bake_photo` mosaicked them as albedo.
Commit c198e082d removed that tile server. Today's `DataKind` has only elevation,
vector map and star catalogue; there is no live imagery provider. Restore the
capability as classification evidence, not as a photo draped over generated
geometry or as a resurrection of the server. Keep source code in Git history.

First source: ESA WorldCover 2021 v200, 11 classes at 10 m, independently
validated global overall accuracy 76.7%. Its 2021 date and classification
errors mean it is a weak prior, never current world truth. CC BY 4.0 attribution
travels with any distributed derivative. Do not treat 2020 v100 to 2021 v200
differences as actual change: the algorithms differ. Optional later source:
cloud-screened Sentinel-2 L2A annual/seasonal reflectance and NDVI, only if
an A/B test proves value beyond WorldCover. RGB camera color is not PBR albedo.

## Native contract and precedence

`LandCoverEvidence` is an immutable, georeferenced cell field with class
fractions, no-data fraction, source identity, epoch, scale and quality; it does
not contain tree instances, materials or OSM geometry. Its COG import adapter
ends at this boundary. `src/world/data` owns source selection, bytes and
decoding; `src/world/ground` combines evidence; `src/generators/flora` consumes
the resulting suitability field. Renderer sees only native geometry/materials.

Use the declared SourceProvider/SourceSet/ContentStore and source revision from
2211; no hardcoded imagery URL or frame-path GeoTIFF processing. A preparation
tool may use GDAL outside the engine to turn pinned WorldCover COG subsets into
compact, checksummed z14/32x32 evidence tiles. Each cell stores area-weighted
fractions rather than one majority label; z14 gives about 76 m cells at the
equator and about 50 m near Switzerland. Measure different tile scales before
choosing the shipped pyramid. Runtime loads bounded tiles asynchronously and
falls back to the current deterministic OSM/DEM rules on absence or timeout.

Explicit OSM roads, rails, buildings, paved areas and water mask incompatible
ground/vegetation outcomes. DEM slope, elevation, exposure and hydrology bound
plausible material and vegetation candidates. Evidence weights unresolved
areas: tree cover raises woody-placement density, grassland raises herbaceous
coverage, cropland does not become permanent forest, built-up does not become
bare rock. OSM landuse disagreements remain visible diagnostics, not silent
override by either source. Seeded placement below cell scale supplies form,
while source fractions only control density/class mixture. Date and weather
affect current surface appearance, not the historical land-cover observation.

## Executable sequence and falsification

1. Pin a small WorldCover 2021 subset for an urban block, a road/woodland
   boundary and alpine bare ground. Add source import, compact tile encoding,
   strict decode and exact reprojection/area tests before a live consumer.
   Start in `src/world/data` plus `test/outshine/src/world/data`.
2. Add the native evidence field and OSM/DEM precedence in `src/world/ground`.
   Assert that crossing tile/antimeridian boundaries preserves class fractions
   and that no-data, corrupt tile, wrong revision and missing cache never turn
   into confident vegetation. Test negative controls by swapping class codes
   and shifting a tile one cell; they must fail. Run `make suite
   SUITE=outshine/src/world/data/LandCoverEvidence` and `make suite
   SUITE=outshine/src/world/ground/LandCoverEvidence`, then `make format` and
   `LINT_JOBS=2 make lint`.
3. Feed only ground material weights (2171) and vegetation suitability (2111).
   Compare hint on/off in Wien, Hockenheim and Koerbersee with outshine-client
   PNGs opened visually. Road, water and building masks must remain clear;
   unclassified terrain keeps a plausible fallback. Record classification
   agreement against withheld map cells, boundary error in metres, tile bytes,
   IO/CPU p95/p99 and memory. A prettier single screenshot is not acceptance.

Sources: https://esa-worldcover.org/en/about/about ;
https://esa-worldcover.org/en/data-access ;
https://sentiwiki.copernicus.eu/web/s2-processing .

# Shared Python Place inputs

Contract: [#216](https://github.com/Weichwolf/outshine/issues/216). Extracted from
the published Buildings Place experiment in PR #211, with independent camera,
height and wire-format controls. The library contains no fixed Place list,
provider URL, renderer, geometry recipe or import from an experiment.

```sh
.venv/bin/python -m pip install -e './lib[places]'
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:lib/tests \
  .venv/bin/python -W error -m unittest test_places
```

```python
from outshine_ref.places import Elevation, acquire, decode_vector, read_camera

camera = read_camera(scenario_path)
receipt = acquire(("elevation", (z, x, y), explicit_url), temporary_root, fetch=True)
elevation = Elevation([receipt])
ground, actual_zoom = elevation.sample_with_zoom(camera.latitude_deg, camera.longitude_deg)
eye = camera.eye(float(ground))
```

`read_camera(path)` retains native geographic station/origin, height declaration,
bearing/pitch, vertical FOV, aspect, sight range, fixed clock and source SHA256.
`local(latitude, longitude, height)` broadcasts arrays to ENU metres at the
declared WGS84 origin at ellipsoid height zero. ECEF curvature is retained.
Input height numbers are unchanged; no geoid/MSL conversion is claimed.
`eye(ground_height_m)` requires an actual finite scalar DEM for relative height;
absolute height needs none. `basis` has right/up/back columns for Blender;
positive pitch raises the view. `project` returns top-left pixel coordinates
and depth; points behind the camera receive NaN pixels. Profile changes must
retain the native aspect. This reader accepts one geographic view and a fixed
timezone-bearing clock, rather than arbitrary native scenario features.

`covering_tiles(lat, lon, radius_m, zoom, max_tiles=4096)` conservatively bounds
a geographic window before allocating tiles. Latitude uses the minimum WGS84
meridional radius; longitude uses a lower bound on the parallel radius over
the entire latitude window. Longitude wraps; source coverage clips at the
Web-Mercator limit. This is a request window, not a visibility/LOD solver.
Buffered MVT polygons can exceed nominal tile bounds: consumers must request
appropriate DEM neighbours/parents and retain that buffered geometry.

`acquire((kind, (z,x,y), url), root, fetch=False)` accepts `vector` or `elevation`,
an explicit data root and an explicit URI. Existing nonempty bounded bytes are
reused even with `fetch=True`; changing sources requires a deliberate new
snapshot/root. Receipts retain URL, tile, file, byte count and SHA256. Only
elevation HTTP404 becomes an explicit `missing` receipt; other HTTP errors,
empty/oversized payloads and absence without fetch raise. Default payload cap
is 16 MiB, timeout 60 s; curl must be installed. First-writer retention does
not overwrite differing bytes. This is offline acquisition, with no frame IO,
background scheduler, automatic source refresh or native asset-cache claim.

`receipt_bytes` checks payload size and pinned SHA before decoding.
`decode_vector(receipt)` lazily imports the `places` extra and retains **every**
MVT layer, feature ID, property and geometry, in tile coordinates with Y down.
`tile_geography(points, extent, tile)` converts Nx2 coordinates, including
the MVT buffer outside its nominal extent, to latitude and canonical longitude.
Source semantics and choice of layers stay with each consumer.

`decode_terrarium` requires one complete opaque, nonanimated, lossless WebP,
2..1024 pixels per side and heights in the native Earth domain −11500..9500 m.
It decodes `R*256 + G + B/256 - 32768` into owned immutable Float64 metres.
`Elevation(receipts).sample[_with_zoom](lat, lon)` uses the finest actually
declared available tile at each coordinate, falling back to declared parents.
Bilinear coordinates span `width-1`/`height-1`, matching the source experiment.
Arrays broadcast without flattening the public shape. Missing coverage and
changed source hashes raise; neither becomes zero or a fabricated zoom.
Raster decode is lazy and retained per instance, so reuse one instance across
queries. No interpolation-error, ground-contact, forest-LOD, frame or device
budget is inferred from these source procedures.

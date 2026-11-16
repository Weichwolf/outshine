"""#1961: does mapped Körbersee forest fill the missing midground?

PYTHONPATH=lib/src .venv/bin/python test/experiments/vegetation_tiles/compare.py
--cache ~/.local/share/outshine/outshine/sources --output /tmp/outshine-research-...
Existing sources only. Diagnostic maps, no vegetation renderer or FPS claim.
"""

import argparse
import json
import math
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

import mapbox_vector_tile
import numpy as np
from PIL import Image, ImageDraw
from scipy.ndimage import map_coordinates
import shapely
from shapely import affinity
from shapely.geometry import box, shape

from outshine_ref.places.height import decode_terrarium
from outshine_ref.places.native_cache import cached_tile
from outshine_ref.places.sources import covering_tiles
from outshine_ref.vegetation.density_tile import canopy_inputs, pack_density, spruce_density


REVISION = '20261025_080012_pt'


def layer_shapes(layers, name, side):
    layer = layers.get(name, {'features': [], 'extent': 4096})
    for feature in layer['features']:
        geometry = affinity.affine_transform(shape(feature['geometry']),
                                             [side / layer['extent'], 0, 0,
                                              -side / layer['extent'], 0, side])
        yield shapely.make_valid(geometry), feature['properties']


def source_fields(cache, tile, side, cells):
    vector, vector_receipt = cached_tile(cache, tile, revision=REVISION)
    elevation, dem_receipt = cached_tile(cache, tile)
    layers = mapbox_vector_tile.decode(vector, default_options={'y_coord_down': True})
    bounds = box(0, 0, side, side)
    wood = shapely.union_all([g for g, p in layer_shapes(layers, 'landcover', side)
                             if p.get('class') == 'wood']).intersection(bounds)
    water = shapely.union_all([g for g, _ in layer_shapes(layers, 'water', side)])
    buildings = [g for g, _ in layer_shapes(layers, 'building', side)]
    roads = []
    for geometry, properties in layer_shapes(layers, 'transportation', side):
        kind = properties.get('class')
        if kind == 'aerialway' or properties.get('brunnel') in ('bridge', 'tunnel'):
            continue
        width = {'primary': 8, 'minor': 6, 'service': 4, 'track': 4, 'path': 2}.get(kind)
        if width is None:
            raise ValueError(f'ground transport reservation width required: {kind}')
        roads.append(geometry.buffer(width / 2))
    blocked = shapely.union_all([water, *buildings, *roads]).intersection(bounds)
    dem = decode_terrarium(elevation)
    uv = np.linspace(0, 1, cells + 1)
    xx, yy = np.meshgrid(uv, uv)
    ground = map_coordinates(dem, [(1 - yy) * dem.shape[0] - .5,
                                   xx * dem.shape[1] - .5], order=1, mode='nearest')
    return wood, blocked, water.intersection(bounds), ground, [vector_receipt, dem_receipt]


def view_counts(field, tile, side, camera, origin, fov, bearing):
    cells = field.density.shape[0]
    yy, xx = np.meshgrid(np.arange(cells) + .5, np.arange(cells) + .5, indexing='ij')
    xx = (xx / cells + tile[1] - origin[0]) * side - camera[0]
    yy = (yy / cells + origin[1] - tile[2]) * side - camera[1]
    distance = np.hypot(xx, yy)
    angle = np.degrees(np.arctan2(xx, yy))
    in_view = np.abs((angle - bearing + 180) % 360 - 180) <= fov / 2
    stems = field.density * (field.bounds[2] / cells) ** 2
    return {label: float(stems[in_view & (distance >= near) & (distance < far)].sum())
            for label, near, far in [('mesh_0_100m', 0, 100), ('impostor_100_2000m', 100, 2000)]}


def diagnostic(records, side, origin, camera, fov, bearing, output):
    panel = 660
    lower_x, upper_x = min(r['tile'][1] for r in records), max(r['tile'][1] for r in records)
    lower_y, upper_y = min(r['tile'][2] for r in records), max(r['tile'][2] for r in records)
    nx, ny = upper_x - lower_x + 1, upper_y - lower_y + 1
    image = Image.new('RGB', (panel * 3, panel + 80), 'white')
    draw = ImageDraw.Draw(image)
    for index, label in enumerate(('Mapped forest / water / reservations',
                                   'Spruce density: black 0 / bright 0.033 stems/m2',
                                   'Canopy coverage: black 0 / bright 1')):
        draw.text((index * panel + 10, 10), label, fill='black')
    for record in records:
        field, water, blocked = record['field'], record['water'], record['blocked']
        cells = field.density.shape[0]
        yy, xx = np.meshgrid((np.arange(cells) + .5) * field.bounds[2] / cells,
                             (np.arange(cells) + .5) * field.bounds[2] / cells, indexing='ij')
        colours = np.full((cells, cells, 3), (174, 177, 146), np.uint8)
        colours[field.forest_fraction > 0] = (27, 83, 42)
        colours[shapely.contains_xy(blocked, xx, yy)] = (92, 89, 82)
        colours[shapely.contains_xy(water, xx, yy)] = (59, 132, 193)
        density = field.density / .033
        coverage = record['canopy']['coverage']
        arrays = [colours, np.stack((density * 245, density * 210, density * 65), axis=-1),
                  np.stack((coverage * 70, coverage * 255, coverage * 105), axis=-1)]
        px = round((record['tile'][1] - lower_x) * panel / nx)
        py = round((record['tile'][2] - lower_y) * panel / ny) + 40
        size = (round(panel / nx), round(panel / ny))
        for index, array in enumerate(arrays):
            image.paste(Image.fromarray(np.flipud(array).astype(np.uint8)).resize(size),
                        (index * panel + px, py))
    cx = (camera[0] / side + origin[0] - lower_x) * panel / nx
    cy = (origin[1] - camera[1] / side - lower_y + 1) * panel / ny + 40
    for index in range(3):
        centre = (index * panel + cx, cy)
        draw.ellipse((centre[0] - 4, cy - 4, centre[0] + 4, cy + 4), fill='red')
        for angle in (bearing - fov / 2, bearing + fov / 2):
            end = (centre[0] + 2000 * math.sin(math.radians(angle)) / side * panel / nx,
                   cy - 2000 * math.cos(math.radians(angle)) / side * panel / ny)
            draw.line((centre, end), fill='red', width=2)
    draw.text((10, panel + 50), 'North up. Red: scenario camera and horizontal FOV to 2 km. '
              'Top-down diagnostic; no occlusion or native render.', fill='black')
    image.save(output / 'forest-density-canopy.png')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    world = json.loads(Path('data/world/vegetation.json').read_text())
    species = json.loads(Path('data/world/species/spruce.json').read_text())
    materials = json.loads(Path('data/world/ground-materials.json').read_text())
    density_max = next(t for t in world['templates'] if t['name'] == 'conifer_forest')['trees']['perM2']
    slope_limit = next(t for t in materials['classes'] if t['name'] == 'forest_floor')['slope']['plausibleDeg'][1]
    scenario = ET.parse('test/outshine/data/places/Koerbersee.scenario').getroot()
    at, view, render = scenario.find('.//at'), scenario.find('.//view'), scenario.find('render')
    latitude, longitude = float(at.get('lat')), float(at.get('lon'))
    tiles = covering_tiles(latitude, longitude, 2000, 14)
    origin = (tiles[0][1], tiles[0][2])
    side = 2 * math.pi * 6378137 * math.cos(math.radians(latitude)) / 2**14
    camera = (((longitude + 180) / 360 * 2**14 - origin[0]) * side,
              (origin[1] + 1 - (1 - math.asinh(math.tan(math.radians(latitude))) / math.pi) / 2 * 2**14) * side)
    fov = math.degrees(2 * math.atan(math.tan(math.radians(float(view.get('fovDeg'))) / 2)
                                    * int(render.get('widthPx')) / int(render.get('heightPx'))))
    records, statistics = [], []
    for tile in tiles:
        tile_lat = math.atan(math.sinh(math.pi * (1 - 2 * (tile[2] + .5) / 2**tile[0])))
        tile_side = 2 * math.pi * 6378137 * math.cos(tile_lat) / 2**tile[0]
        cells = math.ceil(tile_side / 8)
        wood, blocked, water, ground, receipts = source_fields(args.cache, tile, tile_side, cells)
        bounds = (0, 0, tile_side, tile_side)
        field = spruce_density(wood, blocked, ground, bounds, math.degrees(tile_lat), per_m2=density_max,
                               height_m=species['height_m'], alpine_limit=world['alpineLimit'],
                               slope_limit_deg=slope_limit)
        canopy = canopy_inputs(field, ground, crown_width_m=species['spread_m'],
                                leaf_srgb=[species[f'leaf_{c}'] for c in 'rgb'])
        packed = pack_density(field, density_max=density_max, species_height_m=species['height_m'])
        filename = '-'.join(map(str, tile)) + '.npz'
        np.savez(args.output / filename, density_r32_uint=packed.astype('<u4'),
                 ground_asl_m=ground.astype('<f4'), **canopy)
        statistics.append({'tile': tile, 'side_m': tile_side, 'cells': cells,
                           'cell_spacing_m': tile_side / cells, 'wood_m2': wood.area,
                           'available_wood_m2': wood.difference(blocked).area,
                           'expected_stems_whole_tile': float(field.density.sum() * (tile_side / cells) ** 2),
                           'view_expected_stems_no_occlusion': view_counts(
                               field, tile, side, camera, origin, fov, float(at.get('bearingDeg'))),
                           'asset_bytes': (args.output / filename).stat().st_size,
                           'sources': receipts})
        records.append({'tile': tile, 'field': field, 'water': water,
                        'blocked': blocked, 'canopy': canopy})
    diagnostic(records, side, origin, camera, fov, float(at.get('bearingDeg')), args.output)
    report = {'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
              'recipe': 'spruce-mapped-forest-v1', 'vector_revision': REVISION,
              'diagnostic_reference_side_m': side,
              'packing': {'format': 'R32_UINT', 'bits_0_15': 'density UNORM * density_max_m2',
                          'bits_16_23': 'species: 0 empty, 1 spruce',
                          'bits_24_31': 'height UNORM * species_height_m',
                          'rows': 'south to north', 'density_max_m2': density_max,
                          'species_height_m': species['height_m']},
              'camera': {'latitude': latitude, 'longitude': longitude, 'horizontal_fov_deg': fov,
                         'bearing_deg': float(at.get('bearingDeg'))},
              'limits': ['All mapped wood provisionally spruce, no inferred shore groups.',
                         'No treeline jitter; base altitude/latitude and slope ramps only.',
                         'Road full widths primary 8m, minor 6m, service/track 4m, path 2m are provisional reservations.',
                         'Counts integrate density at cell centres: no terrain occlusion, not draw counts.',
                         'Native instance sites must respect predecessor reservations at partial cells.',
                         'Canopy arrays are material-bake inputs, not a shaded native appearance.'],
              'tiles': statistics}
    (args.output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    totals = {k: sum(t['view_expected_stems_no_occlusion'][k] for t in statistics)
              for k in ('mesh_0_100m', 'impostor_100_2000m')}
    print(json.dumps({'cells': cells, 'tiles': len(tiles), 'view_expected_stems_no_occlusion': totals,
                      'wood_m2': sum(t['wood_m2'] for t in statistics)}, indent=2))


if __name__ == '__main__':
    main()

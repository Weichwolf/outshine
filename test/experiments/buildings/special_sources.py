"""Explicit sampling of the actual preferred provider; names never classify geometry."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess

import mapbox_vector_tile
import numpy as np
import shapely

from samples import GIRTH_M, SOURCES


SITES = (
    ('Spittelau', 48.2343, 16.3588, 400., 'calibration'),
    ('Donauturm', 48.2400, 16.4099, 220., 'calibration'),
    ('BerlinFernsehturm', 52.5208, 13.4094, 180., 'calibration'),
    ('Drax', 53.7350, -.9920, 850., 'calibration'),
    ('Neurath', 51.0340, 6.6150, 850., 'calibration'),
    ('Effelsberg', 50.5248, 6.8836, 220., 'calibration'),
    ('Brocken', 51.7992, 10.6153, 300., 'validation'),
    ('TokyoSkytree', 35.7101, 139.8107, 220., 'validation'),
    ('Hameln', 52.1030, 9.3560, 1100., 'validation'),
    ('Emmerthal', 52.0460, 9.3900, 1100., 'validation'),
    ('Grohnde', 52.0350, 9.4110, 1100., 'validation'),
)
ROOT = Path(__file__).resolve().parents[3]
INVENTORY = ROOT/'doc/references/data/osm/special-structures-20261014.json'


def endpoint():
    path = ROOT/'src/generators/osm/providers/PreferredVectorSource.h'
    value = re.search(r'\.Endpoint\s*=\s*"([^"]+)"', path.read_text())
    if value is None:
        raise ValueError('Preferred provider endpoint missing')
    return value[1], str(path.relative_to(ROOT)), hashlib.sha256(path.read_bytes()).hexdigest()


def tiles(latitude, longitude, radius, zoom=14):
    scale = math.cos(math.radians(latitude))
    x = longitude/360*GIRTH_M
    y = math.asinh(math.tan(math.radians(latitude)))*GIRTH_M/(2*math.pi)
    span = GIRTH_M/(1 << zoom)
    lo = np.floor([(x-radius/scale+GIRTH_M/2)/span, (GIRTH_M/2-y-radius/scale)/span]).astype(int)
    hi = np.floor([(x+radius/scale+GIRTH_M/2)/span, (GIRTH_M/2-y+radius/scale)/span]).astype(int)
    return [(zoom, tx, ty) for tx in range(lo[0], hi[0]+1) for ty in range(lo[1], hi[1]+1)]


def acquire():
    template, source, digest = endpoint()
    receipts = {}
    SOURCES.mkdir(parents=True, exist_ok=True)
    for name, lat, lon, radius, split in SITES:
        for tile in tiles(lat, lon, radius):
            if tile not in receipts:
                url = template.format(z=tile[0], x=tile[1], y=tile[2])
                path = SOURCES/(hashlib.sha256(url.encode()).hexdigest()+'.pbf')
                if path.exists():
                    data = path.read_bytes()
                    origin = 'existing-source-cache'
                else:
                    data = subprocess.check_output(['curl', '--fail', '--silent', '--show-error',
                        '--location', '--max-time', '40', '--max-filesize', str(16*1024*1024), url])
                    mapbox_vector_tile.decode(data)
                    path.write_bytes(data)
                    origin = 'network'
                receipts[tile] = dict(tile=list(tile), url=url, sha256=hashlib.sha256(data).hexdigest(),
                    bytes=len(data), origin=origin, sites=[])
            receipts[tile]['sites'].append(name)
    result = dict(provider='OpenFreeMap/OpenMapTiles preferred by Outshine', provider_definition=source,
        provider_definition_sha256=digest, endpoint=template, zoom=14,
        sites=[dict(name=n, latitude=a, longitude=b, radius_m=r, split=s) for n,a,b,r,s in SITES],
        receipts=[receipts[t] for t in sorted(receipts)],
        scope='Named locations select study windows only; no name/location classification input.')
    INVENTORY.write_text(json.dumps(result, indent=2)+'\n')
    return result


def read_site(site):
    inventory = json.loads(INVENTORY.read_text())
    selected = next(s for s in inventory['sites'] if s['name']==site)
    lat, lon = selected['latitude'], selected['longitude']
    scale = math.cos(math.radians(lat))
    anchor = np.array([lon/360*GIRTH_M, math.asinh(math.tan(math.radians(lat)))*GIRTH_M/(2*math.pi)])
    rows, rejected = [], []
    for receipt in inventory['receipts']:
        if site not in receipt['sites']:
            continue
        path = SOURCES/(hashlib.sha256(receipt['url'].encode()).hexdigest()+'.pbf')
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest()!=receipt['sha256']:
            raise ValueError('Pinned special-structure source changed')
        z, tx, ty = receipt['tile']
        span = GIRTH_M/(1 << z)
        layers = mapbox_vector_tile.decode(data, default_options={'y_coord_down':True})
        for layer_name in ('building','poi','landuse','landcover','transportation'):
            layer = layers.get(layer_name, {})
            extent = layer.get('extent', 4096)
            def project(xy):
                return ((xy/extent+[tx,ty])*[span,-span]+[-GIRTH_M/2,GIRTH_M/2]-anchor)*scale
            for ordinal, feature in enumerate(layer.get('features', [])):
                source_id = f'{layer_name}:{feature.get("id", "unknown")}'
                geo = shapely.from_geojson(json.dumps(feature['geometry']))
                pieces = shapely.get_parts(geo) if layer_name=='building' else (geo,)
                for part_index, piece in enumerate(pieces):
                    xy = shapely.get_coordinates(piece)
                    boundary = bool(np.any(xy<=0) or np.any(xy>=extent))
                    geometry = shapely.transform(piece, project)
                    if geometry.distance(shapely.Point(0,0))>selected['radius_m']:
                        continue
                    if not geometry.is_valid:
                        rejected.append(dict(id=source_id, reason=shapely.is_valid_reason(geometry)))
                        continue
                    rows.append(dict(id=f'{source_id}/{tx}/{ty}/{ordinal}/{part_index}', source_id=source_id,
                        layer=layer_name, geometry=geometry, properties=feature['properties'].copy(),
                        source_sha256=receipt['sha256'], tile=receipt['tile'], tile_boundary=boundary,
                        coordinate_step_m=span/extent*scale))
    return rows, dict(site=selected, rejected=rejected,
        projection='Mercator scaled at site latitude; ENU metres; no terrain.')


if __name__=='__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fetch', action='store_true')
    args = parser.parse_args()
    if args.fetch:
        result = acquire()
        print(json.dumps(dict(tiles=len(result['receipts']), bytes=sum(r['bytes'] for r in result['receipts']))))
    for site in SITES:
        rows, report = read_site(site[0])
        poi = [dict(properties=r['properties'], xy=list(r['geometry'].coords)[0]) for r in rows
               if r['layer']=='poi' and r['geometry'].geom_type=='Point' and r['geometry'].distance(shapely.Point(0,0))<220]
        counts = {name:sum(r['layer']==name for r in rows) for name in ('building','poi','landuse','transportation')}
        print(json.dumps(dict(site=site[0], counts=counts, nearby_poi=poi[:15]), ensure_ascii=False))

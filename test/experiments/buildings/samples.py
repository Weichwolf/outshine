"""Pinned real OSM inputs for offline building experiments; acquisition is explicit."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess

import mapbox_vector_tile
import numpy as np
import shapely
from provider_contract import hidden

INVENTORY = Path(__file__).resolve().parents[3]/'doc/references/data/osm/openfreemap-inventory-20261009.json'
SOURCES = Path.home()/'.local/share/outshine/outshine/sources/building-experiments/openfreemap'
GIRTH_M = 40075016.68557849


def centre_receipt(inventory, site):
    rows = sorted((r for r in inventory['receipts'] if site in r['sites'] and r['tile'][0] == 14),
                  key=lambda r: r['tile'])
    if not rows:
        raise ValueError(f'No pinned tile for {site}')
    return rows[len(rows)//2]


def source_bytes(receipt, root=SOURCES, fetch=False):
    path = root/(hashlib.sha256(receipt['url'].encode()).hexdigest()+'.pbf')
    if path.exists():
        data = path.read_bytes()
    elif fetch:
        data = subprocess.check_output(['curl', '--fail', '--silent', '--show-error', '--location',
                                        '--max-time', '40', receipt['url']])
    else:
        raise FileNotFoundError(f'{path}; acquire explicitly with test/experiments/buildings/samples.py --fetch')
    if hashlib.sha256(data).hexdigest() != receipt['sha256']:
        raise ValueError(f'Pinned source changed: {receipt["url"]}')
    if not path.exists():
        root.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return data


def read_site(site, root=SOURCES, context=False, indexed=True):
    inventory = json.loads(INVENTORY.read_text())
    receipt = centre_receipt(inventory, site)
    _, latitude, longitude = next(s for s in inventory['sites'] if s[0] == site)
    scale = math.cos(math.radians(latitude))
    anchor = np.array([longitude/360*GIRTH_M,
                       math.asinh(math.tan(math.radians(latitude)))*GIRTH_M/(2*math.pi)])
    z, tx, ty = receipt['tile']
    span = GIRTH_M/(1 << z)
    layers = mapbox_vector_tile.decode(source_bytes(receipt, root), default_options={'y_coord_down': True})
    layer = layers.get('building', {})
    extent = layer.get('extent', 4096)
    samples, rejected, omitted = [], [], []
    for feature in layer.get('features', []):
        geometry, properties = feature['geometry'], feature['properties']
        if geometry['type'] not in ('Polygon', 'MultiPolygon'):
            rejected.append(dict(id=feature.get('id'), reason='unsupported_geometry'))
            continue
        polygons = geometry['coordinates'] if geometry['type'] == 'MultiPolygon' else [geometry['coordinates']]
        for ordinal, rings in enumerate(polygons):
            key = f'{site}:{feature.get("id", "unknown")}:{ordinal}'
            try:
                suppress = hidden(properties)
            except ValueError as error:
                rejected.append(dict(id=key, reason=str(error)))
                continue
            if suppress:
                omitted.append(dict(id=key, reason='provider_hide_3d', properties=properties.copy()))
                continue
            metres = [((np.asarray(r, dtype=float)/extent+[tx, ty])*[span, -span]
                       +[-GIRTH_M/2, GIRTH_M/2]-anchor)*scale for r in rings]
            polygon = shapely.Polygon(metres[0], metres[1:])
            if not polygon.is_valid or polygon.area <= 1:
                rejected.append(dict(id=key, reason='invalid_or_degenerate_footprint',
                                     detail=shapely.is_valid_reason(polygon)))
                continue
            samples.append(dict(id=key, polygon=polygon, properties=properties, source_sha256=receipt['sha256']))
    report = dict(site=site, receipt=receipt, origin_lat_lon=[latitude, longitude],
                         source_polygons=len(samples)+len(rejected)+len(omitted), rejected=rejected, omitted=omitted,
                         projection='Mercator scaled at the site latitude; no terrain in this experiment')
    if context:
        from context import attach
        def project(xy, size):
            return ((xy/size+[tx, ty])*[span, -span]+[-GIRTH_M/2, GIRTH_M/2]-anchor)*scale
        report['context'] = attach(samples, layers, project, receipt['sha256'], indexed)
    return samples, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sites', nargs='+', default=['Vienna', 'Tokyo', 'CentralPark'])
    parser.add_argument('--sources', type=Path, default=SOURCES)
    parser.add_argument('--fetch', action='store_true')
    args = parser.parse_args()
    inventory = json.loads(INVENTORY.read_text())
    for site in args.sites:
        receipt = centre_receipt(inventory, site)
        source_bytes(receipt, args.sources, args.fetch)
        samples, report = read_site(site, args.sources)
        print(json.dumps(dict(site=site, polygons=len(samples), rejected=len(report['rejected']),
                             source_sha256=receipt['sha256'])), flush=True)


if __name__ == '__main__':
    main()

"""Compare the provider's API and fresh MVT payloads with CGA input capabilities."""

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

import mapbox_vector_tile

from evidence import revision
from context_catalog import LAYER_ROLES, field_role, poi_role
from provider_contract import FIELDS
from samples import INVENTORY, SOURCES, centre_receipt

RESEARCH = SOURCES/'research'
MISSING = ('height', 'building:levels', 'building:min_level', 'building', 'building:part', 'man_made',
           'roof:shape', 'roof:height', 'roof:direction', 'roof:colour', 'building:material', 'roof:material')
REFERENCES = (
    'https://tiles.openfreemap.org/planet',
    'https://github.com/openmaptiles/openmaptiles/blob/master/layers/building/building.yaml',
    'https://esri.github.io/cityengine-sdk/html/classprt_1_1_initial_shape_builder.html',
    'https://esri.github.io/cityengine-sdk/html/cgaref/cgareference/op_extrude.html',
    'https://esri.github.io/cityengine-sdk/html/cgaref/cgareference/op_color.html',
)


def obtain(url, path, fetch):
    if path.is_file():
        return path.read_bytes(), 'retained'
    if not fetch:
        raise FileNotFoundError(f'{path}; acquisition requires --fetch')
    data = subprocess.check_output(['curl', '--fail', '--silent', '--show-error', '--location',
                                    '--max-time', '40', '--max-filesize', '4194304', url])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return data, 'network'


def inspect_tile(site, inventory, template, fetch):
    coordinate = centre_receipt(inventory, site)['tile']
    url = template.format(z=coordinate[0], x=coordinate[1], y=coordinate[2])
    path = RESEARCH/(hashlib.sha256(url.encode()).hexdigest()+'.pbf')
    payload, origin = obtain(url, path, fetch)
    layers = mapbox_vector_tile.decode(payload, default_options={'y_coord_down': True})
    building = layers.get('building', {}).get('features', [])
    fields = Counter(key for feature in building for key in feature['properties'])
    types = {key: sorted({type(f['properties'][key]).__name__ for f in building if key in f['properties']})
             for key in fields}
    return dict(site=site, url=url, tile=coordinate, source_path=str(path), origin=origin,
                source_sha256=hashlib.sha256(payload).hexdigest(), bytes=len(payload), building_features=len(building),
                observed_fields=dict(fields), field_types=types,
                hidden_features=sum(f['properties'].get('hide_3d') is True for f in building),
                geometries=dict(Counter(f['geometry']['type'] for f in building)),
                context_layers={key:dict(features=len(layers[key]['features']),
                                         fields=sorted({k for f in layers[key]['features'] for k in f['properties']}))
                                for key in ('poi', 'landuse', 'transportation', 'aeroway') if key in layers})


def compare(tilejson, fetch):
    raw, _ = obtain(REFERENCES[0], tilejson, fetch)
    schema = json.loads(raw)
    inventory_bytes = INVENTORY.read_bytes()
    inventory = json.loads(inventory_bytes)
    fields = next(layer['fields'] for layer in schema['vector_layers'] if layer['id'] == 'building')
    tiles = [inspect_tile(site, inventory, schema['tiles'][0], fetch) for site in ('Vienna', 'Tokyo', 'CentralPark')]
    known = {row['field']: row for row in FIELDS}
    rows = []
    for field, kind in sorted(fields.items()):
        mapping = known.get(field, dict(field=field, target='noch nicht zugeordnet', meaning='Quellwert erhalten.'))
        rows.append(dict(mapping, type=kind, observed_features=sum(t['observed_fields'].get(field, 0) for t in tiles),
                         inventory_features=inventory['layers']['building']['keys'].get(field, 0)))
    observed = {field for tile in tiles for field in tile['observed_fields']}
    layer_recipes = [dict(layer=row['id'], role=LAYER_ROLES.get(row['id'], 'unclassified source metadata'),
                         fields={k:dict(type=t, role=field_role(row['id'], k)) for k, t in row['fields'].items()})
                     for row in schema['vector_layers']]
    poi_classes = inventory['layers']['poi']['values']['class']
    poi_recipes = [dict(value=value, role=poi_role(dict(**{'class':value})), observed_features=count)
                   for value, count in sorted(poi_classes.items())]
    return dict(**revision(), observed_utc=datetime.now(timezone.utc).isoformat(), provider='OpenFreeMap / OpenMapTiles',
                tilejson_path=str(tilejson), tilejson_sha256=hashlib.sha256(raw).hexdigest(), tile_template=schema['tiles'][0],
                api_layers=schema['vector_layers'], layer_recipes=layer_recipes, poi_recipes=poi_recipes,
                building_fields=rows, fresh_tiles=tiles,
                undeclared_observed_fields=sorted(observed-set(fields)), missing_building_fields=[k for k in MISSING if k not in fields],
                previous_inventory=dict(path=str(INVENTORY), sha256=hashlib.sha256(inventory_bytes).hexdigest(),
                                        tiles=inventory['successful_tiles'], features=inventory['layers']['building']['features']),
                rule_parameters='Eigene CGA-Namen; PRT gleicht Attribute nach Name und Typ mit dem CGB ab.',
                context='Alle Layer/Felder erhalten eine Rolle. Flächenüberdeckung liefert Nutzungskontext; '
                        'POIs und Hausnummern benötigen einen eindeutigen Host. POIs behalten ihren Teil-/Geschossumfang. '
                        'streetWidth ist ein Esri-Kantenattribut; der Provider liefert keine konstruktiven Straßenbreiten.',
                scope='Explizite API-Kontrolle; keine Aktualisierung der gepinnten Versuchsquellen oder fertiger Assets.',
                references=REFERENCES)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tilejson', type=Path, default=RESEARCH/'20261011-tilejson.json')
    parser.add_argument('--fetch', action='store_true')
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/provider-comparison.json'))
    args = parser.parse_args()
    result = compare(args.tilejson, args.fetch)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(output=str(args.output), fields=[row['field'] for row in result['building_fields']],
                          tiles=[{k:t[k] for k in ('site', 'building_features', 'hidden_features')} for t in result['fresh_tiles']],
                          unexpected=result['undeclared_observed_fields'])))


if __name__ == '__main__':
    main()

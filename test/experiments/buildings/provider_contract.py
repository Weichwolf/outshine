"""Provider attributes mapped to explicitly named CGA rule parameters, in metres and sRGB."""

import hashlib
import math

import numpy as np
from PIL import ImageColor

from massing import polygons_of

FIELDS = (
    dict(field='render_height', type='Number', target='buildingHeight',
         meaning='Provideraufbereitete Oberkante über Gelände; keine belegte Originalhöhe.'),
    dict(field='render_min_height', type='Number', target='minHeight',
         meaning='Unterkante über Gelände; Körperhöhe = buildingHeight − minHeight.'),
    dict(field='colour', type='String', target='wallColorR/G/B',
         meaning='Wandfarbe in sRGB für CGA color(); Outshine konvertiert nach linear RGB.'),
    dict(field='hide_3d', type='Boolean', target='hide3d',
         meaning='Vor Geometrieaufbau auslassen; Gebäudeumrisse dürfen Parts nicht doppeln.'),
)


def seed_for(identifier):
    return int.from_bytes(hashlib.blake2b(str(identifier).encode(), digest_size=8).digest(), 'little')


def hidden(properties):
    value = properties.get('hide_3d', False)
    if type(value) is bool:
        return value
    if type(value) in (int, float) and value in (0, 1):
        return bool(value)
    if type(value) is str and value in ('true', 'false'):
        return value == 'true'
    raise ValueError('invalid provider hide_3d')


def number(value):
    try:
        result = float(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f'invalid supplied number: {value!r}') from error
    if type(value) is bool or not math.isfinite(result):
        raise ValueError(f'invalid supplied number: {value!r}')
    return result


def srgb(properties, default=(1., 1., 1.)):
    value = next((properties[k] for k in ('building:colour', 'colour', 'color') if k in properties), None)
    if value is None:
        return np.array(default)
    value = str(value)
    if len(value) == 6 and all(c in '0123456789abcdefABCDEF' for c in value):
        value = '#'+value
    colour = ImageColor.getrgb(value)
    if len(colour) == 4 and colour[3] != 255:
        raise ValueError('transparent supplied wall colour')
    return np.array(colour[:3])/255.


def attributes(properties):
    top = number(properties.get('height', properties.get('render_height', 9.)))
    bottom = number(properties.get('min_height', properties.get('render_min_height', 0.)))
    if top <= bottom:
        raise ValueError('invalid height interval')
    colour = srgb(properties)
    return dict(buildingHeight=top, minHeight=bottom, bodyHeight=top-bottom,
                wallColorR=float(colour[0]), wallColorG=float(colour[1]), wallColorB=float(colour[2]),
                hide3d=hidden(properties))


def origin(properties, original, provider):
    return 'supplied' if original in properties else 'provider-derived' if provider in properties else 'procedural'


def initial_shape(sample):
    polygon = sample['polygon']
    if not polygon.is_valid or polygon.area <= 1:
        raise ValueError('invalid building footprint')
    vertices, indices, counts, holes = [], [], [], []
    for rings in polygons_of(polygon):
        outer = len(counts)
        for ring in rings:
            start = len(vertices)
            vertices.extend(np.c_[ring[:, 0], np.zeros(len(ring)), -ring[:, 1]].tolist())
            indices.extend(range(start, start+len(ring)))
            counts.append(len(ring))
        if len(rings) > 1:
            holes.extend([outer, *range(outer+1, len(counts)), 0xffffffff])
    seed = seed_for(sample['id']) & 0xffffffff
    return dict(name=str(sample['id']), random_seed=seed if seed < 0x80000000 else seed-0x100000000,
                geometry=dict(vertices=vertices, face_indices=indices, face_counts=counts, holes=holes),
                attributes=attributes(sample['properties']), source_properties=sample['properties'].copy(),
                coordinates='local metres: (east, north, up) → Esri (east, up, south)',
                recipe='translate minHeight along world.up; extrude(world.up, bodyHeight); wall color(sRGB)',
                execution='PRT-compatible input descriptor; no CGB or Esri runtime execution')

"""Metric repeat plans and exact box-filtered opening coverage, without derivation trees."""

from dataclasses import dataclass
import math

import numpy as np
import shapely
from shell import prepare
from massing import MIN_EDGE_M, polygons_of, stack
from features import describe, prepare as prepare_features
from provider_contract import attributes, hidden, number, origin, seed_for, srgb
from recipes import choose, frontage

REPEAT_DTYPE = np.dtype([('width_m', '<f8'), ('height_m', '<f8'), ('depth_m', '<f8'),
                         ('columns', '<u4'), ('rows', '<u4'), ('opening_width', '<f8'), ('opening_height', '<f8')])

@dataclass(frozen=True)
class Repeat:
    width_m: float
    height_m: float
    columns: int
    rows: int
    opening_width: float
    opening_height: float
    depth_m: float = .16

    def __post_init__(self):
        if (not all(math.isfinite(x) and x > 0 for x in (self.width_m, self.height_m, self.depth_m))
                or type(self.columns) is not int or type(self.rows) is not int
                or min(self.columns, self.rows) < 1 or max(self.columns, self.rows) > np.iinfo(np.uint32).max
                or not all(0 <= x < 1 for x in (self.opening_width, self.opening_height))):
            raise ValueError('invalid facade repeat')

    @property
    def count(self):
        return self.columns*self.rows if self.opening_width*self.opening_height else 0


def source_number(value):
    return number(value)


def author(sample, style=None, massing=None):
    polygon, properties = sample['polygon'], sample['properties']
    if hidden(properties):
        raise ValueError('provider_hide_3d')
    if not polygon.is_valid or polygon.area <= 1:
        raise ValueError('invalid building footprint')
    rings = polygons_of(polygon)[0]
    seed, parameters = seed_for(sample['id']), attributes(properties)
    top, bottom = parameters['buildingHeight'], parameters['minHeight']
    style = style or 'osm'
    massing = massing or ('plain' if style == 'osm' else 'styled')
    if style not in ('osm', 'bauhaus', 'midcentury', 'artdeco'):
        raise ValueError('unknown architectural style')
    if style == 'osm' and massing != 'plain':
        raise ValueError('OSM source recipe requires the supplied footprint')
    roof = properties.get('roof:shape', 'flat')
    if roof != 'flat':
        raise ValueError(f'roof recipe not yet implemented: {roof}')
    context = sample.get('context', dict(version=1, evidence=[]))
    recipe = choose(properties, context, polygon, top, bottom)
    rhythm = recipe['parameters']
    supplied_rows = properties.get('building:levels')
    floor_height = rhythm['floor_m'] if style == 'osm' else 2.9
    levels = source_number(supplied_rows) if supplied_rows is not None else max(1, math.floor((top-bottom)/floor_height+.5))
    if not math.isfinite(levels) or levels < 1 or levels != int(levels):
        raise ValueError('invalid supplied storeys')
    rows = int(levels)
    kind = next((properties[k] for k in ('man_made', 'building:part', 'building', 'subclass', 'class')
                 if k in properties and properties[k] not in ('yes', 'building')), 'unknown')
    closed = kind in ('chimney', 'silo', 'storage_tank', 'water_tower', 'industrial', 'warehouse', 'roof')
    if kind == 'unknown' and top-bottom > 3*math.sqrt(polygon.area):
        closed = True
    if style == 'osm':
        closed = recipe['family'] in ('utility', 'garage', 'unknown')
    blocked = 'closed utility body' if closed else 'supplied building part' if properties.get('building:part') else None
    layers, decision = stack(polygon, bottom, top, rows, style, massing, blocked)
    facades = []
    for index, layer in enumerate(layers):
        for component in layer['polygons']:
            for ring in component:
                for a, b in zip(ring, np.roll(ring, -1, axis=0)):
                    length = float(np.linalg.norm(b-a))
                    if length <= MIN_EDGE_M:
                        raise ValueError('zero length building edge')
                    axis = rhythm['axis_m'] if style == 'osm' else 3.2
                    columns = max(1, int(math.floor(length/axis+.5)))
                    width, height = (rhythm['width'], rhythm['height']) if style == 'osm' else (
                                    (.88, .64) if style == 'midcentury' else (.8, .58))
                    if closed or length < 1.9:
                        width = height = 0.
                    facades.append(dict(a=a, b=b, layer=index, bottom_m=layer['bottom_m'], top_m=layer['top_m'],
                                        normal=np.r_[(b-a)[1], -(b-a)[0], 0.]/length,
                                        repeat=Repeat(length, layer['top_m']-layer['bottom_m'], columns,
                                                      layer['floors'], width, height)))
    product = dict(id=sample['id'], rings=rings, polygon=shapely.Polygon(rings[0], rings[1:]),
                top_m=top, bottom_m=bottom, style=style, seed=seed, facades=facades, layers=layers, massing=decision,
                source_properties=properties.copy(), context=context, recipe=recipe,
                wall_linear=wall_colour(properties, (1., 1., 1.) if style == 'osm' else (.8, .79, .74)),
                provenance=dict(source_sha256=sample.get('source_sha256'),
                                height=origin(properties, 'height', 'render_height'),
                                bottom=origin(properties, 'min_height', 'render_min_height'),
                                footprint='supplied',
                                footprint_coverage=('tile boundary; completeness unproven' if sample.get('tile_boundary') else 'contained in tile')
                                                   if 'tile_boundary' in sample else 'source extent unspecified',
                                wall_colour='supplied' if any(k in properties for k in ('building:colour', 'colour', 'color')) else 'procedural',
                                floors='supplied' if supplied_rows is not None else 'procedural',
                                roof='supplied' if 'roof:shape' in properties else 'procedural',
                                style='source-volume' if style == 'osm' else 'procedural',
                                massing='preserved' if style == 'osm' else 'procedural',
                                openings=recipe['openings'] if style == 'osm' else 'procedural',
                                use=recipe['origin'], kind=kind))
    if style == 'osm':
        for facade in facades:
            facade['frontage_sources'] = frontage(facade, context)
    product['features'] = describe(product)
    product['opening_patterns'] = sorted({(f['repeat'].opening_width, f['repeat'].opening_height)
                                         for f in facades if f['repeat'].count})
    return product


def plan(sample, style=None, massing=None):
    product = author(sample, style, massing)
    product['shell'], product['surfaces'] = prepare(product)
    product['decoration'], product['feature_bounds'] = prepare_features(product)
    return product


def wall_colour(properties, default=(.8, .79, .74)):
    colour = srgb(properties, default)
    return np.where(colour <= .04045, colour/12.92, ((colour+.055)/1.055)**2.4)


def cell_range(repeat, region=None):
    pw, ph = repeat.width_m/repeat.columns, repeat.height_m/repeat.rows
    if region is None:
        x0, y0, x1, y1 = 0, 0, repeat.columns, repeat.rows
    else:
        low, high = np.asarray(region[:2]), np.asarray(region[2:])
        if not np.isfinite(region).all() or np.any(low > high):
            raise ValueError('invalid detail region')
        x0, y0 = np.clip(np.floor(low/[pw, ph]).astype(int), 0, [repeat.columns, repeat.rows])
        x1, y1 = np.clip(np.ceil(high/[pw, ph]).astype(int), 0, [repeat.columns, repeat.rows])
    return int(x0), int(y0), int(x1), int(y1)


def detail_count(repeat, region=None):
    x0, y0, x1, y1 = cell_range(repeat, region)
    return (x1-x0)*(y1-y0) if repeat.count else 0


def opening_rectangles(repeat, region=None):
    if not repeat.count:
        return np.empty((0, 4), dtype=np.float64)
    x0, y0, x1, y1 = cell_range(repeat, region)
    pw, ph = repeat.width_m/repeat.columns, repeat.height_m/repeat.rows
    y, x = np.mgrid[y0:y1, x0:x1]
    centre = np.c_[(x.ravel()+.5)*pw, (y.ravel()+.5)*ph]
    half = np.array([pw*repeat.opening_width, ph*repeat.opening_height])*.5
    return np.c_[centre-half, centre+half]


def expanded_splits(repeat):
    result = []
    if repeat.count:
        pw, ph = repeat.width_m/repeat.columns, repeat.height_m/repeat.rows
        for row in range(repeat.rows):
            for column in range(repeat.columns):
                centre = [(column+.5)*pw, (row+.5)*ph]
                half = [pw*repeat.opening_width*.5, ph*repeat.opening_height*.5]
                result.append([centre[0]-half[0], centre[1]-half[1], centre[0]+half[0], centre[1]+half[1]])
    return np.asarray(result, dtype=np.float64).reshape(-1, 4)


def interval_integral(position, period, count, fraction):
    x = np.clip(np.asarray(position)/period, 0., count)
    complete = np.floor(x)
    partial = np.clip(x-complete-(1-fraction)*.5, 0., fraction)
    return period*(complete*fraction+partial)


def coverage(repeat, centres, footprint):
    centres, footprint = np.asarray(centres, dtype=float), np.asarray(footprint, dtype=float)
    if (centres.shape[-1] != 2 or footprint.shape[-1] != 2 or not np.isfinite(centres).all()
            or not np.isfinite(footprint).all() or np.any(footprint <= 0)):
        raise ValueError('positive finite box footprint required')
    integral = []
    for axis, (extent, count, fraction) in enumerate(((repeat.width_m, repeat.columns, repeat.opening_width),
                                                   (repeat.height_m, repeat.rows, repeat.opening_height))):
        lower, upper = centres[..., axis]-footprint[..., axis]*.5, centres[..., axis]+footprint[..., axis]*.5
        area = interval_integral(upper, extent/count, count, fraction)-interval_integral(lower, extent/count, count, fraction)
        integral.append(area/footprint[..., axis])
    return np.clip(integral[0]*integral[1], 0., 1.)


def pack_repeats(facades):
    result = np.empty(len(facades), dtype=REPEAT_DTYPE)
    for i, facade in enumerate(facades):
        repeat = facade['repeat']
        result[i] = tuple(getattr(repeat, name) for name in REPEAT_DTYPE.names)
    return result

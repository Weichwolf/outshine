"""Metric repeat plans and exact box-filtered opening coverage, without derivation trees."""

from dataclasses import dataclass
import hashlib
import math

import numpy as np
import shapely
from PIL import ImageColor
from shell import prepare

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


def canonical_ring(coordinates, positive):
    points = np.asarray(coordinates, dtype=np.float64)
    if np.array_equal(points[0], points[-1]):
        points = points[:-1]
    area = np.sum(points[:, 0]*np.roll(points[:, 1], -1)-points[:, 1]*np.roll(points[:, 0], -1))
    if (area > 0) != positive:
        points = points[::-1]
    first = min(range(len(points)), key=lambda i: tuple(points[i]))
    return np.roll(points, -first, axis=0)


def source_number(value):
    try:
        return float(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f'invalid supplied number: {value!r}') from error


def plan(sample, style=None):
    polygon, properties = sample['polygon'], sample['properties']
    if not polygon.is_valid or polygon.area <= 1:
        raise ValueError('invalid building footprint')
    rings = [canonical_ring(polygon.exterior.coords, True)]
    rings += sorted((canonical_ring(r.coords, False) for r in polygon.interiors), key=lambda r: tuple(r.flat))
    digest = hashlib.blake2b(str(sample['id']).encode(), digest_size=8).digest()
    seed = int.from_bytes(digest, 'little')
    style = style or ('bauhaus' if seed % 2 == 0 else 'artdeco')
    if style not in ('bauhaus', 'artdeco'):
        raise ValueError('unknown architectural style')
    top = source_number(properties.get('height', properties.get('render_height', 9.)))
    bottom = source_number(properties.get('min_height', properties.get('render_min_height', 0.)))
    if not math.isfinite(top+bottom) or top <= bottom:
        raise ValueError('invalid height interval')
    roof = properties.get('roof:shape', 'flat')
    if roof != 'flat':
        raise ValueError(f'roof recipe not yet implemented: {roof}')
    supplied_rows = properties.get('building:levels')
    levels = source_number(supplied_rows) if supplied_rows is not None else max(1, math.floor((top-bottom)/2.9+.5))
    if not math.isfinite(levels) or levels < 1 or levels != int(levels):
        raise ValueError('invalid supplied storeys')
    rows = int(levels)
    kind = next((properties[k] for k in ('man_made', 'building:part', 'building', 'subclass', 'class')
                 if k in properties and properties[k] not in ('yes', 'building')), 'unknown')
    closed = kind in ('chimney', 'silo', 'storage_tank', 'water_tower', 'industrial', 'warehouse', 'roof')
    if kind == 'unknown' and top-bottom > 3*math.sqrt(polygon.area):
        closed = True
    facades = []
    for ring in rings:
        for a, b in zip(ring, np.roll(ring, -1, axis=0)):
            length = float(np.linalg.norm(b-a))
            if length <= 1e-8:
                raise ValueError('zero length building edge')
            columns = max(1, int(math.floor(length/3.2+.5)))
            width, height = (.8, .58) if style == 'bauhaus' else (.5, .72)
            if closed or length < 1.9:
                width = height = 0.
            facades.append(dict(a=a, b=b, normal=np.r_[(b-a)[1], -(b-a)[0], 0.]/length,
                                repeat=Repeat(length, top-bottom, columns, rows, width, height)))
    product = dict(id=sample['id'], rings=rings, polygon=shapely.Polygon(rings[0], rings[1:]),
                top_m=top, bottom_m=bottom, style=style, seed=seed, facades=facades,
                source_properties=properties.copy(), wall_linear=wall_colour(properties),
                provenance=dict(height='supplied' if 'height' in properties or 'render_height' in properties else 'procedural',
                                bottom='supplied' if 'min_height' in properties or 'render_min_height' in properties else 'procedural',
                                footprint='supplied',
                                wall_colour='supplied' if any(k in properties for k in ('building:colour', 'colour', 'color')) else 'procedural',
                                floors='supplied' if supplied_rows is not None else 'procedural',
                                roof='supplied' if 'roof:shape' in properties else 'procedural',
                                style='procedural', openings='procedural', kind=kind))
    product['shell'] = prepare(product)
    return product


def wall_colour(properties):
    value = next((properties[k] for k in ('building:colour', 'colour', 'color') if k in properties), None)
    if value is None:
        srgb = np.array([.80, .79, .74])
    else:
        value = str(value)
        if len(value) == 6 and all(c in '0123456789abcdefABCDEF' for c in value):
            value = '#'+value
        colour = ImageColor.getrgb(value)
        if len(colour) == 4 and colour[3] != 255:
            raise ValueError('transparent supplied wall colour')
        srgb = np.array(colour[:3])/255.
    return np.where(srgb <= .04045, srgb/12.92, ((srgb+.055)/1.055)**2.4)


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

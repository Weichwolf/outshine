"""Compare exported native terrain pages with their actual prepared source parents."""

import argparse
import csv
import json
from pathlib import Path

import numpy as np


def sample_parent(field, source_tile, page_tile, side):
    source_zoom, source_x, source_y = source_tile
    zoom, x, y = page_tile
    for level, column, row in (source_tile, page_tile):
        if not 0 <= level <= 30 or not 0 <= column < 2**level or not 0 <= row < 2**level:
            raise ValueError('tile coordinates leave the geographic grid')
    drop = zoom - source_zoom
    if drop < 0 or (x >> drop, y >> drop) != (source_x, source_y):
        raise ValueError('source tile is not the geographic parent of the page')
    if field.ndim != 2 or min(field.shape) < 2 or side < 2 or not np.all(np.isfinite(field)):
        raise ValueError('reference requires a two-dimensional field and complete page')
    fraction = np.linspace(0, 1, side)
    px = ((x + fraction) / 2**drop - source_x) * (field.shape[1] - 1)
    py = ((y + fraction) / 2**drop - source_y) * (field.shape[0] - 1)
    if (np.any(px < 0) or np.any(px > field.shape[1] - 1)
            or np.any(py < 0) or np.any(py > field.shape[0] - 1)):
        raise ValueError('page samples leave the declared source field')
    xi = np.minimum(np.floor(px).astype(int), field.shape[1] - 2)
    yi = np.minimum(np.floor(py).astype(int), field.shape[0] - 2)
    fx, fy = px - xi, py - yi
    top = field[yi[:, None], xi] * (1 - fx) + field[yi[:, None], xi + 1] * fx
    bottom = field[yi[:, None] + 1, xi] * (1 - fx) + field[yi[:, None] + 1, xi + 1] * fx
    return top * (1 - fy[:, None]) + bottom * fy[:, None]


def check_reference():
    columns = np.linspace(12, 13, 63)
    rows = np.linspace(18, 19, 35)
    field = 3 * columns[None, :] + 7 * rows[:, None] + 10
    for drop in range(5):
        for child_x, child_y in [(0, 0), (2**drop - 1, 2**drop - 1)]:
            x, y = (12 << drop) + child_x, (18 << drop) + child_y
            actual = sample_parent(field, (6, 12, 18), (6 + drop, x, y), 33)
            fraction = np.linspace(0, 1, 33)
            east, north = (x + fraction) / 2**drop, (y + fraction) / 2**drop
            expected = 3 * east[None, :] + 7 * north[:, None] + 10
            np.testing.assert_allclose(actual, expected, rtol=0, atol=1e-12)
    for wrong in [(6, 12, 17), (6, 11, 18), (8, 48, 72)]:
        try:
            sample_parent(field, wrong, (7, 24, 36), 33)
        except ValueError:
            continue
        raise AssertionError('an unrelated parent was accepted')


def compare(pages_directory, fields_manifest, selected):
    fields = {tuple(row['tile']): row for row in json.loads(fields_manifest.read_text())}
    with (pages_directory / 'pages.csv').open() as source:
        pages = list(csv.DictReader(source))
    results = []
    for index in selected:
        page = pages[index]
        tile = tuple(int(page[key]) for key in ('zoom', 'x', 'y'))
        source_zoom = int(page['source_zoom'])
        drop = tile[0] - source_zoom
        if drop < 0:
            raise ValueError('page declares a finer source than its own extent')
        source_tile = (source_zoom, tile[1] >> drop, tile[2] >> drop)
        source = fields[source_tile]
        field = np.fromfile(source['path'], dtype='<f4')
        field = field.reshape(source['rows'], source['cols']).astype(float)
        side, halo = int(page['side']), int(page['halo'])
        native = np.fromfile(pages_directory / f'{index}.f32', dtype='<f4')
        native = native.reshape(side + 2 * halo, side + 2 * halo)
        native = native[halo:halo + side, halo:halo + side]
        expected = sample_parent(field, source_tile, tile, side)
        delta = native - expected
        results.append(dict(page=index, tile=tile, source_tile=source_tile,
                            source_key=source['key'], minimum_delta_m=float(delta.min()),
                            maximum_delta_m=float(delta.max())))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pages', type=Path)
    parser.add_argument('--fields', type=Path)
    parser.add_argument('--select', type=int, nargs='+', default=[])
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    check_reference()
    if args.pages is None and args.fields is None:
        print('PASS: independent affine fields and unrelated-parent rejection')
        return
    if args.pages is None or args.fields is None or args.output is None or not args.select:
        parser.error('comparison requires --pages, --fields, --select and --output')
    report = dict(scope='Prepared source parent versus final native page, including deformation and seams',
                  results=compare(args.pages, args.fields, args.select))
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()

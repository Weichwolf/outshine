#!/usr/bin/env python3
"""Direct plan aggregation; no fine mesh is built for the far candidate.

Uses building_lod.py dependencies and TSV manifest (cache path, zoom, x, y).
Flat ground/roofs isolate the representation. Output is an experiment, not a place gate.
"""
import argparse
import json
import math
import time
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image

import building_lod as lod


def polygon_area(ring):
    return abs(np.sum(ring[:, 0]*np.roll(ring[:, 1], -1)
                      -ring[:, 1]*np.roll(ring[:, 0], -1)))/2


def aggregate(plans, bands):
    result, groups = [], {}
    for plan in plans:
        rings, top, bottom, colour, centre = plan
        closest = np.maximum(np.maximum(rings[0].min(axis=0), -rings[0].max(axis=0)), 0)
        distance = np.linalg.norm(closest)
        size = next(size for end, size in bands if distance < end)
        if not size or bottom > 0:
            result.append(plan)
            continue
        area = max(0., polygon_area(rings[0])-sum(polygon_area(r) for r in rings[1:]))
        if area == 0:
            continue
        key = (size, *np.floor(centre/size).astype(int))
        if key not in groups:
            groups[key] = [0., np.zeros(2), 0., np.zeros(3), plan]
        group = groups[key]
        group[0] += area
        group[1] += centre*area
        group[2] += top*area
        group[3] += colour*area
        if top > group[4][1]:
            group[4] = plan
    for (size, _, _), (area, position, height, colour, tallest) in groups.items():
        centre, top = position/area, height/area
        half = min(size, math.sqrt(area))/2
        ring = centre+np.array([[-half, -half], [half, -half], [half, half], [-half, half]])
        result.append(([ring], top, 0., colour/area, centre))
        if tallest[1] > top*1.5:
            result.append(tallest)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', required=True, type=Path)
    parser.add_argument('--scenario', type=Path, default=Path('src/assets/places/Tokyo.scenario'))
    parser.add_argument('--reference', type=Path, default=Path('build/experiments/building-lod-checked/reference.png'))
    parser.add_argument('--output', type=Path, default=Path('build/experiments/building-massing'))
    args = parser.parse_args()
    view = ET.parse(args.scenario).getroot().find('views/view')
    at = view.find('at').attrib
    reference = np.array(Image.open(args.reference).convert('RGB'))
    height, width = reference.shape[:2]
    focal = height/(2*math.tan(math.radians(float(view.attrib['fovDeg']))/2))
    eye = np.array([0., 0., float(at['heightM'])])
    basis = lod.camera(float(at['bearingDeg']), float(at['pitchDeg']))
    plans, source_bytes, _ = lod.read_plans(args.manifest, (float(at['lon']), float(at['lat'])))
    began = time.perf_counter()
    bands = [(250, 0), (1000, 32), (4000, 256), (math.inf, 2048)]
    products = aggregate(plans, bands)
    aggregation_ms = (time.perf_counter()-began)*1000
    mesh, admitted = lod.triangles(products, eye, basis, width, height, focal)
    field = lod.rasterize(mesh, eye, basis, width, height, focal, layers=1)
    image = lod.shade(field[1][0], field[2][0], field[0][0], [1, -1, 2])
    args.output.mkdir(parents=True, exist_ok=True)
    Image.fromarray(image).save(args.output/'candidate.png')
    difference = np.abs(image.astype(int)-reference.astype(int))
    rows = []
    low = 0
    for high, size in bands:
        members = {i for i, p in enumerate(products) if low <= np.linalg.norm(p[4]) < high}
        rows.append({'nearM': low, 'farM': high if math.isfinite(high) else None,
                     'cellM': size, 'products': len(members),
                     'admittedTriangles': sum(owner in members for _, _, _, owner in mesh)})
        low = high
    report = {'profile': [width, height], 'sourceBytes': source_bytes, 'sourcePlans': len(plans),
              'products': len(products), 'frustumProducts': admitted, 'triangles': len(mesh),
              'aggregationPythonMs': aggregation_ms, 'bands': rows,
              'meanByteError': float(difference.mean()),
              'limits': ['flat ground/roofs', 'square massing changes silhouette and gaps',
                         'overlapping zooms and tile fragments may remain',
                         'cell boundaries need stable transitions', 'not a native performance claim']}
    (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

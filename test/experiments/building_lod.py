#!/usr/bin/env python3
"""Offline OSM ablation: shells versus unlit depth fields, including disocclusion.

Requires numpy, Pillow, mapbox-vector-tile and mapbox-earcut. Outputs belong in build/.
Manifest: TSV cache path, zoom, tile x, tile y. Only exact ring duplicates are removed;
overlapping zooms and tile fragments may remain. Flat ground/roofs and Lambert lighting
deliberately isolate representation cost; no engine/GPU claim.
"""
import argparse
import json
import math
import time
from pathlib import Path
import xml.etree.ElementTree as ET

import mapbox_earcut
import mapbox_vector_tile
import numpy as np
from PIL import Image

GIRTH = 40075016.68557849


def ring_key(ring):
    points = list(map(tuple, np.round(ring, 3)))
    start = min(range(len(points)), key=points.__getitem__)
    forward = points[start:]+points[:start]
    backward = [forward[0]]+forward[:0:-1]
    return tuple(min(forward, backward))


def read_plans(manifest, origin):
    plans, seen, source_bytes, missing_heights = [], set(), 0, 0
    lon, lat = origin
    scale = math.cos(math.radians(lat))
    anchor = np.array([(lon / 360) * GIRTH,
                       math.asinh(math.tan(math.radians(lat))) * GIRTH / (2 * math.pi)])
    for line in manifest.read_text().splitlines():
        path, zoom, tx, ty = line.split('\t')
        data = Path(path).read_bytes()
        source_bytes += len(data)
        tile = mapbox_vector_tile.decode(data, default_options={'y_coord_down': True})
        layer = tile.get('building', {})
        extent = layer.get('extent', 4096)
        span = GIRTH / (2 ** int(zoom))
        for feature in layer.get('features', []):
            props, geometry = feature['properties'], feature['geometry']
            polygons = geometry['coordinates']
            if geometry['type'] == 'Polygon':
                polygons = [polygons]
            if geometry['type'] not in ('Polygon', 'MultiPolygon'):
                continue
            for polygon in polygons:
                rings = []
                for ring in polygon:
                    points = np.asarray(ring, dtype=np.float64)
                    if len(points) > 1 and np.array_equal(points[0], points[-1]):
                        points = points[:-1]
                    if len(points) < 3:
                        continue
                    mercator = np.column_stack(((int(tx) + points[:, 0] / extent) * span - GIRTH / 2,
                                                 GIRTH / 2 - (int(ty) + points[:, 1] / extent) * span))
                    rings.append((mercator - anchor) * scale)
                if not rings:
                    continue
                height = props.get('render_height', props.get('height'))
                minimum = props.get('render_min_height', props.get('min_height', 0))
                try:
                    height = float(height)
                    minimum = float(minimum)
                    if not math.isfinite(height + minimum) or height <= minimum:
                        raise ValueError('height')
                except (TypeError, ValueError):
                    height, minimum = 9., 0.
                    missing_heights += 1
                colour = str(props.get('colour', props.get('color', 'c1b7a4'))).removeprefix('#')
                try:
                    if len(colour) != 6:
                        raise ValueError('colour')
                    colour = np.array([int(colour[i:i+2], 16) / 255 for i in (0, 2, 4)])
                except ValueError:
                    colour = np.array([.76, .72, .64])
                key = (ring_key(rings[0]), tuple(sorted(ring_key(r) for r in rings[1:])),
                       height, minimum, tuple(colour))
                if key in seen:
                    continue
                seen.add(key)
                centre = rings[0].mean(axis=0)
                plans.append((rings, height, minimum, colour, centre))
    return plans, source_bytes, missing_heights


def camera(yaw, pitch):
    yaw, pitch = map(math.radians, (yaw, pitch))
    right = np.array([math.cos(yaw), -math.sin(yaw), 0])
    forward = np.array([math.sin(yaw)*math.cos(pitch), math.cos(yaw)*math.cos(pitch), math.sin(pitch)])
    return np.stack((right, np.cross(right, forward), forward))


def triangles(plans, eye, basis, width, height, focal):
    made, admitted = [], 0
    for owner, (rings, top, bottom, colour, centre) in enumerate(plans):
        corners = np.array([[e, n, z] for e in (rings[0][:, 0].min(), rings[0][:, 0].max())
                            for n in (rings[0][:, 1].min(), rings[0][:, 1].max())
                            for z in (bottom, top)])
        projected = (corners-eye) @ basis.T
        x, y, z = projected.T
        if (np.all(z < .1) or np.all(x*focal > z*width/2) or np.all(x*focal < -z*width/2)
                or np.all(y*focal > z*height/2) or np.all(y*focal < -z*height/2)):
            continue
        admitted += 1
        for ring_index, ring in enumerate(rings):
            area = np.sum(ring[:, 0]*np.roll(ring[:, 1], -1)-ring[:, 1]*np.roll(ring[:, 0], -1))
            for a, b in zip(ring, np.roll(ring, -1, axis=0)):
                delta = b-a
                normal = np.array([delta[1], -delta[0], 0.]) * (1 if area > 0 else -1)
                if ring_index:
                    normal = -normal
                norm = np.linalg.norm(normal)
                if norm == 0:
                    continue
                normal /= norm
                midpoint = np.array([*(.5*(a+b)), .5*(top+bottom)])
                if np.dot(normal, eye-midpoint) <= 0:
                    continue
                vertices = np.array([[*a, bottom], [*b, bottom], [*b, top], [*a, top]])
                for indices in ((0, 1, 2), (0, 2, 3)):
                    made.append((vertices[list(indices)], normal, colour, owner))
        if top < eye[2]:
            points = np.concatenate(rings)
            ends = np.cumsum([len(ring) for ring in rings], dtype=np.uint32)
            indices = mapbox_earcut.triangulate_float64(points, ends).reshape(-1, 3)
            roof_colour = colour * np.array([.8, .76, .72])
            for triangle in indices:
                made.append((np.column_stack((points[triangle], np.full(3, top))),
                             np.array([0., 0., 1.]), roof_colour, owner))
    return made, admitted


def clip_near(vertices, near=.1):
    result = []
    for a, b in zip(vertices, np.roll(vertices, -1, axis=0)):
        if a[2] >= near:
            result.append(a)
        if (a[2] >= near) != (b[2] >= near):
            result.append(a + (b-a)*((near-a[2])/(b[2]-a[2])))
    return np.array(result)


def rasterize(mesh, eye, basis, width, height, focal, layers=2):
    depth = np.full((layers, height, width), np.inf, dtype=np.float32)
    normal = np.zeros((layers, height, width, 3), dtype=np.float32)
    colour = np.zeros_like(normal)
    owners = np.full((layers, height, width), -1, dtype=np.int32)
    for vertices, n, c, owner in mesh:
        polygon = clip_near((vertices-eye) @ basis.T)
        for index in range(1, len(polygon)-1):
            tri = polygon[[0, index, index+1]]
            screen = np.column_stack((width/2+focal*tri[:, 0]/tri[:, 2],
                                      height/2-focal*tri[:, 1]/tri[:, 2]))
            lo = np.maximum(np.floor(screen.min(axis=0)).astype(int), 0)
            hi = np.minimum(np.ceil(screen.max(axis=0)).astype(int), [width-1, height-1])
            if np.any(lo > hi):
                continue
            a, b, d = screen
            denominator = (b[1]-d[1])*(a[0]-d[0])+(d[0]-b[0])*(a[1]-d[1])
            if abs(denominator) < 1e-8:
                continue
            xx, yy = np.meshgrid(np.arange(lo[0], hi[0]+1)+.5, np.arange(lo[1], hi[1]+1)+.5)
            u = ((b[1]-d[1])*(xx-d[0])+(d[0]-b[0])*(yy-d[1]))/denominator
            v = ((d[1]-a[1])*(xx-d[0])+(a[0]-d[0])*(yy-d[1]))/denominator
            inside = (u >= 0) & (v >= 0) & (u+v <= 1)
            inverse = u/tri[0, 2]+v/tri[1, 2]+(1-u-v)/tri[2, 2]
            z = np.divide(1., inverse, out=np.full_like(inverse, np.inf), where=inverse > 0)
            row, col = np.nonzero(inside)
            y, x = row+lo[1], col+lo[0]
            candidate = z[row, col]
            candidate_n = np.broadcast_to(n, (len(x), 3)).copy()
            candidate_c = np.broadcast_to(c, (len(x), 3)).copy()
            candidate_owner = np.full(len(x), owner, dtype=np.int32)
            for layer in range(layers):
                take = candidate < depth[layer, y, x] - 1e-5
                yt, xt = y[take], x[take]
                previous = (depth[layer, yt, xt].copy(), normal[layer, yt, xt].copy(),
                            colour[layer, yt, xt].copy(), owners[layer, yt, xt].copy())
                depth[layer, yt, xt] = candidate[take]
                normal[layer, yt, xt] = candidate_n[take]
                colour[layer, yt, xt] = candidate_c[take]
                owners[layer, yt, xt] = candidate_owner[take]
                candidate[take], candidate_n[take], candidate_c[take], candidate_owner[take] = previous
    return depth, normal, colour, owners


def shade(normal, colour, depth, sun):
    sun = np.asarray(sun, dtype=np.float32)
    sun /= np.linalg.norm(sun)
    light = .38+.62*np.maximum(normal @ sun, 0)
    rgb = np.clip(colour*light[..., None], 0, 1)
    rgb[~np.isfinite(depth)] = [.23, .36, .5]
    return np.rint(rgb*255).astype(np.uint8)


def replay(field, eye, basis, target_eye, target_basis, focal, layers):
    depths, normals, colours, owners = field
    height, width = depths.shape[1:]
    layer, row, col = np.nonzero(np.isfinite(depths[:layers]))
    z = depths[layer, row, col]
    local = np.column_stack(((col+.5-width/2)*z/focal, (height/2-row-.5)*z/focal, z))
    assert np.isfinite(local).all()
    world = np.einsum('ij,jk->ik', local, basis) + eye
    target = np.einsum('ij,kj->ik', world-target_eye, target_basis)
    selected = np.flatnonzero(target[:, 2] > .1)
    pixel = np.full((len(target), 2), -1, dtype=np.int64)
    front = target[selected]
    pixel[selected] = np.floor(np.column_stack((width/2+focal*front[:, 0]/front[:, 2],
                                               height/2-focal*front[:, 1]/front[:, 2]))).astype(int)
    selected = selected[(pixel[selected, 0] >= 0) & (pixel[selected, 0] < width)
                        & (pixel[selected, 1] >= 0) & (pixel[selected, 1] < height)]
    flat = pixel[selected, 1]*width+pixel[selected, 0]
    order = np.lexsort((target[selected, 2], flat))
    selected, flat = selected[order], flat[order]
    first = np.r_[True, flat[1:] != flat[:-1]] if len(flat) else np.zeros(0, dtype=bool)
    selected, flat = selected[first], flat[first]
    out_depth = np.full(height*width, np.inf, np.float32)
    out_normal = np.zeros((height*width, 3), np.float32)
    out_colour = np.zeros_like(out_normal)
    out_owner = np.full(height*width, -1, np.int32)
    out_depth[flat] = target[selected, 2]
    out_normal[flat] = normals[layer[selected], row[selected], col[selected]]
    out_colour[flat] = colours[layer[selected], row[selected], col[selected]]
    out_owner[flat] = owners[layer[selected], row[selected], col[selected]]
    return (out_depth.reshape(height, width), out_normal.reshape(height, width, 3),
            out_colour.reshape(height, width, 3), out_owner.reshape(height, width))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--scenario', type=Path, default=Path('src/assets/places/Tokyo.scenario'))
    parser.add_argument('--output', type=Path, default=Path('build/experiments/building-lod'))
    parser.add_argument('--width', type=int, default=640)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    scenario = ET.parse(args.scenario).getroot()
    at = scenario.find('views/view/at').attrib
    view = scenario.find('views/view').attrib
    width, height = args.width, args.width*9//16
    focal = height/(2*math.tan(math.radians(float(view['fovDeg']))/2))
    eye = np.array([0., 0., float(at['heightM'])])
    basis = camera(float(at['bearingDeg']), float(at['pitchDeg']))
    began = time.perf_counter()
    plans, source_bytes, missing_heights = read_plans(args.manifest, (float(at['lon']), float(at['lat'])))
    decode_ms = (time.perf_counter()-began)*1000
    began = time.perf_counter()
    mesh, admitted = triangles(plans, eye, basis, width, height, focal)
    generation_ms = (time.perf_counter()-began)*1000
    began = time.perf_counter()
    field = rasterize(mesh, eye, basis, width, height, focal)
    capture_ms = (time.perf_counter()-began)*1000
    reference = shade(field[1][0], field[2][0], field[0][0], [1, -1, 2])
    Image.fromarray(reference).save(args.output/'reference.png')
    rows = []
    for offset in (0., 1., 4., 16.):
        target_eye = eye+np.array([offset, 0., 0.])
        if offset == 0:
            target_field = field
        else:
            target_mesh, _ = triangles(plans, target_eye, basis, width, height, focal)
            target_field = rasterize(target_mesh, target_eye, basis, width, height, focal, layers=1)
        truth = shade(target_field[1][0], target_field[2][0], target_field[0][0], [1, -1, 2])
        Image.fromarray(truth).save(args.output/f'truth-{offset:g}m.png')
        for layers in (1, 2):
            began = time.perf_counter()
            d, n, c, owner = replay(field, eye, basis, target_eye, basis, focal, layers)
            replay_ms = (time.perf_counter()-began)*1000
            image = shade(n, c, d, [1, -1, 2])
            covered = np.isfinite(target_field[0][0])
            missing = covered & ~np.isfinite(d)
            valid = covered & np.isfinite(d)
            depth_difference = np.subtract(d, target_field[0][0], where=valid, out=np.zeros_like(d))
            wrong = valid & (np.abs(depth_difference) > .1)
            difference = np.abs(image.astype(int)-truth.astype(int))
            rows.append({'movementM': offset, 'layers': layers, 'replayPythonMs': replay_ms,
                         'missingPixels': int(missing.sum()), 'wrongDepthPixels': int(wrong.sum()),
                         'meanByteError': float(difference.mean()), 'maxByteError': int(difference.max())})
            Image.fromarray(image).save(args.output/f'replay-{offset:g}m-{layers}layers.png')
    Image.fromarray(shade(field[1][0], field[2][0], field[0][0], [-1, 1, .7])).save(args.output/'relit.png')
    visible = np.unique(field[3][0][field[3][0] >= 0])
    bands = []
    for low, high in ((0, 250), (250, 1000), (1000, 4000), (4000, 16000), (16000, math.inf)):
        members = {i for i,p in enumerate(plans) if low <= np.linalg.norm(p[4]) < high}
        bands.append({'nearM':low, 'farM':high if math.isfinite(high) else None,
                      'plans':len(members), 'visiblePlans':sum(int(i) in members for i in visible),
                      'admittedTriangles':sum(owner in members for _,_,_,owner in mesh)})
    report = {'profile':[width,height], 'sourceBytes':source_bytes, 'plans':len(plans),
              'missingHeightFallbacks':missing_heights, 'frustumPlans':admitted,
              'visiblePlans':len(visible), 'admittedTriangles':len(mesh),
              'unlitFieldBytes':sum(x.nbytes for x in field), 'decodePythonMs':decode_ms,
              'generationPythonMs':generation_ms, 'capturePythonMs':capture_ms,
              'bands':bands, 'replays':rows,
              'limits':['flat ground/roofs', 'local Mercator approximation', 'one view, not 360 coverage',
                        'overlapping zooms and tile fragments may remain', 'Lambert lighting only',
                        'point reprojection needs filtering and disocclusion repair',
                        'Python times are not native CPU or GPU estimates']}
    (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()

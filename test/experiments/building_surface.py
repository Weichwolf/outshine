#!/usr/bin/env python3
"""Trace cached OSM prisms through a source BVH without generating triangles.

Flat ground/roofs deliberately match building_lod.py, not native roof semantics.
The output is unlit depth/normal/material; light changes do not retrace sources.
"""
import argparse
import json
import math
import struct
import time
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image

from building_lod import camera, rasterize, read_plans, shade


class SourceIndex:
    def __init__(self, plans):
        self.plans = plans
        self.bounds = np.array([[*rings[0].min(axis=0), bottom,
                                 *rings[0].max(axis=0), top]
                                for rings, top, bottom, _, _ in plans])
        self.order = np.arange(len(plans))
        self.nodes = []
        self.faces = np.cumsum([0]+[sum(map(len, p[0]))+1 for p in plans])
        if plans:
            self.partition(0, len(plans))

    def partition(self, first, count):
        indices = self.order[first:first+count]
        boxes = self.bounds[indices]
        low, high = boxes[:, :3].min(axis=0), boxes[:, 3:].max(axis=0)
        at = len(self.nodes)
        self.nodes.append((low, high, first, count, 0, 0))
        if count > 8:
            centres = (boxes[:, :3]+boxes[:, 3:])*.5
            axis = np.argmax(np.ptp(centres, axis=0))
            half = count//2
            self.order[first:first+count] = indices[np.argpartition(centres[:, axis], half)]
            left = self.partition(first, half)
            right = self.partition(first+half, count-half)
            self.nodes[at] = (low, high, first, count, left, right)
        return at


def inside_ring(ring, points):
    inside = np.zeros(len(points), bool)
    for a, b in zip(ring, np.roll(ring, -1, axis=0)):
        if b[1] == a[1]:
            continue
        straddles = (a[1] > points[:, 1]) != (b[1] > points[:, 1])
        cross_x = a[0]+(points[:, 1]-a[1])*(b[0]-a[0])/(b[1]-a[1])
        inside ^= straddles & (points[:, 0] < cross_x)
    return inside


def ray_box(low, high, eye, directions, inverse, maximum):
    near = np.full(len(directions), .1)
    far = maximum.copy()
    for axis in range(3):
        parallel = directions[:, axis] == 0
        a = (low[axis]-eye[axis])*inverse[:, axis]
        b = (high[axis]-eye[axis])*inverse[:, axis]
        near = np.maximum(near, np.where(parallel, -np.inf, np.minimum(a, b)))
        far = np.minimum(far, np.where(parallel, np.inf, np.maximum(a, b)))
        far[parallel & ((eye[axis] < low[axis]) | (eye[axis] > high[axis]))] = -np.inf
    return near <= far


def trace_prism(plan, owner, face_first, eye, rays, depth, normals, colours, owners, faces):
    rings, top, bottom, colour, _ = plan
    faces_tested = 0

    def store(candidate, take, normal, material, face):
        take &= (candidate >= .1) & (candidate < depth)
        depth[take] = candidate[take]
        normals[take] = normal
        colours[take] = material
        owners[take] = owner
        faces[take] = face

    face = face_first
    for hole, ring in enumerate(rings):
        area = np.sum(ring[:, 0]*np.roll(ring[:, 1], -1)-ring[:, 1]*np.roll(ring[:, 0], -1))
        for a, b in zip(ring, np.roll(ring, -1, axis=0)):
            face += 1
            edge = b-a
            length = np.linalg.norm(edge)
            if length == 0:
                continue
            normal = np.array([edge[1], -edge[0], 0.])/length * (1 if area > 0 else -1)
            if hole:
                normal = -normal
            denominator = rays[:, 0]*edge[1]-rays[:, 1]*edge[0]
            delta = a-eye[:2]
            distance = np.divide(delta[0]*edge[1]-delta[1]*edge[0], denominator,
                                 out=np.full(len(rays), np.inf), where=denominator != 0)
            along = np.divide(delta[0]*rays[:, 1]-delta[1]*rays[:, 0], denominator,
                              out=np.full(len(rays), np.inf), where=denominator != 0)
            z = eye[2]+np.where(np.isfinite(distance), distance, 0)*rays[:, 2]
            toward = rays[:, 0]*normal[0]+rays[:, 1]*normal[1]
            take = (along >= 0) & (along <= 1) & (z >= bottom) & (z <= top) & (toward < 0)
            store(distance, take, normal, colour, face)
            faces_tested += 1
    if eye[2] > top:
        distance = np.divide(top-eye[2], rays[:, 2], out=np.full(len(rays), np.inf),
                             where=rays[:, 2] != 0)
        viable = (distance >= .1) & (distance < depth)
        ids = np.flatnonzero(viable)
        points = eye[:2]+rays[ids, :2]*distance[ids, None]
        inside = inside_ring(rings[0], points)
        for ring in rings[1:]:
            inside &= ~inside_ring(ring, points)
        viable[ids] = inside
        store(distance, viable, [0, 0, 1], colour*np.array([.8, .76, .72]), face+1)
        faces_tested += 1
    return faces_tested


def trace(index, eye, basis, width, height, focal):
    row, col = np.indices((height, width))
    local = np.column_stack(((col.ravel()+.5-width/2)/focal,
                             (height/2-row.ravel()-.5)/focal, np.ones(width*height)))
    rays = np.einsum('ij,jk->ik', local, basis)
    inverse = np.divide(1., rays, out=np.zeros_like(rays), where=rays != 0)
    depth = np.full(width*height, np.inf)
    normals = np.zeros((width*height, 3), np.float32)
    colours = np.zeros_like(normals)
    owners = np.full(width*height, -1, np.int32)
    faces = np.full(width*height, -1, np.int64)
    pending = [(0, np.arange(width*height))] if index.nodes else []
    visits, prism_tests, face_tests = 0, 0, 0
    while pending:
        at, ids = pending.pop()
        low, high, first, count, left, right = index.nodes[at]
        take = ray_box(low, high, eye, rays[ids], inverse[ids], depth[ids])
        visits += len(ids)
        ids = ids[take]
        if not len(ids):
            continue
        if count > 8:
            children = sorted((left, right), key=lambda c: np.linalg.norm(
                eye-np.clip(eye, index.nodes[c][0], index.nodes[c][1])), reverse=True)
            pending.extend((child, ids) for child in children)
            continue
        for owner in index.order[first:first+count]:
            box = index.bounds[owner]
            take = ray_box(box[:3], box[3:], eye, rays[ids], inverse[ids], depth[ids])
            selected = ids[take]
            if not len(selected):
                continue
            held = [array[selected].copy() for array in (depth, normals, colours, owners, faces)]
            prism_tests += len(selected)
            face_tests += trace_prism(index.plans[owner], owner, index.faces[owner], eye,
                                      rays[selected], *held)
            for array, value in zip((depth, normals, colours, owners, faces), held):
                array[selected] = value
    field = (depth.reshape(height, width), normals.reshape(height, width, 3),
             colours.reshape(height, width, 3), owners.reshape(height, width))
    return field, faces.reshape(height, width), {
        'nodeRayTests': visits, 'prismRayTests': prism_tests, 'facePacketTests': face_tests}


def patches(faces):
    remaining = faces.copy()
    height, width = faces.shape
    result = []
    for y in range(height):
        x = 0
        while x < width:
            face = remaining[y, x]
            if face < 0:
                x += 1
                continue
            right = x+1
            while right < width and remaining[y, right] == face:
                right += 1
            bottom = y+1
            while bottom < height and np.all(remaining[bottom, x:right] == face):
                bottom += 1
            remaining[y:bottom, x:right] = -1
            result.append((x, y, right, bottom, int(face)))
            x = right
    return result


def reconstruct(rectangles, field, eye, basis, width, height, focal):
    depth, normals, colours, owners = field
    result = []
    for left, top, right, bottom, _ in rectangles:
        normal, colour, owner = normals[top, left], colours[top, left], owners[top, left]
        at = np.einsum('i,ij->j',
                        [(left+.5-width/2)/focal, (height/2-top-.5)/focal, 1.], basis)
        plane = np.dot(at*depth[top, left], normal)
        corners = np.array([[left, bottom], [right, bottom], [right, top], [left, top]])
        local = np.column_stack(((corners[:, 0]-width/2)/focal,
                                  (height/2-corners[:, 1])/focal, np.ones(4)))
        rays = np.einsum('ij,jk->ik', local, basis)
        denominator = np.einsum('ij,j->i', rays, normal)
        if np.any(denominator == 0):
            raise ValueError('a surface patch crosses a projection singularity')
        along = plane/denominator
        if np.any(~np.isfinite(along)) or np.any(along <= 0):
            raise ValueError('a surface patch crosses the eye plane')
        vertices = eye+rays*along[:, None]
        for indices in ((0, 1, 2), (0, 2, 3)):
            result.append((vertices[list(indices)], normal, colour, owner))
    return result


def export_native(path, plans, eye, basis, width, height, focal):
    rings = [ring for plan in plans for ring in plan[0]]
    points = np.concatenate(rings)
    ranges = np.column_stack((np.cumsum([0]+[len(ring) for ring in rings[:-1]]),
                              [len(ring) for ring in rings])).astype('<u8')
    with path.open('wb') as output:
        output.write(struct.pack('<QQQII13d', len(plans), len(rings), len(points), width, height,
                                  *eye, *basis.ravel(), focal))
        output.write(points.astype('<f8').tobytes())
        output.write(ranges.tobytes())
        first = 0
        for rings, top, bottom, colour, _ in plans:
            output.write(struct.pack('<QQdd3f', first, len(rings), bottom, top, *colour))
            first += len(rings)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--scenario', type=Path, default=Path('src/assets/places/Tokyo.scenario'))
    parser.add_argument('--output', type=Path, default=Path('build/experiments/building-surface'))
    parser.add_argument('--reference', type=Path)
    parser.add_argument('--width', type=int, default=640)
    parser.add_argument('--movement', type=float, nargs='*', default=[0., 1., 4., 16.])
    parser.add_argument('--native-input', type=Path)
    args = parser.parse_args()
    scenario = ET.parse(args.scenario).getroot()
    at, view = scenario.find('views/view/at').attrib, scenario.find('views/view').attrib
    width, height = args.width, args.width*9//16
    focal = height/(2*math.tan(math.radians(float(view['fovDeg']))/2))
    eye = np.array([0., 0., float(at['heightM'])])
    basis = camera(float(at['bearingDeg']), float(at['pitchDeg']))
    start = time.perf_counter()
    plans, source_bytes, _ = read_plans(args.manifest, (float(at['lon']), float(at['lat'])))
    decode_ms = (time.perf_counter()-start)*1000
    if args.native_input:
        export_native(args.native_input, plans, eye, basis, width, height, focal)
    start = time.perf_counter()
    index = SourceIndex(plans)
    index_ms = (time.perf_counter()-start)*1000
    start = time.perf_counter()
    field, faces, counts = trace(index, eye, basis, width, height, focal)
    trace_ms = (time.perf_counter()-start)*1000
    start = time.perf_counter()
    rectangles = patches(faces)
    patch_ms = (time.perf_counter()-start)*1000
    image = shade(field[1], field[2], field[0], [1, -1, 2])
    args.output.mkdir(parents=True, exist_ok=True)
    Image.fromarray(image).save(args.output/'surface.png')
    Image.fromarray(shade(field[1], field[2], field[0], [-1, 1, .7])).save(args.output/'relit.png')
    report = {'profile': [width, height], 'sourceBytes': source_bytes, 'plans': len(plans),
              'sourceTriangles': 0, 'sourceVertices': 0, 'nodes': len(index.nodes),
              'visiblePlans': int(len(np.unique(field[3][field[3] >= 0]))),
              'surfacePatches': len(rectangles), 'surfaceTriangles': len(rectangles)*2,
              'decodePythonMs': decode_ms, 'indexPythonMs': index_ms,
              'tracePythonMs': trace_ms, 'patchPythonMs': patch_ms,
              'limits': ['flat ground/roofs', 'one view, not full 360-degree residency',
                         'one depth layer loses disoccluded surfaces',
                         'no native device/frame-time claim'], **counts}
    mesh = reconstruct(rectangles, field, eye, basis, width, height, focal)
    movements = []
    for offset in args.movement:
        target_eye = eye+np.array([offset, 0, 0])
        truth = field if offset == 0 else trace(index, target_eye, basis, width, height, focal)[0]
        captured = rasterize(mesh, target_eye, basis, width, height, focal, layers=1)
        replay = shade(captured[1][0], captured[2][0], captured[0][0], [1, -1, 2])
        reference = shade(truth[1], truth[2], truth[0], [1, -1, 2])
        covered = np.isfinite(truth[0])
        missing = covered & ~np.isfinite(captured[0][0])
        difference = np.abs(reference.astype(int)-replay.astype(int))
        low = 0
        bands = []
        for high in (250, 1000, 4000, 16000, math.inf):
            band = (truth[0] >= low) & (truth[0] < high)
            bands.append({'nearDepthM': low, 'farDepthM': high if math.isfinite(high) else None,
                          'missingPixels': int((missing & band).sum())})
            low = high
        movements.append({'movementM': offset, 'missingPixels': int(missing.sum()),
                          'changedPixels': int(np.any(difference != 0, axis=2).sum()),
                          'meanByteError': float(difference.mean()),
                          'maxByteError': int(difference.max()), 'depthBands': bands})
        Image.fromarray(replay).save(args.output/f'patch-replay-{offset:g}m.png')
        Image.fromarray(reference).save(args.output/f'patch-truth-{offset:g}m.png')
    report['patchReplays'] = movements
    if args.reference:
        reference = np.array(Image.open(args.reference).convert('RGB'))
        if reference.shape != image.shape:
            raise ValueError('reference resolution must match')
        difference = np.abs(reference.astype(int)-image.astype(int))
        report.update(changedPixels=int(np.any(difference != 0, axis=2).sum()),
                      meanByteError=float(difference.mean()), maxByteError=int(difference.max()))
    (args.output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()

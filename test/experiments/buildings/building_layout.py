"""Bounded fixed-envelope room partitions; geometry precedes architectural decoration."""

import math
import time

import numpy as np
import shapely
from shapely.affinity import affine_transform


WALL_M = .24
PARTITION_M = .12
DOOR_M = 1.10
AGENT_RADIUS_M = .30
MAX_ROOMS = 12
TARGET_AREA_M2 = 70.
DERIVED_GRID_M = 1e-8
AREA_TOL_M2 = 1e-6


def canonical_geometry(geometry):
    result = shapely.set_precision(geometry, DERIVED_GRID_M)
    if geometry.symmetric_difference(result).area > AREA_TOL_M2:
        raise ValueError('Derived precision exceeds the 1e-6 square metre area tolerance')
    return result


def polygons(geometry):
    return [part for part in shapely.get_parts(geometry) if part.geom_type == 'Polygon' and part.area > 1e-8]


def segments(geometry):
    if geometry.geom_type == 'LineString':
        points = list(geometry.simplify(1e-8).coords)
        return [shapely.LineString([a, b]) for a, b in zip(points, points[1:]) if math.dist(a, b) > 1e-8]
    if geometry.geom_type in ('MultiLineString', 'GeometryCollection'):
        return [line for part in shapely.get_parts(geometry) for line in segments(part)]
    return []


def frame(polygon):
    ring = np.array(polygon.minimum_rotated_rectangle.exterior.coords)[:4]
    vectors = np.roll(ring, -1, axis=0)-ring
    vector = max(vectors, key=lambda v: (float(np.dot(v, v)), float(v[0]), float(v[1])))
    tangent = vector/np.linalg.norm(vector)
    if tangent[0] < 0 or (tangent[0] == 0 and tangent[1] < 0):
        tangent = -tangent
    normal, centre = np.array([-tangent[1], tangent[0]]), np.array(polygon.centroid.coords[0])
    forward = [*tangent, *normal, -float(tangent@centre), -float(normal@centre)]
    reverse = [tangent[0], normal[0], tangent[1], normal[1], *centre]
    return forward, reverse


def usable(polygon):
    if polygon.geom_type != 'Polygon' or polygon.area < 10.:
        return False
    clear = polygon.buffer(-PARTITION_M/2-AGENT_RADIUS_M, join_style='mitre')
    return clear.geom_type == 'Polygon' and not clear.is_empty and clear.area > 1.


def candidate_split(polygon):
    bounds = polygon.bounds
    candidates = []
    for axis in (0, 1):
        for fraction in (.5, .4, .6):
            cut = bounds[axis]+(bounds[axis+2]-bounds[axis])*fraction
            left = list(bounds)
            right = list(bounds)
            left[axis+2], right[axis] = cut, cut
            a = polygon.intersection(shapely.box(*left))
            b = polygon.intersection(shapely.box(*right))
            if not usable(a) or not usable(b):
                continue
            shared = segments(a.boundary.intersection(b.boundary))
            if not shared or max(line.length for line in shared) < DOOR_M+.40:
                continue
            score = abs(a.area-b.area)/polygon.area+.1*(a.length/math.sqrt(a.area)+b.length/math.sqrt(b.area))
            candidates.append((score, axis, fraction, a, b))
    return min(candidates, key=lambda row: row[:3])[3:] if candidates else None


def bsp(polygon):
    leaves, exhausted, tested = [polygon], set(), 0
    while len(leaves) < MAX_ROOMS:
        choices = [(part.area, index) for index, part in enumerate(leaves)
                   if part.area > TARGET_AREA_M2 and index not in exhausted]
        if not choices:
            break
        _, index = max(choices)
        split = candidate_split(leaves[index])
        tested += 6
        if split is None:
            exhausted.add(index)
        else:
            leaves[index] = split[0]
            leaves.append(split[1])
    return leaves, tested


def strips(polygon):
    a, b, c, d = polygon.bounds
    count = min(MAX_ROOMS, max(1, math.ceil(polygon.area/TARGET_AREA_M2)))
    zones = [part for index in range(count)
             for part in polygons(polygon.intersection(shapely.box(a+(c-a)*index/count, b,
                                                                    a+(c-a)*(index+1)/count, d)))]
    return zones, count


def rectangle(centre, tangent, width, outward, inward):
    centre, tangent = np.asarray(centre), np.asarray(tangent)
    normal = np.array([tangent[1], -tangent[0]])
    return shapely.Polygon([centre+tangent*s+normal*t for s, t in
                            ((-width/2, outward), (width/2, outward), (width/2, -inward), (-width/2, -inward))])


def contacts(zones):
    result = []
    for first, a in enumerate(zones):
        for second in range(first+1, len(zones)):
            shared = segments(a.boundary.intersection(zones[second].boundary))
            if shared:
                result.append((first, second, shared))
    return result


def room_portals(floors, shared_edges):
    doors = []
    for first, second, shared in shared_edges:
        for line in sorted(shared, key=lambda item: -item.length):
            if line.length < DOOR_M+.40:
                continue
            middle = np.array(line.interpolate(.5, normalized=True).coords[0])
            direction = np.array(line.interpolate(.6, normalized=True).coords[0])-middle
            tangent = direction/np.linalg.norm(direction)
            opening = rectangle(middle, tangent, DOOR_M, .20, .20)
            normal = np.array([tangent[1], -tangent[0]])
            backing = [shapely.LineString([middle+normal*sign*.46-tangent*DOOR_M/2,
                                          middle+normal*sign*.46+tangent*DOOR_M/2]) for sign in (-1, 1)]
            owners = [{i for i in (first, second) if floors[i].covers(edge)} for edge in backing]
            if len(owners[0])!=1 or len(owners[1])!=1 or owners[0]==owners[1]:continue
            approach = shapely.LineString([middle-normal*.45,middle+normal*.45])
            passage = floors[first].union(floors[second]).union(opening)
            if passage.covers(approach) and approach.distance(passage.boundary)>=AGENT_RADIUS_M:
                doors.append(dict(id=f'door-{first}-{second}', rooms=[first, second], polygon=opening,
                                  centre=middle.tolist(), tangent=tangent.tolist(), height_m=2.15, kind='room-door'))
                break
    return doors


def adjacency(rooms, portals):
    reached = {0} if rooms else set()
    for _ in range(len(rooms)):
        previous = len(reached)
        for door in portals:
            if reached.intersection(door['rooms']):
                reached.update(door['rooms'])
        if len(reached) == previous:
            break
    return reached


def world_floor(room, transform):
    result = affine_transform(room, transform)
    if result.is_valid and result.geom_type == 'Polygon':
        floor = result
    else:
        repaired = result if result.is_valid else shapely.make_valid(result, method='structure', keep_collapsed=False)
        candidates = polygons(repaired)
        if not candidates:
            return shapely.Polygon()
        floor = max(candidates, key=lambda part: part.area)
        if abs(floor.area-result.area) > 1e-6:
            return shapely.Polygon()
    canonical = canonical_geometry(floor)
    if canonical.geom_type != 'Polygon' or canonical.is_empty:
        return shapely.Polygon()
    return canonical


def transform_portal(door, transform):
    door['polygon'] = affine_transform(door['polygon'], transform)
    door['centre'] = list(affine_transform(shapely.Point(door['centre']), transform).coords[0])
    vector = np.asarray(door['tangent'])
    door['tangent'] = [transform[0]*vector[0]+transform[1]*vector[1], transform[2]*vector[0]+transform[3]*vector[1]]
    return door


def partitions(interior):
    forward, reverse = frame(interior)
    local = affine_transform(interior, forward)
    results = []
    for name, procedure in (('strips', strips), ('bsp', bsp)):
        began = time.perf_counter()
        zones, tested = procedure(local)
        if len(zones) > MAX_ROOMS:
            results.append(dict(method=name, zones=[], rooms=[], portals=[], valid=False, score=None,
                                candidates=tested, elapsed_ms=(time.perf_counter()-began)*1000,
                                reason='clipped strips exceed the bounded room count'))
            continue
        shared_edges = contacts(zones)
        walls = shapely.union_all([line for _, _, shared in shared_edges for line in shared]).buffer(
            PARTITION_M/2, cap_style='flat', join_style='mitre')
        rooms = [zone.difference(walls) for zone in zones]
        portals = room_portals(rooms, shared_edges)
        valid = len(zones) <= MAX_ROOMS and all(usable(zone) for zone in zones)
        connected = len(adjacency(rooms, portals)) == len(rooms)
        score = sum(max(0., room.area-TARGET_AREA_M2)/TARGET_AREA_M2 for room in rooms)
        score += .1*sum(room.length/math.sqrt(max(room.area, 1e-8)) for room in rooms)
        for door in portals:
            transform_portal(door, reverse)
        zones = [affine_transform(zone, reverse) for zone in zones]
        rooms = [world_floor(room, reverse) for room in rooms]
        valid = valid and all(not room.is_empty for room in rooms)
        results.append(dict(method=name, zones=zones, rooms=rooms, portals=portals, valid=valid and connected,
                            score=score, candidates=tested, elapsed_ms=(time.perf_counter()-began)*1000,
                            reason=None if valid and connected else 'room size, clearance or portal connectivity'))
    valid = [result for result in results if result['valid']]
    winner = min(valid, key=lambda item: (item['score'], item['method'])) if valid else dict(
        method='open-plan', zones=[interior], rooms=[interior], portals=[], candidates=0,
        valid=True, reason='bounded partitions unsuitable; retain one open room')
    comparison = [{key: row[key] for key in ('method', 'valid', 'score', 'candidates', 'elapsed_ms', 'reason')}
                  | dict(rooms=len(row['rooms'])) for row in results]
    return winner, comparison

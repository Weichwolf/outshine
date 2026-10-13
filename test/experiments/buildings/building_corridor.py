"""Compare direct doglegs and a bounded orthogonal visibility grid for common access."""

import heapq

import numpy as np
import shapely

from building_layout import polygons


CORRIDOR_ZONE_M = 1.62
MAX_GRID_AXIS = 40


def doglegs(space, anchors):
    root, paths = anchors[0], []
    for target in anchors[1:]:
        choices = [shapely.LineString([root, [root[0], target[1]], target]),
                   shapely.LineString([root, [target[0], root[1]], target])]
        legal = [line for line in choices if space.covers(line)]
        if not legal:
            return None, dict(candidates=2*(len(anchors)-1), reason='no contained two-segment dogleg')
        paths.append(min(legal, key=lambda line: (line.length, line.wkb_hex)))
    return paths, dict(candidates=2*(len(anchors)-1), grid_nodes=0, grid_edges=0)


def coordinates(space, anchors, axis):
    required = sorted({float(point[axis]) for point in anchors})
    events = np.unique([float(point[axis]) for part in polygons(space)
                        for ring in [part.exterior, *part.interiors] for point in ring.coords])
    available = MAX_GRID_AXIS-len(required)
    if available < 2:
        raise ValueError('too many circulation anchors for the bounded grid')
    if len(events) > available:
        events = events[np.unique(np.linspace(0, len(events)-1, available).astype(int))]
    return np.unique([*required, *events])


def graph(space, anchors):
    x, y = coordinates(space, anchors, 0), coordinates(space, anchors, 1)
    xx, yy = np.meshgrid(x, y)
    points = np.c_[xx.ravel(), yy.ravel()]
    shapely.prepare(space)
    legal = shapely.covers(space, shapely.points(points))
    links = []
    for index, valid in enumerate(legal):
        if not valid:
            continue
        if index % len(x)+1 < len(x) and legal[index+1]:
            links.append((index, index+1))
        if index+len(x) < len(points) and legal[index+len(x)]:
            links.append((index, index+len(x)))
    segments = shapely.linestrings(np.asarray([[points[a], points[b]] for a, b in links])) if links else []
    contained = shapely.covers(space, segments)
    adjacency = [[] for _ in points]
    for (a, b), valid in zip(links, contained):
        if valid:
            distance = float(np.linalg.norm(points[a]-points[b]))
            adjacency[a].append((b, distance))
            adjacency[b].append((a, distance))
    indices = [int(np.argmin(abs(y-point[1])))*len(x)+int(np.argmin(abs(x-point[0]))) for point in anchors]
    if any(not legal[index] or np.linalg.norm(points[index]-point) > 1e-8 for index, point in zip(indices, anchors)):
        return None, dict(candidates=len(links), grid_nodes=len(points), reason='anchor outside the route grid')
    connected, remaining, paths = {indices[0]}, set(indices[1:])-{indices[0]}, []
    while remaining:
        distances, previous = dict.fromkeys(connected, 0.), {}
        queue = [(0., node) for node in sorted(connected)]
        heapq.heapify(queue)
        found = None
        while queue:
            distance, node = heapq.heappop(queue)
            if distance > distances[node]:
                continue
            if node in remaining:
                found = node
                break
            for target, cost in adjacency[node]:
                value = distance+cost
                if value < distances.get(target, float('inf')):
                    distances[target], previous[target] = value, node
                    heapq.heappush(queue, (value, target))
        if found is None:
            return None, dict(candidates=len(links), grid_nodes=len(points), reason='bounded grid does not reach every anchor')
        path = [found]
        while path[-1] not in connected:
            path.append(previous[path[-1]])
        connected.update(path)
        remaining.difference_update(connected)
        paths.append(shapely.LineString(points[path[::-1]]))
    return paths, dict(candidates=len(links), grid_nodes=len(points), grid_edges=sum(map(len, adjacency))//2)


def routes(space, anchors, method):
    return doglegs(space, anchors) if method == 'doglegs' else graph(space, anchors)

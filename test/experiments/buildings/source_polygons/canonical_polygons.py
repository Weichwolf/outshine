"""Geometric research on polygon observations; MVT container IDs are provenance only."""
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path

from shapely.geometry import LineString, Point, Polygon, box
from shapely.geometry.polygon import orient
from shapely.ops import unary_union
from shapely.strtree import STRtree


@dataclass(frozen=True)
class Observation:
    namespace: tuple
    tile: tuple
    input_digest: str
    container: int | None
    ordinal: int
    attributes: str
    polygon: object
    window: tuple


def rings(polygon):
    return [polygon.exterior, *polygon.interiors]


def on_window(point, window):
    x, y = point
    return x in (window[0], window[2]) or y in (window[1], window[3])


def real_vertices(observation):
    return {tuple(p) for ring in rings(observation.polygon) for p in list(ring.coords)[:-1]
            if not on_window(p, observation.window)}


def real_edges(observation):
    result = []
    for ring in rings(observation.polygon):
        coords = list(ring.coords)
        for a, b in zip(coords, coords[1:]):
            # Only edges along the source clipping boundary are known artificial.
            cut = ((a[0] == b[0] and a[0] in (observation.window[0], observation.window[2])) or
                   (a[1] == b[1] and a[1] in (observation.window[1], observation.window[3])))
            if not cut:
                result.append(LineString([a, b]))
    return result


def whole(observation):
    return not any(on_window(p, observation.window) for ring in rings(observation.polygon)
                   for p in list(ring.coords)[:-1])


def _ring_token(ring):
    points = list(ring.coords)[:-1]
    assert all(float(x).is_integer() and float(y).is_integer() for x, y in points)
    points = [(int(x), int(y)) for x, y in points]
    return min(tuple(points[i:] + points[:i]) for i in range(len(points)))


def shape_token(polygon):
    # Zero removes exact collinear vertices only; no positive simplification tolerance.
    polygon = orient(polygon.simplify(0, preserve_topology=True), sign=1.0)
    return (_ring_token(polygon.exterior), tuple(sorted(_ring_token(hole) for hole in polygon.interiors)))


def dependency_digest(observations):
    records = sorted({(o.namespace, o.tile, o.input_digest) for o in observations})
    return hashlib.sha256(json.dumps(records, separators=(',', ':')).encode()).hexdigest()


def components(observations):
    """Only distinct source tiles may connect non-identical observations."""
    parents = list(range(len(observations)))
    def find(x):
        while parents[x] != x:
            parents[x] = parents[parents[x]]
            x = parents[x]
        return x
    tree = STRtree([o.polygon for o in observations])
    for i, observation in enumerate(observations):
        for raw_j in tree.query(observation.polygon):
            j = int(raw_j)
            if j <= i:
                continue
            other = observations[j]
            if observation.namespace != other.namespace:
                continue
            equal = observation.polygon.equals(other.polygon)
            if observation.tile == other.tile and not equal:
                continue
            if equal or observation.polygon.intersection(other.polygon).area > 0:
                parents[find(j)] = find(i)
    groups = {}
    for i, observation in enumerate(observations):
        groups.setdefault(find(i), []).append(observation)
    return list(groups.values())


def boundary_proposal(observations):
    """Discard artificial clip vertices from a union proposal, then require exact support."""
    union = unary_union([o.polygon for o in observations])
    if union.geom_type != 'Polygon':
        return None, 'disconnected-or-ambiguous-union'
    known = set().union(*(real_vertices(o) for o in observations))
    proposed = []
    for ring in rings(union):
        points = [tuple(p) for p in list(ring.coords)[:-1] if tuple(p) in known]
        if len(points) < 3:
            return None, 'missing-original-corners'
        proposed.append(points)
    polygon = Polygon(proposed[0], proposed[1:])
    if not polygon.is_valid or polygon.area <= 0:
        return None, 'invalid-reconstructed-polygon'
    lines = unary_union([edge for o in observations for edge in real_edges(o)])
    if not lines.covers(polygon.boundary):
        return None, 'source-edge-support-incomplete'
    return polygon, 'exact-edge-cover'


def resolve(observations, method='whole-first'):
    proof = dependency_digest(observations)
    base = {'observations': len(observations), 'dependency_digest': proof}
    if len({o.attributes for o in observations}) != 1:
        return dict(base, status='conflicting-attributes')
    full = [o.polygon for o in observations if whole(o)]
    distinct = {shape_token(p) for p in full}
    if len(distinct) > 1:
        return dict(base, status='conflicting-whole-polygons')
    if method == 'whole-first' and full:
        candidate = full[0]
        proof_kind = 'whole-source-witness'
    else:
        candidate, proof_kind = boundary_proposal(observations)
        if full and (candidate is None or not candidate.equals(full[0])):
            candidate, proof_kind = full[0], 'whole-source-witness-after-proposal'
        if candidate is None:
            return dict(base, status=proof_kind)
    for observation in observations:
        if observation.polygon.area > candidate.area:
            return dict(base, status='fragment-larger-than-whole')
        for vertex in real_vertices(observation):
            if not candidate.boundary.covers(Point(vertex)):
                return dict(base, status='incompatible-original-corner')
        for edge in real_edges(observation):
            a, b = list(edge.coords)
            if (not on_window(a, observation.window) and not on_window(b, observation.window)
                    and not candidate.boundary.covers(edge)):
                return dict(base, status='incompatible-original-edge')
    token = shape_token(candidate)
    semantic = (observations[0].namespace, observations[0].attributes, token)
    digest = hashlib.sha256(json.dumps(semantic, separators=(',', ':')).encode()).hexdigest()
    return dict(base, status='ready', proof=proof_kind, shape=token,
                geometry_digest=digest, area=candidate.area)


def assemble(observations, namespace, method='whole-first'):
    if any(o.namespace != namespace for o in observations):
        raise ValueError('mixed declared source snapshots require upstream admission')
    return [resolve(group, method) for group in components(observations)]


def load_exports(paths, buffer_units=64):
    observations = []
    largest_residual = 0.0
    for path in paths:
        value = json.loads(Path(path).read_text())
        extent = value['extent']
        for feature in value['features']:
            z, x, y = feature['tile']
            if feature['source_id_empty'] or feature['revision_empty']:
                raise ValueError('source namespace is not declared')
            namespace = (feature['source_origin'], feature['source_kind'],
                         feature['source_id_sha256'], feature['revision_sha256'],
                         value['schema'], 'buildings', z, extent)
            size = 2 ** z * extent
            outer, holes, ordinal = None, [], None
            def emit():
                if outer is None:
                    return
                polygon = Polygon(outer, holes)
                if not polygon.is_valid or polygon.area <= 0:
                    raise ValueError(('invalid-original-polygon', feature['tile'], feature['id'], ordinal))
                window = (x * extent - buffer_units, y * extent - buffer_units,
                          (x + 1) * extent + buffer_units, (y + 1) * extent + buffer_units)
                if not box(*window).covers(polygon):
                    raise ValueError('source coordinates contradict the declared buffer profile')
                observations.append(Observation(namespace, tuple(feature['tile']), feature['input_digest'],
                                                feature['id'], ordinal, feature['attribute_sha256'], polygon, window))
            for index, ring in enumerate(feature['rings']):
                points = []
                for lat, lon in ring['lat_lon']:
                    a = (lon + 180) / 360 * size
                    b = (1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2 * size
                    residual = max(abs(a - round(a)), abs(b - round(b)))
                    # Inverting known MVT-derived doubles, bounded by floating point ULPs.
                    if residual > 64 * math.ulp(max(abs(a), abs(b), 1.0)):
                        raise ValueError('coordinate is not on the declared source grid')
                    largest_residual = max(largest_residual, residual)
                    points.append((round(a), round(b)))
                if ring['exterior']:
                    emit()
                    outer, holes, ordinal = points, [], index
                elif outer is None:
                    raise ValueError('hole has no owning exterior')
                else:
                    holes.append(points)
            emit()
    return observations, largest_residual

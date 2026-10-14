"""Immutable metric hip-roof partitions with independently checked wavefront geometry."""

from dataclasses import dataclass
import math
import threading
import time as clock

import numpy as np
import shapely
from shapely import affinity

from .scope import scalar

_TRACER_LOCK = threading.Lock()


class SkeletonBudgetExceeded(RuntimeError):
    pass


@dataclass(frozen=True, slots=True)
class SkeletonLimits:
    vertices: int = 256
    nodes: int = 1024
    python_events: int = 4096
    seconds: float = 2.

    def __post_init__(self):
        if any(type(value) is not int or value <= 0 for value in (self.vertices, self.nodes)):
            raise ValueError('Skeleton capacities must be positive integers')
        if type(self.python_events) is not int or self.python_events < 0:
            raise ValueError('Skeleton event capacity must be a nonnegative integer')
        seconds = scalar(self.seconds)
        if seconds <= 0:
            raise ValueError('Skeleton cooperative deadline must be positive seconds')


@dataclass(frozen=True, slots=True)
class HipFacet:
    node_indices: tuple[int, ...]
    boundary: tuple[int, int]
    gradient: tuple[float, float]
    intercept_m: float
    polygon: object


@dataclass(frozen=True, slots=True)
class HipPlan:
    """Local plane coordinates plus origin; distances are wavefront travel in metres.

    Arrays are immutable bytes-backed values. Facet polygons and coefficients
    use positions relative to origin. Lifted vertices use the input plane's XY
    and a third coordinate along its normal; callers own the world-plane frame.
    """
    origin: tuple[float, float]
    footprint: object
    positions_m: object
    distances_m: object
    facets: tuple[HipFacet, ...]
    method: str
    events: int | None
    maximum_refinement_m: float
    fallback_reason: str | None = None

    def factor(self, value, mode='byAngle'):
        value = scalar(value)
        if mode not in ('byAngle', 'byHeight') or value <= 0:
            raise ValueError('Hip roofs require a positive byAngle/byHeight value')
        if mode == 'byAngle' and not 1e-10 <= value < 90:
            raise ValueError('Hip angle must be in [1e-10,90) degrees')
        return value/self.distances_m.max() if mode == 'byHeight' else math.tan(math.radians(value))

    def lifted_vertices(self, value, mode='byAngle'):
        factor = self.factor(value, mode)
        return np.c_[self.positions_m+np.asarray(self.origin), self.distances_m*factor]


def immutable_array(values):
    array = np.ascontiguousarray(values, dtype=np.float64)
    return np.frombuffer(array.tobytes(), dtype=np.float64).reshape(array.shape)


def input_data(polygon, limits):
    if not isinstance(polygon, shapely.Polygon) or polygon.is_empty or not polygon.is_valid or polygon.has_z:
        raise ValueError('Hip planning requires one valid nonempty XY polygon')
    original = shapely.orient_polygons(polygon)
    rings = [np.asarray(ring.coords)[:-1] for ring in (original.exterior, *original.interiors)]
    if not all(np.isfinite(ring).all() for ring in rings):
        raise ValueError('Hip input is nonfinite')
    if sum(map(len, rings)) > limits.vertices:
        raise SkeletonBudgetExceeded('Hip input exceeds the vertex budget')
    origin = rings[0][0].copy(); rings = [ring-origin for ring in rings]
    local = shapely.Polygon(rings[0], rings[1:])
    if local.area <= 1e-10:
        raise ValueError('Hip footprint has no usable area')
    boundaries = {}
    for number, ring in enumerate(rings):
        for index, (a, b) in enumerate(zip(ring, np.roll(ring, -1, axis=0), strict=True)):
            edge = b-a; length = np.linalg.norm(edge)
            if length <= 1e-10:
                raise ValueError('Hip boundary has a zero-length edge')
            gradient = np.array([-edge[1], edge[0]])/length
            boundaries[frozenset((tuple(a), tuple(b)))] = ((number, index), gradient, -float(a@gradient))
    return origin, local, rings, boundaries


def rectangular(rings):
    if len(rings) != 1 or len(rings[0]) != 4:
        return False
    edges = np.roll(rings[0], -1, axis=0)-rings[0]
    lengths = np.linalg.norm(edges, axis=1)
    unit = edges/lengths[:, None]
    return bool(np.all(abs(np.einsum('ij,ij->i', unit, np.roll(unit, -1, axis=0))) <= 1e-12)
                and np.allclose(edges[:2], -edges[2:], atol=1e-10, rtol=1e-12))


def rectangle_data(rings):
    if not rectangular(rings):
        raise ValueError('Analytic hip construction requires a four-edge rectangle')
    points = rings[0]; a, b = points[1]-points[0], points[3]-points[0]
    width, depth = np.linalg.norm(a), np.linalg.norm(b)
    u, v = a/width, b/depth; radius = min(width, depth)/2.
    peaks = ([points[0]+u*radius+v*radius, points[0]+u*(width-radius)+v*radius] if width >= depth
             else [points[0]+u*radius+v*radius, points[0]+u*radius+v*(depth-radius)])
    if np.linalg.norm(peaks[1]-peaks[0]) <= 1e-12:
        peaks = peaks[:1]
    nodes = np.r_[points, peaks]; distances = np.r_[np.zeros(4), np.full(len(peaks), radius)]
    lookup = {tuple(point): index for index, point in enumerate(nodes)}; faces = []
    for index in range(4):
        first, last = points[index], points[(index+1) % 4]
        edge = last-first; normal = np.array([-edge[1], edge[0]])/np.linalg.norm(edge)
        selected = [point for point in peaks if abs((point-first)@normal-radius) <= 1e-10]
        facet = shapely.MultiPoint([first, last, *selected]).convex_hull
        faces.append(tuple(lookup[tuple(point)] for point in np.asarray(facet.exterior.coords)[:-1]))
    return nodes, distances, tuple(faces), 0


def skeleton_data(rings, limits, began):
    try:
        from py_straight_skeleton import algorithm
    except ModuleNotFoundError as error:
        raise ModuleNotFoundError('Skeleton roofs require the optional outshine-reference[roofs] extra') from error

    class Tracer(algorithm.NoopAlgorithmTracer):
        steps = 0

        def on_step_begin(self, step, time, algorithm):
            self.steps = step
            if step > limits.python_events:
                raise SkeletonBudgetExceeded('Skeleton event budget exceeded')
            if clock.perf_counter()-began > limits.seconds:
                raise SkeletonBudgetExceeded('Skeleton cooperative time budget exceeded')

    remaining = limits.seconds-(clock.perf_counter()-began)
    if remaining <= 0 or not _TRACER_LOCK.acquire(timeout=remaining):
        raise SkeletonBudgetExceeded('Skeleton tracer acquisition exceeded the cooperative budget')
    previous = algorithm.GLOBAL_ALGORITHM_TRACER; tracer = Tracer()
    try:
        algorithm.set_global_algorithm_tracer(tracer)
        try:
            result = algorithm.StraightSkeletonAlgorithm().compute_skeleton(rings[0].tolist(), [ring.tolist() for ring in rings[1:]])
            if len(result.nodes) > limits.nodes:
                raise SkeletonBudgetExceeded('Skeleton node budget exceeded')
            nodes = np.array([[node.position.x, node.position.y] for node in result.nodes])
            distances = np.array([node.time for node in result.nodes])
            faces = tuple(tuple(face[:-1] if face[0] == face[-1] else face) for face in result.get_faces())
        except SkeletonBudgetExceeded:
            raise
        except (RuntimeError, ArithmeticError) as error:
            raise ValueError('Skeleton backend rejected the input: '+str(error)) from error
    finally:
        algorithm.set_global_algorithm_tracer(previous); _TRACER_LOCK.release()
    return nodes, distances, faces, tracer.steps


def face_boundaries(nodes, faces, boundaries):
    owners = []
    for face in faces:
        if len(face) < 3 or any(type(index) is not int or not 0 <= index < len(nodes) for index in face):
            raise ValueError('Skeleton facet has invalid node indices')
        matches = [boundaries[key] for a, b in zip(face, face[1:]+face[:1])
                   if (key := frozenset((tuple(nodes[a]), tuple(nodes[b])))) in boundaries]
        if len(matches) != 1:
            raise ValueError('Skeleton facet has no unique original boundary edge')
        owners.append(matches[0])
    if len(owners) != len(boundaries) or len({owner[0] for owner in owners}) != len(boundaries):
        raise ValueError('Skeleton facets do not represent every source edge exactly once')
    return owners


def refine(nodes, distances, faces, owners):
    refined = nodes.copy(); heights = distances.copy(); incident = [[] for _ in nodes]
    for face, (_, gradient, intercept) in zip(faces, owners, strict=True):
        for index in face:
            incident[index].append((gradient, intercept))
    for index, constraints in enumerate(incident):
        if heights[index] == 0:
            continue
        matrix = np.array([[*gradient, -1.] for gradient, _ in constraints])
        right = -np.array([intercept for _, intercept in constraints])
        solution, _, rank, _ = np.linalg.lstsq(matrix, right, rcond=None)
        if rank < 2 or np.max(abs(matrix@solution-right)) > 1e-7:
            raise ValueError('Skeleton ridge topology has incompatible edge-plane constraints')
        if rank == 2:
            candidates = np.array([nodes[index]@gradient+intercept for gradient, intercept in constraints])
            if np.ptp(candidates) > 1e-7:
                raise ValueError('Collinear skeleton facets disagree at their shared node')
            heights[index] = float(candidates.mean())
        else:
            refined[index], heights[index] = solution[:2], solution[2]
    displacement = float(np.linalg.norm(refined-nodes, axis=1).max())
    if displacement > 5e-5 or not np.isfinite(refined).all() or not np.isfinite(heights).all() or heights.min() < -1e-8:
        raise ValueError('Skeleton refinement exceeds its metric correction contract')
    heights = np.maximum(heights, 0.)
    return refined, heights, displacement


def validate(local, nodes, distances, faces, owners):
    if not len(nodes) or not np.isfinite(nodes).all() or not np.isfinite(distances).all() or distances.max() <= 1e-10:
        raise ValueError('Skeleton has no finite positive rise')
    facets, edges = [], {}
    for face, (boundary, gradient, intercept) in zip(faces, owners, strict=True):
        polygon = shapely.Polygon(nodes[list(face)])
        if not polygon.is_valid or polygon.area <= 1e-10:
            raise ValueError('Skeleton produced an invalid facet')
        if np.max(abs(nodes[list(face)]@gradient+intercept-distances[list(face)])) > 1e-7:
            raise ValueError('Skeleton nodes do not lie on their source-edge height planes')
        facets.append(HipFacet(face, boundary, tuple(gradient), intercept, polygon))
        for a, b in zip(face, face[1:]+face[:1]):
            edges.setdefault(tuple(sorted((a, b))), []).append(boundary)
    if any(len(group) not in (1, 2) for group in edges.values()):
        raise ValueError('Skeleton facets have a nonmanifold ridge')
    for (a, b), group in edges.items():
        if len(group) == 1 and (distances[a] != 0 or distances[b] != 0):
            raise ValueError('Skeleton ridge has an open boundary')
    union = shapely.union_all([facet.polygon for facet in facets]); tolerance = max(1e-7, local.area*1e-10)
    if union.symmetric_difference(local).area > tolerance or sum(f.polygon.area for f in facets)-union.area > tolerance:
        raise ValueError('Skeleton facets leave gaps, overlap or fill a courtyard')
    return tuple(facets)



def native_data(local, rings, boundaries, limits):
    if local.interiors:
        from .skeleton_graph import graph_data
        data, _ = graph_data(local, rings, limits.nodes)
        return data, face_boundaries(data[0], data[2], boundaries)
    from .skeleton_native import partition
    nodes, faces = partition(local, rings, limits.nodes)
    owners = face_boundaries(nodes, faces, boundaries); incident = [[] for _ in nodes]
    for face, (_, gradient, intercept) in zip(faces, owners, strict=True):
        for index in face:
            incident[index].append(nodes[index]@gradient+intercept)
    original = {tuple(point) for ring in rings for point in ring}
    distances = np.array([0. if tuple(point) in original else np.mean(heights)
                          for point, heights in zip(nodes, incident, strict=True)])
    return (nodes, distances, faces, None), owners


def finish(origin, local, boundaries, data, method, limits, began, fallback=None, owners=None):
    nodes, distances, faces, events = data
    if len(nodes) > limits.nodes:
        raise SkeletonBudgetExceeded('Hip node budget exceeded')
    if owners is None:
        owners = face_boundaries(nodes, faces, boundaries)
    nodes, distances, correction = refine(nodes, distances, faces, owners)
    facets = validate(local, nodes, distances, faces, owners)
    if clock.perf_counter()-began > limits.seconds:
        raise SkeletonBudgetExceeded('Hip preparation exceeded the cooperative deadline')
    return HipPlan(tuple(origin), local, immutable_array(nodes), immutable_array(distances), facets, method, events, correction, fallback)


def prepare_hip(polygon, method='auto', limits=SkeletonLimits()):
    """Prepare immutable facets; lifting never repeats skeleton construction.

    Auto tries strict native ownership first, then Python if native validation
    rejects it. Rectangles avoid both backends. Python event limits do not
    measure native events; native deadlines are checked after the call. Calls
    belong in cancellable workers, not a frame. Sources never get simplified.
    """
    if method not in ('auto', 'skeleton', 'rectangle') or not isinstance(limits, SkeletonLimits):
        raise ValueError('Unknown hip planning method or limits')
    began = clock.perf_counter(); origin, local, rings, boundaries = input_data(polygon, limits)
    if method == 'rectangle' or method == 'auto' and rectangular(rings):
        return finish(origin, local, boundaries, rectangle_data(rings), 'rectangle', limits, began)
    fallback = None
    if method == 'auto':
        try:
            data, owners = native_data(local, rings, boundaries, limits)
            return finish(origin, local, boundaries, data, 'native', limits, began, owners=owners)
        except ValueError as error:
            fallback = str(error)
    if clock.perf_counter()-began > limits.seconds:
        raise SkeletonBudgetExceeded('No cooperative time budget remains for the Python solver')
    data = skeleton_data(rings, limits, began)
    return finish(origin, local, boundaries, data, 'skeleton', limits, began, fallback)

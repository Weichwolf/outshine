"""Tessellate rational patches on affine parameter isolines; keep whole source ports."""

import itertools
import time
from dataclasses import dataclass
from dataclasses import field as data_field

import numpy as np
from scipy.spatial import cKDTree

from ._budget import PortLimit


class SliceLimit(PortLimit):
    pass


def parameter_terms(model, xy, part, tolerance):
    normal = model.inward_normals[part]
    signed = np.einsum("nki,ki->nk", xy[:, None] - model.cut_centres[part], normal)
    d = signed.sum(axis=1) / model.scales[part]
    if np.any(d <= 1e-8):
        raise ValueError("positive rational slice denominator required")
    u = signed[:, 0] / model.scales[part]
    parameter = u / d
    parameter[abs(signed[:, 0]) <= tolerance] = 0.0
    parameter[abs(signed[:, 1]) <= tolerance] = 1.0
    return u, d, parameter


def initial_knots(model, mesh, segments=8, *, max_stations=512):
    tolerance = 64 * np.finfo(float).eps * max(1, abs(mesh["xy"]).max())
    output = {}
    for part in np.unique(mesh["parts"][mesh["parts"] >= 0]):
        if model.plane_ids[part, 0] == model.plane_ids[part, 1]:
            continue
        vertices = np.unique(mesh["faces"][mesh["parts"] == part])
        _, _, t = parameter_terms(model, mesh["xy"][vertices], part, tolerance)
        knots = np.unique(np.r_[t, np.linspace(t.min(), t.max(), segments + 1)])
        epsilon = 64 * np.finfo(float).eps * max(1, abs(knots).max())
        output[int(part)] = knots[np.r_[True, np.diff(knots) > epsilon]]
        if len(output[int(part)]) > max_stations:
            raise SliceLimit("port_part_station_budget")
    return output


@dataclass
class SliceBuilder:
    model: object
    original_xy: np.ndarray
    original_targets: np.ndarray
    max_vertices: int = 131072
    max_faces: int = 262144
    max_weld_pairs: int = 1048576
    xy: list = data_field(init=False)
    targets: list = data_field(init=False)
    nodes: dict = data_field(default_factory=dict)
    edge_nodes: dict = data_field(default_factory=dict)
    terms: dict = data_field(default_factory=dict)

    def __post_init__(self):
        self.xy = list(self.original_xy)
        self.targets = list(self.original_targets)
        self.tolerance = 64 * np.finfo(float).eps * max(1, abs(self.original_xy).max())

    def term(self, part, vertex):
        key = int(part), int(vertex)
        if key not in self.terms:
            u, d, t = parameter_terms(
                self.model, self.original_xy[[vertex]], part, self.tolerance
            )
            self.terms[key] = float(u[0]), float(d[0]), float(t[0])
        return self.terms[key]

    def crossing(self, a, b, part, level):
        if a[1] == level:
            return a
        if b[1] == level:
            return b
        support = a[2] | b[2]
        if len(support) != 2:
            raise ValueError(
                f"parameter isolines intersect inside part {part}: {a}, {b}, level {level}"
            )
        first, last = sorted(support)
        key = first, last, int(part), float(level)
        if key not in self.nodes:
            ua, da, _ = self.term(part, first)
            ub, db, _ = self.term(part, last)
            denominator = ub - ua - level * (db - da)
            if denominator == 0:
                raise ValueError("parallel slice crossing cannot be represented")
            fraction = float(np.clip((level * da - ua) / denominator, 0, 1))
            point = self.original_xy[first] + fraction * (
                self.original_xy[last] - self.original_xy[first]
            )
            if np.linalg.norm(point - self.original_xy[first]) <= self.tolerance:
                vertex = first
            elif np.linalg.norm(point - self.original_xy[last]) <= self.tolerance:
                vertex = last
            else:
                if len(self.xy) >= self.max_vertices:
                    raise SliceLimit("slice_vertex_budget")
                vertex = len(self.xy)
                self.xy.append(point)
                self.targets.append(
                    self.original_targets[first]
                    + fraction
                    * (self.original_targets[last] - self.original_targets[first])
                )
                self.edge_nodes.setdefault((first, last), []).append((fraction, vertex))
            self.nodes[key] = vertex
        vertex = self.nodes[key]
        return (
            vertex,
            float(level),
            frozenset([vertex]) if vertex in support else support,
        )

    def clip(self, polygon, part, level, above):
        output = []
        for a, b in zip(polygon, polygon[1:] + polygon[:1]):
            inside_a = a[1] >= level if above else a[1] <= level
            inside_b = b[1] >= level if above else b[1] <= level
            if inside_a:
                output.append(a)
            if inside_a != inside_b:
                output.append(self.crossing(a, b, part, level))
        return [p for i, p in enumerate(output) if p[0] != output[i - 1][0]]

    def triangle(self, corners, part, knots):
        polygon = [
            (int(v), self.term(part, v)[2], frozenset([int(v)])) for v in corners
        ]
        t = [p[1] for p in polygon]
        epsilon = 64 * np.finfo(float).eps * max(1, max(abs(v) for v in t))
        if max(t) - min(t) <= epsilon:
            return [tuple(corners)], [(min(t), max(t))]
        first = max(0, np.searchsorted(knots, min(t), side="right") - 1)
        last = min(len(knots) - 1, np.searchsorted(knots, max(t), side="left"))
        faces, intervals = [], []
        for lo, hi in zip(knots[first:last], knots[first + 1 : last + 1]):
            sliced = self.clip(polygon, part, lo, True)
            sliced = self.clip(sliced, part, hi, False) if sliced else []
            if len(sliced) < 3:
                continue
            for index in range(1, len(sliced) - 1):
                face = (sliced[0][0], sliced[index][0], sliced[index + 1][0])
                if len(set(face)) == 3:
                    faces.append(face)
                    intervals.append((lo, hi))
        return faces, intervals

    def conform_original(self, corners):
        faces = [tuple(map(int, corners))]
        for a, b in zip(corners, np.roll(corners, -1)):
            key = tuple(sorted((int(a), int(b))))
            specs = sorted(set(self.edge_nodes.get(key, [])))
            points = [
                v for _, v in (specs if a < b else specs[::-1]) if v not in (a, b)
            ]
            if not points:
                continue
            owner = next(i for i, f in enumerate(faces) if a in f and b in f)
            opposite = next(v for v in faces[owner] if v not in (a, b))
            chain = [int(a), *points, int(b)]
            faces[owner : owner + 1] = [
                (u, v, opposite) for u, v in itertools.pairwise(chain) if u != v
            ]
        return faces


def weld_vertices(xy, protected, original_count, tolerance, maximum=1048576):
    tree = cKDTree(xy)
    count = (int(tree.count_neighbors(tree, tolerance)) - len(xy)) // 2
    if count > maximum:
        raise SliceLimit("slice_weld_pair_budget")
    pairs = tree.query_pairs(tolerance, output_type="ndarray")
    parents = np.arange(len(xy))

    def root(i):
        while parents[i] != i:
            parents[i] = parents[parents[i]]
            i = parents[i]
        return int(i)

    skipped = 0
    for a, b in pairs:
        a, b = root(a), root(b)
        if a == b:
            continue
        if protected[a] and protected[b]:
            skipped += 1
            continue
        chosen = a if protected[a] else b if protected[b] else min(a, b)
        parents[b if chosen == a else a] = chosen
    labels = np.array([root(i) for i in range(len(xy))])
    displacement = np.linalg.norm(xy - xy[labels], axis=1)
    if displacement.max(initial=0) > tolerance:
        raise ValueError("slice rounding weld exceeds its metric contract")
    return labels, {
        "numeric_weld_m": float(displacement.max(initial=0)),
        "weld_pairs": count,
        "distinct_protected_vertices_kept": skipped,
        "original_vertices_remapped": int(
            np.count_nonzero(labels[:original_count] != np.arange(original_count))
        ),
    }


def finish_mesh(builder, original, product, captured, faces, parents, intervals):
    xy = np.asarray(builder.xy)
    targets = np.asarray(builder.targets)
    protected = np.zeros(len(xy), bool)
    protected[np.unique(captured["road_vertex_map"][product["port_edges"]])] = True
    protected[
        np.setdiff1d(np.arange(len(original["xy"])), captured["road_vertex_map"])
    ] = True
    labels, report = weld_vertices(
        xy, protected, len(original["xy"]), builder.tolerance, builder.max_weld_pairs
    )
    faces = labels[np.asarray(faces, int)]
    parents, intervals = np.asarray(parents, int), np.asarray(intervals, float)
    corners = xy[faces]
    a, b = corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0]
    area = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    keep = (area > 0) & np.all(np.diff(np.sort(faces, axis=1), axis=1) != 0, axis=1)
    report["numerically_collapsed_faces"] = int(np.count_nonzero(~keep))
    report["collapsed_area_m2"] = float(abs(area[~keep]).sum() * 0.5)
    if report["collapsed_area_m2"] > builder.tolerance * max(
        1, np.linalg.norm(a, axis=1).sum()
    ):
        raise ValueError("slice normalization would remove resolvable source area")
    faces, parents, intervals = faces[keep], parents[keep], intervals[keep]
    _keys, first, inverse, counts = np.unique(
        np.sort(faces, axis=1),
        axis=0,
        return_index=True,
        return_inverse=True,
        return_counts=True,
    )
    if np.any(counts > 1):
        for key in np.flatnonzero(counts > 1):
            owners = parents[inverse == key]
            if np.ptp(original["parts"][owners]) != 0:
                raise ValueError("rounding weld overlaps different original modules")
            if np.any(original["frames"][owners] != original["frames"][owners[0]]):
                raise ValueError("rounding weld cannot discard competing grade frames")
        faces, parents, intervals = faces[first], parents[first], intervals[first]
    report["duplicate_faces_removed"] = int(len(inverse) - len(faces))
    if len(np.unique(parents)) != len(original["faces"]):
        raise SliceLimit("port_original_face_unresolved")
    used, new_faces = np.unique(faces, return_inverse=True)
    lookup = np.full(len(xy), -1, int)
    lookup[used] = np.arange(len(used))
    source_map = lookup[labels[: len(original["xy"])]]
    if np.any(source_map < 0):
        raise ValueError("numeric slice normalization lost an original vertex anchor")
    ports = {
        tuple(sorted(source_map[list(edge)])): owner
        for edge, owner in original["port_roots"].items()
    }
    mesh = {
        "xy": xy[used],
        "faces": new_faces.reshape(-1, 3),
        "parts": original["parts"][parents],
        "frames": original["frames"][parents],
        "targets": targets[used],
        "roots": original["roots"][parents],
        "port_roots": ports,
        "original_vertex_map": source_map,
        "intervals": intervals,
    }
    report.update(
        vertices=len(used), faces=len(faces), weld_tolerance_m=builder.tolerance
    )
    return mesh, report


def slice_mesh(
    model,
    original,
    product,
    captured,
    knots,
    *,
    seconds=15,
    max_vertices=131072,
    max_faces=262144,
    max_weld_pairs=1048576,
):
    if not np.isfinite(seconds) or seconds < 0:
        raise ValueError("finite nonnegative slice time required")
    began = time.monotonic()
    if len(original["xy"]) > max_vertices or len(original["faces"]) > max_faces:
        raise SliceLimit("port_output_budget")
    builder = SliceBuilder(
        model,
        original["xy"],
        original["targets"],
        max_vertices,
        max_faces,
        max_weld_pairs,
    )
    output, parents, intervals, deferred = [], [], [], []
    for face, (corners, part) in enumerate(zip(original["faces"], original["parts"])):
        if face % 256 == 0 and time.monotonic() - began > seconds:
            raise SliceLimit("slice_time_budget")
        if part not in knots:
            deferred.append(face)
            continue
        generated, ranges = builder.triangle(corners, int(part), knots[part])
        output.extend(generated)
        parents.extend([face] * len(generated))
        intervals.extend(ranges)
        if len(output) > builder.max_faces:
            raise SliceLimit("slice_face_budget")
    for index, face in enumerate(deferred):
        if index % 256 == 0 and time.monotonic() - began > seconds:
            raise SliceLimit("slice_time_budget")
        generated = builder.conform_original(original["faces"][face])
        output.extend(generated)
        parents.extend([face] * len(generated))
        intervals.extend([(np.nan, np.nan)] * len(generated))
        if len(output) > builder.max_faces:
            raise SliceLimit("slice_face_budget")
    mesh, report = finish_mesh(
        builder, original, product, captured, output, parents, intervals
    )
    report.update(
        geometry_ms=(time.monotonic() - began) * 1000,
        parameter_stations=sum(len(v) for v in knots.values()),
        scope="Original whole ports protected; other indexed geometry normalized only inside 64*eps*coordinate scale. Every original vertex has a recorded anchor map; source inputs remain unchanged.",
    )
    return mesh, report

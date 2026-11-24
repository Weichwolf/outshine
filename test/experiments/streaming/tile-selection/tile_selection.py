"""Exercise a coverage-preserving tile cut with cached product bounds."""

import argparse
import json
import math
import sqlite3
from collections import defaultdict
from dataclasses import dataclass, field
from itertools import product
from pathlib import Path
from time import perf_counter
from xml.etree import ElementTree

import numpy as np


@dataclass
class Tile:
    key: tuple
    low: np.ndarray
    high: np.ndarray
    error_m: float
    boxes: int
    children: tuple
    centre: tuple = field(init=False)
    half: tuple = field(init=False)

    def __post_init__(self):
        self.centre = tuple(map(float, (self.low + self.high) * 0.5))
        self.half = tuple(map(float, (self.high - self.low) * 0.5))


def parent_of(key):
    level, column, row = key
    return level + 1, column // 2, row // 2


def bounds_tree(boxes, leaf_m, levels):
    """Derive box proxies directly from cached bounds with a matching-corner error."""
    groups = defaultdict(list)
    for box in boxes:
        centre = np.mean(box, axis=0)
        column, row = (math.floor(centre[axis] / leaf_m) for axis in (0, 2))
        groups[0, column, row].append(box)
    children_by_key = {}
    for level in range(levels):
        following = defaultdict(list)
        parents = defaultdict(list)
        built = []
        for key, members in groups.items():
            originals = np.asarray(members)
            spacing = (leaf_m << level) / 16
            proxies = np.rint(originals / spacing) * spacing if level else originals
            displacement = np.abs(proxies - originals).max(axis=1)
            error = float(np.linalg.norm(displacement, axis=-1).max())
            low = np.minimum(originals[:, 0], proxies[:, 0]).min(axis=0)
            high = np.maximum(originals[:, 1], proxies[:, 1]).max(axis=0)
            children = tuple(children_by_key.get(key, ()))
            for child in children:
                low = np.minimum(low, child.low)
                high = np.maximum(high, child.high)
            boxes_at_level = len(np.unique(proxies.reshape(-1, 6), axis=0))
            tile = Tile(key, low, high, error, boxes_at_level, children)
            built.append(tile)
            if level + 1 < levels:
                address = parent_of(key)
                following[address].extend(members)
                parents[address].append(tile)
        if not following:
            return tuple(built)
        groups, children_by_key = following, parents
    raise AssertionError("tree requires a positive number of levels")


@dataclass
class Camera:
    eye: np.ndarray
    axes: np.ndarray
    focal_px: float
    slopes: np.ndarray
    near_m: float
    radius_m: float

    planes: tuple = field(init=False)
    positive_planes: tuple = field(init=False)

    def __post_init__(self):
        self.planes = tuple(tuple(map(float, row)) for row in self.axes.T)
        self.positive_planes = tuple(tuple(map(abs, row)) for row in self.planes)

    def error_pixels(self, tile):
        offset = tuple(float(a - b) for a, b in zip(tile.centre, self.eye))
        if (
            math.hypot(*(max(0.0, abs(d) - h) for d, h in zip(offset, tile.half)))
            > self.radius_m
        ):
            return None
        # Each support interval encloses the AABB's projection onto this camera axis.
        x, y, z = (_dot(offset, axis) for axis in self.planes)
        dx, dy, dz = (_dot(tile.half, axis) for axis in self.positive_planes)
        far = z + dz
        if (
            far < self.near_m
            or abs(x) - dx > far * self.slopes[0]
            or abs(y) - dy > far * self.slopes[1]
        ):
            return None
        depth = z - dz
        if depth <= max(self.near_m, tile.error_m):
            return math.inf
        radial = math.hypot(abs(x) + dx, abs(y) + dy) / depth
        # The bound follows from (xy + dxy)/(z + dz) - xy/z.
        return (
            self.focal_px
            * tile.error_m
            * math.hypot(1, radial)
            / (depth - tile.error_m)
        )


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


@dataclass
class Cut:
    drawn: list
    requests: set
    uncovered: set
    visited: int = 0


def select(roots, camera, threshold, resident):
    """Keep a resident parent until the full visible child cut can replace it."""
    result = Cut([], set(), set())

    def visit(tile):
        result.visited += 1
        error = camera.error_pixels(tile)
        if error is None:
            return [], set()
        held = tile.key in resident
        if tile.children and (error > threshold or not held):
            drawn, missing = [], set()
            for child in tile.children:
                child_tiles, child_missing = visit(child)
                drawn.extend(child_tiles)
                missing.update(child_missing)
            if not missing:
                if error <= threshold:
                    result.requests.add(tile.key)
                return drawn, missing
            if not held:
                result.requests.add(tile.key)
                return drawn, missing
        if held:
            return [tile], set()
        result.requests.add(tile.key)
        return [], {tile.key}

    for root in roots:
        drawn, missing = visit(root)
        result.drawn.extend(drawn)
        result.uncovered.update(missing)
    return result


def all_tiles(roots):
    for tile in roots:
        yield tile
        yield from all_tiles(tile.children)


def local_frame(latitude_deg, longitude_deg):
    latitude, longitude = np.radians((latitude_deg, longitude_deg))
    sin_lat, cos_lat = math.sin(latitude), math.cos(latitude)
    sin_lon, cos_lon = math.sin(longitude), math.cos(longitude)
    up = np.asarray((cos_lat * cos_lon, cos_lat * sin_lon, sin_lat))
    east = (-sin_lon, cos_lon, 0)
    south = (sin_lat * cos_lon, sin_lat * sin_lon, -cos_lat)
    eccentricity2 = 1 / 298.257223563 * (2 - 1 / 298.257223563)
    normal_radius = 6378137 / math.sqrt(1 - eccentricity2 * sin_lat**2)
    anchor = normal_radius * up * (1, 1, 1 - eccentricity2)
    return anchor, np.column_stack((east, up, south))


def cached_bounds(database, anchor, axes, radius_m):
    where = (
        "SELECT DISTINCT a.minx,a.miny,a.minz,a.maxx,a.maxy,a.maxz FROM bounds AS b "
        "JOIN assets AS a ON a.id=b.id WHERE a.kind='building-static-lod' AND a.level=1 "
        "AND b.minx<=? AND b.maxx>=? AND b.miny<=? AND b.maxy>=? AND b.minz<=? AND b.maxz>=?"
    )
    search = tuple(
        part
        for coordinate in anchor
        for part in (coordinate + radius_m, coordinate - radius_m)
    )
    with sqlite3.connect(
        database.resolve().as_uri() + "?mode=ro", uri=True
    ) as connection:
        rows = np.asarray(
            connection.execute(where, search).fetchall(), dtype=np.float64
        )
    if not len(rows):
        raise ValueError("no cached medium building bounds at this camera")
    centres = np.einsum("ni,ij->nj", (rows[:, :3] + rows[:, 3:]) * 0.5 - anchor, axes)
    extents = np.einsum("ni,ij->nj", (rows[:, 3:] - rows[:, :3]) * 0.5, abs(axes))
    return np.stack((centres - extents, centres + extents), axis=1)


def camera_samples(motion, profile, near_m, radius_m):
    view = np.asarray(motion["start"]["world_from_camera"]).reshape(4, 4)
    orientation = view[:3, :3].T * (1, 1, -1)
    eye = np.asarray(motion["start"]["position_m"])
    velocity = np.asarray(motion["linear_velocity_mps"])
    yaw_radians = math.radians(motion["angular_velocity_deg_s"][1])
    slope = math.tan(math.radians(motion["start"]["perspective_fov_deg"]) * 0.5)
    slopes = np.asarray((slope * profile["width_px"] / profile["height_px"], slope))
    for ratio in np.linspace(0, 1, 13):
        time_s = ratio * motion["frames"] * motion["time_step_s"]
        sine, cosine = math.sin(yaw_radians * time_s), math.cos(yaw_radians * time_s)
        turn = np.asarray(((cosine, 0, sine), (0, 1, 0), (-sine, 0, cosine)))
        yield Camera(
            eye + velocity * time_s,
            turn @ orientation,
            profile["height_px"] / (2 * slope),
            slopes,
            near_m,
            radius_m,
        )


def verify_projection(boxes):
    """Compare the analytic bound with corresponding corner projections."""
    cases = np.asarray(list(product((-1.0, 1.0), repeat=3)))
    generator = np.random.default_rng(1958)
    for box in boxes[:240]:
        low, high = box
        centre = (low + high) * 0.5
        points = centre + (high - low) * 0.5 * cases
        shifts = generator.uniform(-1, 1, points.shape)
        error = float(np.linalg.norm(shifts, axis=1).max())
        camera = Camera(
            centre - (0, 0, float(high[2] - low[2]) + 10),
            np.eye(3),
            624,
            np.asarray((1.0, 1.0)),
            0.05,
            240000,
        )
        original = points - camera.eye
        displaced = original + shifts
        delta = original[:, :2] / original[:, 2:] - displaced[:, :2] / displaced[:, 2:]
        threshold = camera.error_pixels(Tile((0, 0, 0), low, high, error, 1, ()))
        assert threshold is not None
        assert np.linalg.norm(delta, axis=1).max() * camera.focal_px <= threshold


def verify_coverage(baseline, candidate, maximum_level):
    assert not candidate.uncovered
    drawn = {tile.key for tile in candidate.drawn}
    assert len(drawn) == len(candidate.drawn)
    for tile in baseline.drawn:
        matches = 0
        address = tile.key
        while address[0] <= maximum_level:
            matches += address in drawn
            address = parent_of(address)
        assert matches == 1


def replay(roots, report, radius_m, threshold):
    nodes = tuple(all_tiles(roots))
    resident = {tile.key for tile in nodes}
    root_keys = {tile.key for tile in roots}
    leaves = {tile.key for tile in nodes if not tile.children}
    motion, profile = report["motion"], report["profile"]
    result = []
    for camera in camera_samples(motion, profile, motion["start"]["near_m"], radius_m):
        baseline = select(roots, camera, -1, resident)
        start = perf_counter()
        target = select(roots, camera, threshold, resident)
        selection_ms = (perf_counter() - start) * 1000
        delayed = select(roots, camera, threshold, resident - leaves | root_keys)
        zooming_out = select(roots, camera, math.inf, resident - root_keys)
        assert not baseline.uncovered
        for cut in (target, delayed, zooming_out):
            verify_coverage(baseline, cut, roots[0].key[0])
        assert all(camera.error_pixels(tile) is not None for tile in target.drawn)
        result.append(
            (
                len(baseline.drawn),
                len(target.drawn),
                len(delayed.drawn),
                target.visited,
                len(delayed.requests),
                sum(tile.boxes for tile in baseline.drawn),
                sum(tile.boxes for tile in target.drawn),
                selection_ms,
            )
        )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--database", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--radius-m", type=float, default=20000)
    parser.add_argument("--pixel-error", type=float, default=2.0)
    options = parser.parse_args()
    if min(options.radius_m, options.pixel_error) <= 0:
        parser.error("radius and pixel error must be positive")
    world = ElementTree.parse(options.scenario).getroot().find("world")
    anchor, axes = local_frame(float(world.attrib["lat"]), float(world.attrib["lon"]))
    boxes = cached_bounds(options.database, anchor, axes, options.radius_m)
    verify_projection(boxes)
    start = perf_counter()
    roots = bounds_tree(boxes, 128, 8)
    construction_ms = (perf_counter() - start) * 1000
    samples = replay(
        roots,
        json.loads(options.report.read_text()),
        float(world.attrib["sightM"]),
        options.pixel_error,
    )
    medians = np.median(samples, axis=0).tolist()
    print(
        json.dumps(
            {
                "cached_unique_medium_building_bounds": len(boxes),
                "sampled_poses": len(samples),
                "construction_ms": construction_ms,
                "median": dict(
                    zip(
                        (
                            "leaf_tiles",
                            "selected_tiles",
                            "held_parent_tiles",
                            "visited_nodes",
                            "delayed_requests",
                            "leaf_boxes",
                            "selected_boxes",
                            "selection_ms",
                        ),
                        medians,
                    )
                ),
            }
        )
    )


if __name__ == "__main__":
    main()

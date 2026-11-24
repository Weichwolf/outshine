"""Compare grouped building extrusions against their source silhouettes."""

import argparse
import gzip
import json
import math
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from time import perf_counter

import numpy as np
from PIL import Image, ImageDraw
from shapely import intersection, make_valid, simplify, union_all
from shapely.geometry import MultiPolygon, Polygon, box

EARTH_RADIUS_M = 6378137.0
WIDTH, HEIGHT = 1280, 720


@dataclass(frozen=True)
class Extrusion:
    footprint: Polygon
    bottom_m: float
    top_m: float


def polygons(geometry):
    if geometry.is_empty:
        return
    if isinstance(geometry, Polygon):
        yield geometry
    elif isinstance(geometry, MultiPolygon) or hasattr(geometry, "geoms"):
        for part in geometry.geoms:
            yield from polygons(part)


def rings_with_holes(rings):
    shell, holes = None, []
    for ring in rings:
        area = sum(x * v - y * u for (x, y), (u, v) in zip(ring, ring[1:] + ring[:1]))
        if len(ring) < 3 or area == 0:
            continue
        if area > 0:
            if shell is not None:
                yield shell, holes
            shell, holes = ring, []
        elif shell is not None:
            holes.append(ring)
    if shell is not None:
        yield shell, holes


def read_source(path):
    with gzip.open(path, "rt") as source:
        data = json.load(source)
    latitude = math.radians(float(data["anchor"]["lat"]))
    longitude = float(data["anchor"]["lon"])
    extent = 1 << data["z"]
    metres = 2 * math.pi * EARTH_RADIUS_M * math.cos(latitude) / extent
    origin_x = (longitude + 180) / 360 * extent
    origin_y = (1 - math.asinh(math.tan(latitude)) / math.pi) * extent / 2
    for tile in data["tiles"]:
        x, z = (tile["x"] - origin_x) * metres, (tile["y"] - origin_y) * metres
        clip = box(x, z, x + metres, z + metres)
        for feature in tile["buildings"]:
            top = feature["height_m"]
            bottom = float(feature["base_m"] or 0)
            if top is None or not 0 <= bottom < top:
                continue
            pitch = metres / feature["extent"]

            def position(ring, x=x, z=z, pitch=pitch):
                return [(x + a * pitch, z + b * pitch) for a, b in ring]

            for shell, holes in rings_with_holes(feature["rings"]):
                footprint = Polygon(position(shell), [position(hole) for hole in holes])
                valid = footprint if footprint.is_valid else make_valid(footprint)
                for polygon in polygons(intersection(valid, clip)):
                    yield Extrusion(polygon, bottom, top)


def group_extrusions(source, pitch_m, spacing_m):
    groups = defaultdict(list)
    for part in source:
        bounds = part.footprint.bounds
        x = math.floor((bounds[0] + bounds[2]) * 0.5 / spacing_m)
        z = math.floor((bounds[1] + bounds[3]) * 0.5 / spacing_m)
        if pitch_m:
            bottom = round(part.bottom_m / pitch_m) * pitch_m
            top = max(bottom + pitch_m, round(part.top_m / pitch_m) * pitch_m)
            footprint = simplify(part.footprint, pitch_m * 0.5, preserve_topology=True)
        else:
            bottom, top, footprint = part.bottom_m, part.top_m, part.footprint
        groups[x, z, bottom, top].append(footprint)
    return tuple(
        Extrusion(polygon, bottom, top)
        for (_, _, bottom, top), footprints in groups.items()
        for polygon in polygons(union_all(footprints))
    )


def triangles(extrusions):
    total = 0
    for item in extrusions:
        footprint = item.footprint
        edges = sum(
            len(ring.coords) - 1 for ring in (footprint.exterior, *footprint.interiors)
        )
        total += 3 * edges + 2 * len(footprint.interiors) - 2
    return total


@dataclass
class Perspective:
    eye: np.ndarray
    axes: np.ndarray
    focal: float

    @classmethod
    def look_at(cls, azimuth, elevation, distance, centre):
        offset = np.asarray(
            (
                math.sin(azimuth) * math.cos(elevation),
                math.sin(elevation),
                math.cos(azimuth) * math.cos(elevation),
            )
        )
        forward = -offset
        right = np.cross(forward, (0, 1, 0))
        right /= np.linalg.norm(right)
        up = np.cross(right, forward)
        return cls(
            centre + distance * offset,
            np.column_stack((right, -up, forward)),
            HEIGHT / (2 * math.tan(math.radians(55) / 2)),
        )

    def project(self, ring, height):
        xyz = np.column_stack((ring[:, 0], np.full(len(ring), height), ring[:, 1]))
        camera = np.einsum("ni,ij->nj", xyz - self.eye, self.axes)
        assert (camera[:, 2] > 0).all()
        points = camera[:, :2] * (self.focal / camera[:, 2, None])
        points += (WIDTH * 0.5, HEIGHT * 0.5)
        return points


def silhouette(extrusions, camera):
    image = Image.new("1", (WIDTH, HEIGHT))
    walls = ImageDraw.Draw(image)
    for item in extrusions:
        footprint = item.footprint
        top_rings = []
        for ring in (footprint.exterior, *footprint.interiors):
            xz = np.asarray(ring.coords)
            top = camera.project(xz, item.top_m)
            bottom = camera.project(xz, item.bottom_m)
            top_rings.append([tuple(point) for point in top])
            for a, b, c, d in zip(bottom, bottom[1:], top[1:], top):
                walls.polygon((tuple(a), tuple(b), tuple(c), tuple(d)), fill=1)
        top = top_rings[0]
        xs, ys = zip(*top)
        x0, x1 = max(0, math.floor(min(xs))), min(WIDTH, math.ceil(max(xs)) + 1)
        y0, y1 = max(0, math.floor(min(ys))), min(HEIGHT, math.ceil(max(ys)) + 1)
        if x0 >= x1 or y0 >= y1:
            continue
        roof = Image.new("1", (x1 - x0, y1 - y0))
        paint = ImageDraw.Draw(roof)
        for index, ring in enumerate(top_rings):
            paint.polygon([(x - x0, y - y0) for x, y in ring], fill=index == 0)
        image.paste(1, (x0, y0), roof)
    return image


def compare(before, after):
    reference, candidate = np.asarray(before, dtype=bool), np.asarray(after, dtype=bool)
    pixels = np.count_nonzero(reference)
    if not pixels:
        raise ValueError("view contains no reference coverage")
    missed = np.count_nonzero(reference & ~candidate) / pixels
    added = np.count_nonzero(candidate & ~reference) / pixels
    return missed, added


def verify_mask_comparison():
    reference = Image.fromarray(np.asarray(((True, False), (False, True))))
    shifted = Image.fromarray(np.asarray(((True, True), (False, False))))
    assert compare(reference, reference) == (0, 0)
    assert compare(reference, shifted) == (0.5, 0.5)


def sample_distances(source, candidates):
    centre = (0.0, 50.0, 0.0)
    radius = 0.0
    for item in (*source, *(item for group in candidates.values() for item in group)):
        low_x, low_z, high_x, high_z = item.footprint.bounds
        dx = max(abs(low_x - centre[0]), abs(high_x - centre[0]))
        dy = max(abs(item.bottom_m - centre[1]), abs(item.top_m - centre[1]))
        dz = max(abs(low_z - centre[2]), abs(high_z - centre[2]))
        radius = max(radius, math.hypot(dx, dy, dz))
    # Distances are outside the sample sphere for every tested direction.
    first = max(4500, math.ceil(radius * 1.1 / 500) * 500)
    return first, first * 2, first * 4


def measure(source, candidates, distances, image_path):
    width, height = 426, 240
    sheet = Image.new(
        "RGB",
        (width * (len(candidates) + 1), (height + 22) * len(distances)),
        "#171b24",
    )
    labels = ImageDraw.Draw(sheet)
    results = defaultdict(list)
    for i, distance in enumerate(distances):
        for direction in (25, 115, 205, 295):
            camera = Perspective.look_at(
                math.radians(direction),
                math.radians(12),
                distance,
                np.asarray((0.0, 50.0, 0.0)),
            )
            before = silhouette(source, camera)
            if direction == 25:
                sheet.paste(
                    before.convert("RGB").resize((width, height)),
                    (0, i * (height + 22)),
                )
                labels.text(
                    (4, i * (height + 22) + height + 3),
                    f"Reference, {distance:g} m",
                    fill="white",
                )
            for j, (pitch, extrusions) in enumerate(candidates.items(), start=1):
                after = silhouette(extrusions, camera)
                missed, added = compare(before, after)
                results[pitch].append((distance, direction, missed, added))
                if direction == 25:
                    x = width * j
                    y = i * (height + 22)
                    sheet.paste(after.convert("RGB").resize((width, height)), (x, y))
                    labels.text(
                        (x + 4, y + height + 3),
                        f"{pitch:g} m simplification, {missed:.1%} lost",
                        fill="white",
                    )
    sheet.save(image_path)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--image", type=Path, required=True)
    options = parser.parse_args()
    verify_mask_comparison()
    source = tuple(read_source(options.source))
    if not source:
        parser.error("no positive-height source geometry in the sample")
    candidates, builds_ms = {}, {}
    for pitch in (2.0, 5.0, 10.0):
        start = perf_counter()
        candidates[pitch] = group_extrusions(source, pitch, 256)
        builds_ms[pitch] = (perf_counter() - start) * 1000
    results = measure(
        source, candidates, sample_distances(source, candidates), options.image
    )
    print(
        json.dumps(
            {
                "reference_extrusions": len(source),
                "reference_triangles": triangles(source),
                "candidates": {
                    pitch: {
                        "extrusions": len(candidate),
                        "triangles": triangles(candidate),
                        "build_ms": builds_ms[pitch],
                        "views": results[pitch],
                    }
                    for pitch, candidate in candidates.items()
                },
            }
        )
    )


if __name__ == "__main__":
    main()

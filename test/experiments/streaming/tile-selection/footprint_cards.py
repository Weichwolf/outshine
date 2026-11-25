"""Compare baked building-group coverage cards against real footprints during flight."""

from __future__ import annotations

import argparse
import json
import math
import zlib
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from time import perf_counter

import numpy as np
from group_footprints import (
    HEIGHT,
    WIDTH,
    Extrusion,
    Perspective,
    read_source,
    silhouette,
    triangles,
)
from PIL import Image, ImageChops, ImageDraw


@dataclass
class Capture:
    origin: np.ndarray
    axes: np.ndarray
    pitch: float

    def project(self, ring, height):
        points = np.column_stack((ring[:, 0], np.full(len(ring), height), ring[:, 1]))
        return ((points - self.origin) @ self.axes[:, :2]) / self.pitch


@dataclass
class Card:
    capture: Capture
    size: tuple[int, int]
    bounds: np.ndarray
    displacement: float
    parts: list[Extrusion]
    mask: Image.Image | None = None
    raster_ms: float = 0

    def pixels(self):
        if self.mask is None:
            began = perf_counter()
            samples = 4
            origin, axes, pitch = (
                self.capture.origin,
                self.capture.axes,
                self.capture.pitch,
            )
            camera = Capture(origin, axes, pitch / samples)
            image = silhouette(
                self.parts, camera, tuple(side * samples for side in self.size)
            )
            self.mask = image.convert("L").resize(self.size, Image.Resampling.BOX)
            self.raster_ms = (perf_counter() - began) * 1000
        return self.mask

    def reprojection(self, camera, samples, aperture=(0, 0, WIDTH, HEIGHT)):
        origin, axes, pitch = self.capture.origin, self.capture.axes, self.capture.pitch
        x, y, right, bottom = aperture
        size = np.asarray((right - x, bottom - y)) * samples
        focal = camera.focal * samples
        offsets = (self.bounds - camera.eye) @ camera.axes
        card_origin = (origin - camera.eye) @ camera.axes
        u, v, normal = axes.T @ camera.axes
        matrix = np.column_stack((u * pitch, v * pitch, card_origin))
        width, height = self.size
        z = min(
            offsets[:, 2].min(),
            card_origin[2]
            + min(0, matrix[2, 0] * width)
            + min(0, matrix[2, 1] * height),
        )
        if z <= 0:
            return None
        extent = abs(offsets[:, :2]).max(axis=0) / z
        texel_offset = 2 * pitch * (abs(u) + abs(v))
        shift = self.displacement * abs(normal) + texel_offset
        # Bound point displacement from (x + dx)/(z + dz) - x/z.
        error = camera.focal * np.linalg.norm(shift[:2] + extent * shift[2]) / z
        intrinsic = np.array(
            [
                [focal, 0, (WIDTH / 2 - x) * samples],
                [0, focal, (HEIGHT / 2 - y) * samples],
                [0, 0, 1],
            ]
        )
        transform = intrinsic @ matrix
        if abs(np.linalg.det(transform)) < 1e-6:
            return None
        drawn = transform @ np.array(
            [
                [0, width, 0, width],
                [0, 0, height, height],
                [1, 1, 1, 1],
            ]
        )
        pixels = drawn[:2] / drawn[2]
        left, top = np.maximum(0, np.floor(pixels.min(axis=1))).astype(int)
        right, bottom = np.minimum(size, np.ceil(pixels.max(axis=1))).astype(int)
        if not (left < right and top < bottom):
            return error, None
        crop_transform = np.array([[1, 0, left], [0, 1, top], [0, 0, 1]])
        inverse = np.linalg.solve(transform, crop_transform)
        if abs(inverse[2, 2]) < 1e-14:
            return None
        return error, ((left, top, right, bottom), (inverse / inverse[2, 2]).flat[:8])

    def draw(self, image, warp, blend=ImageChops.lighter):
        if warp is None:
            return
        box, transform = warp
        left, top, right, bottom = box
        cropped = self.pixels().transform(
            (right - left, bottom - top),
            Image.Transform.PERSPECTIVE,
            transform,
            Image.Resampling.BILINEAR,
        )
        image.paste(blend(image.crop(box), cropped), box)


def corners(parts):
    low = np.full(3, np.inf)
    high = -low.copy()
    for part in parts:
        west, north, east, south = part.footprint.bounds
        low = np.minimum(low, (west, part.bottom_m, north))
        high = np.maximum(high, (east, part.top_m, south))
    return np.asarray(np.meshgrid(*zip(low, high))).reshape(3, -1).T


def plan_card(parts, axes, pitch):
    bounds = corners(parts)
    space = bounds @ axes
    low, high = space.min(axis=0), space.max(axis=0)
    left = np.floor(low[:2] / pitch) - 1
    size = np.ceil(high[:2] / pitch).astype(int) + 1 - left.astype(int)
    origin = axes @ np.r_[left * pitch, (low[2] + high[2]) / 2]
    return Card(
        Capture(origin, axes, pitch),
        (int(size[0]), int(size[1])),
        bounds,
        (high[2] - low[2]) / 2,
        parts,
    )


def catalogue(parts, group_metres):
    groups = defaultdict(list)
    for part in parts:
        west, north, east, south = part.footprint.bounds
        key = (
            math.floor((west + east) / (2 * group_metres)),
            math.floor((north + south) / (2 * group_metres)),
        )
        groups[key].append(part)
    return tuple(groups.values())


def plan_views(group, views, pitch):
    centres = np.array(
        [
            [(a + c) / 2, (b + d) / 2]
            for a, b, c, d in (part.footprint.bounds for part in group)
        ]
    )
    result = []
    for axes in views:
        depth = centres @ axes[[0, 2], 2]
        middle = (depth.min() + depth.max()) / 2
        shallow = [part for part, at in zip(group, depth) if at < middle]
        deep = [part for part, at in zip(group, depth) if at >= middle]
        result.append(
            (
                plan_card(group, axes, pitch),
                tuple(
                    plan_card(layer, axes, pitch) for layer in (shallow, deep) if layer
                ),
            )
        )
    return result


@dataclass
class CroppedCamera:
    camera: Perspective
    offset: np.ndarray
    samples: int

    def project(self, ring, height):
        return (self.camera.project(ring, height) - self.offset) * self.samples


def projection_aperture(bounds, camera, padding):
    corners = np.concatenate(
        [
            camera.project(bounds[bounds[:, 1] == height][:, [0, 2]], height)
            for height in np.unique(bounds[:, 1])
        ]
    )
    left, top = np.maximum(0, np.floor(corners.min(axis=0)).astype(int) - padding)
    right, bottom = np.minimum(
        (WIDTH, HEIGHT), np.ceil(corners.max(axis=0)).astype(int) + padding
    )
    return int(left), int(top), int(right), int(bottom)


def full_mask(painted, aperture):
    left, top, right, bottom = aperture
    image = Image.new("L", (WIDTH, HEIGHT))
    image.paste(
        painted.convert("L").resize((right - left, bottom - top), Image.Resampling.BOX),
        (left, top),
    )
    return image


def reference_mask(parts, bounds, camera, samples, aperture=None):
    if aperture is None:
        aperture = projection_aperture(bounds, camera, padding=1)
    left, top, right, bottom = aperture
    if left >= right or top >= bottom:
        return Image.new("L", (WIDTH, HEIGHT))
    view = CroppedCamera(camera, np.array((left, top)), samples)
    size = (right - left) * samples, (bottom - top) * samples
    return full_mask(silhouette(parts, view, size), aperture)


def geometry_cost(group):
    faces = triangles(group)
    corners = sum(
        len(ring.coords) - 1
        for part in group
        for ring in (part.footprint.exterior, *part.footprint.interiors)
    )
    return faces, faces * 6 + corners * 2 * 12


def render(
    groups, group_costs, assets, views, view, aperture, error_px, layered, blend
):
    left, top, right, bottom = aperture
    size = (right - left) * view.samples, (bottom - top) * view.samples
    image = Image.new("L", size)
    work = 0
    admitted, bytes_decoded = 0, 0
    for group, (faces, mesh_bytes), captures in zip(groups, group_costs, assets):
        centre = captures[0][0].bounds.mean(axis=0)
        heading = view.camera.eye - centre
        index = (views[:, :, 2] @ heading).argmin()
        flat, layers = captures[index]
        cards = layers if layered else (flat,)
        # Estimate RGBA8, packed normal and depth before baking an R8 coverage proxy.
        worth_rasterizing = (
            sum(math.prod(card.size) * 8 + 64 for card in cards) < mesh_bytes
        )
        warps = (
            [card.reprojection(view.camera, view.samples, aperture) for card in cards]
            if worth_rasterizing
            else []
        )
        if warps and all(warp is not None and warp[0] <= error_px for warp in warps):
            admitted += 1
            work += len(cards) * 2
            for card, (_, warp) in zip(cards, warps):
                if warp is None:
                    continue
                bytes_decoded += math.prod(card.size)
                card.draw(image, warp, blend)
        else:
            image = ImageChops.lighter(
                image,
                silhouette(group, view, size).convert("L"),
            )
            work += faces
    return full_mask(image, aperture), {
        "triangles": work,
        "groups": admitted,
        "decoded_r8_bytes": bytes_decoded,
    }


def compare(reference, candidate):
    exact, proxy = (
        np.asarray(image, dtype=np.float64) / 255 for image in (reference, candidate)
    )
    total = exact.sum()
    if total <= 0:
        raise ValueError("capture has no building coverage")
    return {
        "missed_coverage": float(np.maximum(exact - proxy, 0).sum() / total),
        "added_coverage": float(np.maximum(proxy - exact, 0).sum() / total),
    }


def verify_card():
    from shapely.geometry import box

    camera = Perspective.look_at(math.radians(25), math.radians(12), 1400, np.zeros(3))
    card = plan_card([Extrusion(box(-15, -15, 15, 15), 0, 15)], camera.axes, 0.5)
    reprojection = card.reprojection(camera, 1)
    assert card.mask is None
    _, (box, transform) = reprojection
    left, top, right, bottom = box
    warped = card.pixels().transform(
        (right - left, bottom - top), Image.Transform.PERSPECTIVE, transform
    )
    y, x = np.asarray(warped).nonzero()
    assert len(x) > 50
    assert abs(x.mean() + left - WIDTH / 2) < 2
    assert abs(y.mean() + top - HEIGHT / 2) < 10
    turn = Perspective.look_at(math.radians(70), math.radians(4), 25, np.zeros(3))
    close = card.reprojection(turn, 1)
    assert close is None or close[0] > 4


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("image", type=Path)
    parser.add_argument("--extent-m", type=float, default=400)
    parser.add_argument("--directions", type=int, choices=(8, 16), default=8)
    parser.add_argument("--elevations", type=int, nargs="+", default=(12,))
    parser.add_argument("--group-metres", type=int, choices=(128, 256), default=256)
    parser.add_argument("--texel-metres", type=int, choices=(2, 4, 8), default=2)
    parser.add_argument("--distances-m", type=int, nargs=3, default=(3000, 6000, 12000))
    parser.add_argument("--compose", choices=("max", "independent"), default="max")
    parser.add_argument("--reference-samples", type=int, choices=(4, 8, 16), default=4)
    parser.add_argument("--bake-all", action="store_true")
    options = parser.parse_args()
    if not 0 < options.extent_m <= 2000:
        parser.error("expected a sample half-extent of at most 2000 metres")
    if min(options.distances_m) <= 0:
        parser.error("camera distances must be positive")
    if len(options.elevations) > 4 or any(
        not 0 <= angle <= 80 for angle in options.elevations
    ):
        parser.error("expected up to four capture elevations between 0 and 80 degrees")
    verify_card()
    parts = [
        part
        for part in read_source(options.source)
        if max(abs(part.footprint.centroid.x), abs(part.footprint.centroid.y))
        <= options.extent_m
    ]
    if not parts:
        parser.error("no source footprints in sample window")
    source_bounds = corners(parts)
    groups = catalogue(parts, options.group_metres)
    group_costs = [geometry_cost(group) for group in groups]
    views = np.array(
        [
            Perspective.look_at(yaw, math.radians(elevation), 1, np.zeros(3)).axes
            for elevation in options.elevations
            for yaw in np.arange(options.directions) * math.tau / options.directions
        ]
    )
    start = perf_counter()
    assets = [plan_views(group, views, options.texel_metres) for group in groups]
    plan_ms = 1000 * (perf_counter() - start)
    modes = ("single", "two depth groups")
    all_cards = {mode: [] for mode in modes}
    for captures in assets:
        for flat, sheets in captures:
            for mode, cards in zip(modes, ((flat,), sheets)):
                all_cards[mode].extend(cards)
    if options.bake_all:
        for cards in all_cards.values():
            for card in cards:
                card.pixels()
    distances = options.distances_m
    blend = (
        ImageChops.screen if options.compose == "independent" else ImageChops.lighter
    )
    samples = options.reference_samples
    error_px = 4
    sheet = Image.new("RGB", (420 * 3, 225 * len(distances)), "#20242b")
    labels = ImageDraw.Draw(sheet)
    results = []
    for row, distance in enumerate(distances):
        for angle, elevation, offset in ((25, 12, 0), (40, 12, 128), (115, 4, -128)):
            camera = Perspective.look_at(
                math.radians(angle),
                math.radians(elevation),
                distance,
                np.array([offset, 22.0, 0.0]),
            )
            aperture = projection_aperture(source_bounds, camera, padding=error_px + 2)
            before = reference_mask(parts, source_bounds, camera, samples, aperture)
            cropped = CroppedCamera(camera, np.array(aperture[:2]), samples)
            view = {"distance_m": distance, "yaw_deg": angle, "pitch_deg": elevation}
            if samples != 4:
                legacy = reference_mask(parts, source_bounds, camera, 4, aperture)
                view["reference_4x"] = compare(before, legacy)
            for col, (image, label) in enumerate(
                [(before, "Footprints")]
                + [
                    (
                        render(
                            groups,
                            group_costs,
                            assets,
                            views,
                            cropped,
                            aperture,
                            error_px,
                            mode == modes[1],
                            blend,
                        ),
                        mode,
                    )
                    for mode in modes
                ]
            ):
                if col:
                    image, work = image
                    view[label] = compare(before, image) | work
                if angle == 115:
                    crop = (430, 225, 850, 429)
                    sheet.paste(image.crop(crop), (col * 420, row * 225))
                    labels.text(
                        (col * 420 + 5, row * 225 + 206),
                        f"{distance} m   {label}",
                        fill="white",
                    )
            results.append(view)
    sheet.save(options.image)
    stored = {
        mode: {
            "available": len(cards),
            "baked": sum(card.mask is not None for card in cards),
            "raster_ms": sum(card.raster_ms for card in cards),
            "compressed_bytes": sum(
                len(zlib.compress(card.mask.tobytes())) + 64
                for card in cards
                if card.mask is not None
            ),
        }
        for mode, cards in all_cards.items()
    }
    print(
        json.dumps(
            {
                "parts": len(parts),
                "groups": len(groups),
                "group_metres": options.group_metres,
                "directions": options.directions,
                "capture_elevations": options.elevations,
                "texel_metres": options.texel_metres,
                "coverage_composition": options.compose,
                "reference_samples": options.reference_samples,
                "source_triangles": triangles(parts),
                "plan_ms": plan_ms,
                "coverage_archives": stored,
                "views": results,
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()

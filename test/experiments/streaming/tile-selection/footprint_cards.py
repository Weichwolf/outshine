"""Compare baked building-group coverage cards against real footprints during flight."""

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
    mask: Image.Image
    bounds: np.ndarray
    displacement: float

    def reprojection(self, camera, samples):
        origin, axes, pitch = self.capture.origin, self.capture.axes, self.capture.pitch
        size = np.asarray((WIDTH, HEIGHT)) * samples
        focal = camera.focal * samples
        offsets = (self.bounds - camera.eye) @ camera.axes
        card_origin = (origin - camera.eye) @ camera.axes
        u, v, normal = axes.T @ camera.axes
        matrix = np.column_stack((u * pitch, v * pitch, card_origin))
        z = min(
            offsets[:, 2].min(),
            card_origin[2]
            + min(0, matrix[2, 0] * self.mask.width)
            + min(0, matrix[2, 1] * self.mask.height),
        )
        if z <= 0:
            return None
        extent = abs(offsets[:, :2]).max(axis=0) / z
        texel_offset = 2 * pitch * (abs(u) + abs(v))
        shift = self.displacement * abs(normal) + texel_offset
        # Bound point displacement from (x + dx)/(z + dz) - x/z.
        error = camera.focal * np.linalg.norm(shift[:2] + extent * shift[2]) / z
        intrinsic = np.array(
            [[focal, 0, size[0] / 2], [0, focal, size[1] / 2], [0, 0, 1]]
        )
        transform = intrinsic @ matrix
        if abs(np.linalg.det(transform)) < 1e-6:
            return None
        drawn = transform @ np.array(
            [
                [0, self.mask.width, 0, self.mask.width],
                [0, 0, self.mask.height, self.mask.height],
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


def corners(parts):
    low = np.full(3, np.inf)
    high = -low.copy()
    for part in parts:
        west, north, east, south = part.footprint.bounds
        low = np.minimum(low, (west, part.bottom_m, north))
        high = np.maximum(high, (east, part.top_m, south))
    return np.asarray(np.meshgrid(*zip(low, high))).reshape(3, -1).T


def bake(parts, axes, pitch):
    bounds = corners(parts)
    space = bounds @ axes
    low, high = space.min(axis=0), space.max(axis=0)
    left = np.floor(low[:2] / pitch) - 1
    size = np.ceil(high[:2] / pitch).astype(int) + 1 - left.astype(int)
    origin = axes @ np.r_[left * pitch, (low[2] + high[2]) / 2]
    capture = Capture(origin, axes, pitch)
    samples = 4
    paint = Capture(origin, axes, pitch / samples)
    mask = silhouette(parts, paint, tuple(size * samples)).convert("L")
    return Card(
        capture,
        mask.resize(tuple(size), Image.Resampling.BOX),
        bounds,
        (high[2] - low[2]) / 2,
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


def baked_views(group, views, pitch):
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
                bake(group, axes, pitch),
                tuple(bake(layer, axes, pitch) for layer in (shallow, deep) if layer),
            )
        )
    return result


def view_camera(camera, samples):
    return Perspective(
        camera.eye,
        camera.axes,
        camera.focal * samples,
        (WIDTH * samples, HEIGHT * samples),
    )


def render(groups, assets, views, camera, samples, error_px, layered):
    size = WIDTH * samples, HEIGHT * samples
    image = Image.new("L", size)
    work = 0
    admitted, bytes_decoded = 0, 0
    for group, captures in zip(groups, assets):
        centre = captures[0][0].bounds.mean(axis=0)
        heading = camera.eye[[0, 2]] - centre[[0, 2]]
        index = (views[:, [0, 2], 2] @ heading).argmin()
        flat, layers = captures[index]
        cards = layers if layered else (flat,)
        warps = [card.reprojection(camera, samples) for card in cards]
        if all(warp is not None and warp[0] <= error_px for warp in warps):
            admitted += 1
            work += len(cards) * 2
            for card, (_, warp) in zip(cards, warps):
                if warp is None:
                    continue
                bytes_decoded += card.mask.width * card.mask.height
                box, transform = warp
                left, top, right, bottom = box
                cropped = card.mask.transform(
                    (right - left, bottom - top),
                    Image.Transform.PERSPECTIVE,
                    transform,
                    Image.Resampling.BILINEAR,
                )
                image.paste(ImageChops.lighter(image.crop(box), cropped), box)
        else:
            image = ImageChops.lighter(
                image,
                silhouette(group, view_camera(camera, samples), size).convert("L"),
            )
            work += triangles(group)
    return image.resize((WIDTH, HEIGHT), Image.Resampling.BOX), {
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
    from group_footprints import Extrusion
    from shapely.geometry import box

    camera = Perspective.look_at(math.radians(25), math.radians(12), 1400, np.zeros(3))
    card = bake([Extrusion(box(-15, -15, 15, 15), 0, 15)], camera.axes, 0.5)
    reprojection = card.reprojection(camera, 1)
    _, (box, transform) = reprojection
    left, top, right, bottom = box
    warped = card.mask.transform(
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
    parser.add_argument("--group-metres", type=int, choices=(128, 256), default=256)
    parser.add_argument("--texel-metres", type=int, choices=(2, 4, 8), default=2)
    options = parser.parse_args()
    if not 0 < options.extent_m <= 2000:
        parser.error("expected a sample half-extent of at most 2000 metres")
    verify_card()
    parts = [
        part
        for part in read_source(options.source)
        if max(abs(part.footprint.centroid.x), abs(part.footprint.centroid.y))
        <= options.extent_m
    ]
    if not parts:
        parser.error("no source footprints in sample window")
    groups = catalogue(parts, options.group_metres)
    views = np.array(
        [
            Perspective.look_at(yaw, math.radians(12), 1, np.zeros(3)).axes
            for yaw in np.arange(options.directions) * math.tau / options.directions
        ]
    )
    start = perf_counter()
    assets = [baked_views(group, views, options.texel_metres) for group in groups]
    bake_ms = 1000 * (perf_counter() - start)
    modes = ("single", "two depth groups")
    stored = {mode: 0 for mode in modes}
    for captures in assets:
        for flat, sheets in captures:
            for mode, cards in zip(modes, ((flat,), sheets)):
                stored[mode] += sum(
                    len(zlib.compress(card.mask.tobytes())) + 64 for card in cards
                )
    distances = (3000, 6000, 12000)
    samples = 4
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
            before = silhouette(
                parts, view_camera(camera, samples), (WIDTH * samples, HEIGHT * samples)
            ).convert("L")
            before = before.resize((WIDTH, HEIGHT), Image.Resampling.BOX)
            view = {"distance_m": distance, "yaw_deg": angle, "pitch_deg": elevation}
            for col, (image, label) in enumerate(
                [(before, "Footprints")]
                + [
                    (
                        render(
                            groups, assets, views, camera, samples, 4, mode == modes[1]
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
    print(
        json.dumps(
            {
                "parts": len(parts),
                "groups": len(groups),
                "group_metres": options.group_metres,
                "directions": options.directions,
                "texel_metres": options.texel_metres,
                "source_triangles": triangles(parts),
                "bake_ms": bake_ms,
                "coverage_archive_bytes": stored,
                "views": results,
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()

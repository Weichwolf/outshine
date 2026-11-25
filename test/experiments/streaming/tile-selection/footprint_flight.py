"""Compare terrain-seated footprint cards and urban grid coverage along a native flight."""

import argparse
import gzip
import json
import math
import sqlite3
import struct
from contextlib import closing
from pathlib import Path
from time import perf_counter
from xml.etree import ElementTree

import numpy as np
from footprint_cards import (
    CroppedCamera,
    catalogue,
    compare,
    corners,
    geometry_cost,
    plan_card,
    projection_aperture,
    reference_mask,
    render,
)
from group_footprints import (
    EARTH_RADIUS_M,
    HEIGHT,
    WIDTH,
    Extrusion,
    Perspective,
    read_source,
)
from impostor_depth import content
from PIL import Image, ImageChops, ImageDraw
from shapely.geometry import box
from tile_selection import local_frame


class Terrain:
    def __init__(self, directory, source):
        anchor = source["anchor"]
        self.latitude = math.radians(float(anchor["lat"]))
        self.longitude = math.radians(float(anchor["lon"]))
        self.tile_count = 1 << source["z"]
        self.origin = (
            np.asarray(
                (
                    (self.longitude + math.pi) / (2 * math.pi),
                    (1 - math.asinh(math.tan(self.latitude)) / math.pi) * 0.5,
                )
            )
            * self.tile_count
        )
        self.span_m = (
            2 * math.pi * EARTH_RADIUS_M * math.cos(self.latitude) / self.tile_count
        )
        self.tiles = {}
        for tile in source["tiles"]:
            x, y = tile["x"], tile["y"]
            with Image.open(directory / f"{x}-{y}.webp") as image:
                rgb = np.asarray(image.convert("RGB"), dtype=np.float32)
            self.tiles[x, y] = (
                rgb @ np.asarray((256, 1, 1 / 256), dtype=np.float32) - 32768
            )
        (self.side, _) = next(iter(self.tiles.values())).shape
        if any(a.shape != (self.side, self.side) for a in self.tiles.values()):
            raise ValueError("terrain requires tiles with a common square extent")

    def heights(self, positions):
        sample = (self.origin + positions / self.span_m) * self.side - 0.5
        indices = np.floor(sample).astype(int)
        wx, wy = (sample - indices).T
        heights = np.zeros(len(positions))
        for shift_x, shift_y, weight in (
            (0, 0, (1 - wx) * (1 - wy)),
            (1, 0, wx * (1 - wy)),
            (0, 1, (1 - wx) * wy),
            (1, 1, wx * wy),
        ):
            tile, local = np.divmod(indices + (shift_x, shift_y), self.side)
            for key in np.unique(tile, axis=0):
                inside = np.all(tile == key, axis=1)
                x, y = local[inside].T
                heights[inside] += self.tiles[tuple(key)][y, x] * weight[inside]
        return heights


def seat(parts, terrain):
    if not parts:
        return []
    points = [part.footprint.representative_point() for part in parts]
    heights = terrain.heights(np.array([(p.x, p.y) for p in points]))
    return [
        Extrusion(p.footprint, p.bottom_m + h, p.top_m + h)
        for p, h in zip(parts, heights)
    ]


def render_poses(report, scene, terrain, count):
    origin = ElementTree.parse(scene).getroot().find("world").attrib
    anchor, frame = local_frame(float(origin["lat"]), float(origin["lon"]))
    target, target_frame = local_frame(
        math.degrees(terrain.latitude), math.degrees(terrain.longitude)
    )
    motion = report["motion"]
    if any(motion["angular_velocity_deg_s"]) or any(
        report["profile"][key] != value
        for key, value in (("width_px", WIDTH), ("height_px", HEIGHT))
    ):
        raise ValueError("expected a recorded straight 720p flight")
    start, end = motion["start"], motion["finish"]
    basis = np.array(start["world_from_camera"]).reshape(4, 4)[:3, :3].T
    axes = target_frame.T @ frame @ (basis * (1, -1, -1))
    eyes = np.linspace(start["position_m"], end["position_m"], count)
    focal = HEIGHT / (2 * math.tan(math.radians(start["perspective_fov_deg"]) / 2))
    for eye in (eyes @ frame.T + (anchor - target)) @ target_frame:
        yield Perspective(eye, axes, focal)


def grid_sample(database, terrain, extent_m):
    anchor, _ = local_frame(
        math.degrees(terrain.latitude), math.degrees(terrain.longitude)
    )
    search = tuple(
        float(value + margin)
        for value in anchor
        for margin in (extent_m * 2, -extent_m * 2)
    )
    scale = math.cos(terrain.latitude)
    mx, my = (
        EARTH_RADIUS_M * terrain.longitude,
        EARTH_RADIUS_M * math.asinh(math.tan(terrain.latitude)),
    )
    seen = set()
    with closing(
        sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)
    ) as db:
        rows = db.execute(
            """SELECT a.key FROM bounds b JOIN assets a ON a.id=b.id
            WHERE b.minx<=? AND b.maxx>=? AND b.miny<=? AND b.maxy>=? AND b.minz<=? AND b.maxz>=?
            AND a.kind='urban-area' ORDER BY a.id DESC""",
            search,
        ).fetchall()
        for (key,) in rows:
            payload = content(db, key)
            magic, pitch, fill, patch_side, first_x, first_y = struct.unpack_from(
                "<QddIqq", payload
            )
            if (
                magic != 0x31414552414E4255
                or patch_side != 32
                or (first_x, first_y) in seen
            ):
                continue
            seen.add((first_x, first_y))
            columns, lines = struct.unpack_from("<II", payload, 68)
            size = columns * lines
            tiers = np.frombuffer(payload, "u1", size, len(payload) - size).reshape(
                lines, columns
            )
            rows_at, columns_at = np.nonzero(tiers)
            x = ((first_x + columns_at + 0.5) * pitch - mx) * scale
            y = (my - (first_y - rows_at + 0.5) * pitch) * scale
            inside = np.maximum(np.abs(x), np.abs(y)) <= extent_m
            half = pitch * fill * scale * 0.5
            heights = np.array((0, 6, 12, 24, 48))[
                tiers[rows_at[inside], columns_at[inside]]
            ]
            for east, south, height in zip(x[inside], y[inside], heights):
                yield Extrusion(
                    box(east - half, south - half, east + half, south + half), 0, height
                )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "dem", "assets", "scene", "benchmark", "image"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--extent-m", type=float, default=1200)
    parser.add_argument("--poses", type=int, default=5)
    parser.add_argument("--plan-all", action="store_true")
    args = parser.parse_args()
    if args.poses < 2 or not 0 < args.extent_m <= 2000:
        parser.error("expected multiple poses and a bounded source half extent")
    start = perf_counter()
    with gzip.open(args.source, "rt") as file:
        terrain = Terrain(args.dem, json.load(file))
    parts = [
        part
        for part in read_source(args.source)
        if max(abs(part.footprint.centroid.x), abs(part.footprint.centroid.y))
        <= args.extent_m
    ]
    if not parts:
        parser.error("no footprints in the input window")
    grounded = seat(parts, terrain)
    grid = seat(list(grid_sample(args.assets, terrain, args.extent_m)), terrain)
    groups = catalogue(grounded, 256)
    directions = np.asarray(
        [
            Perspective.look_at(yaw, math.radians(elevation), 1, np.zeros(3)).axes
            for elevation in (4, 12)
            for yaw in np.arange(16) * math.tau / 16
        ]
    )
    bounds = corners(grounded + grid)
    centres = np.asarray([corners(group).mean(axis=0) for group in groups])
    report = json.loads(args.benchmark.read_text())
    cameras = list(render_poses(report, args.scene, terrain, args.poses))
    frames = report["motion"]["frames"]
    eyes = np.linspace(cameras[0].eye, cameras[-1].eye, frames + 1)
    axes = directions[:, :, 2]
    chosen = np.asarray([np.argmin((eye - centres) @ axes.T, axis=1) for eye in eyes])
    changed = int(np.count_nonzero(np.diff(chosen, axis=0)))
    sampled = [np.argmin((cam.eye - centres) @ axes.T, axis=1) for cam in cameras]
    if not args.plan_all:
        directions = directions[np.unique(np.concatenate((chosen, sampled)))]
    plan_start = perf_counter()
    costs = [geometry_cost(group) for group in groups]
    assets = [
        [(plan_card(group, axes, 4), ()) for axes in directions] for group in groups
    ]
    plan_ms = 1000 * (perf_counter() - plan_start)
    prepped_ms = 1000 * (perf_counter() - start)
    margins = [projection_aperture(bounds, camera, 8) for camera in cameras]
    crop = (
        min(r[0] for r in margins),
        min(r[1] for r in margins),
        max(r[2] for r in margins),
        max(r[3] for r in margins),
    )
    w, h = crop[2] - crop[0], crop[3] - crop[1]
    sheet = Image.new("RGB", (w * 3, (h + 24) * args.poses), "#20242b")
    labels = ImageDraw.Draw(sheet)
    rows = []
    for index, (camera, aperture) in enumerate(zip(cameras, margins)):
        reference = reference_mask(grounded, bounds, camera, 16, aperture)
        fallback = reference_mask(grid, bounds, camera, 16, aperture)
        ungrounded = reference_mask(parts, bounds, camera, 16, aperture)
        cropped = CroppedCamera(camera, np.array(aperture[:2]), 16)
        candidate, work = render(
            groups,
            costs,
            assets,
            directions,
            cropped,
            aperture,
            4,
            False,
            ImageChops.screen,
        )
        rows.append(
            {
                "frame_fraction": index / (args.poses - 1),
                "fallback": compare(reference, fallback),
                "unseated": compare(reference, ungrounded),
                "card": compare(reference, candidate) | work,
            }
        )
        for col, (image, name) in enumerate(
            (
                (fallback, "Current urban grid"),
                (reference, "Terrain-seated footprints"),
                (candidate, "Terrain-seated cards"),
            )
        ):
            sheet.paste(image.crop(crop), (col * w, index * (h + 24)))
            labels.text((col * w + 3, index * (h + 24) + h + 4), name, fill="white")
    sheet.save(args.image)
    print(
        json.dumps(
            {
                "parts": len(parts),
                "grid_prisms": len(grid),
                "groups": len(groups),
                "source_triangles": sum(faces for faces, _ in costs),
                "prepare_ms": prepped_ms,
                "plan_ms": plan_ms,
                "planned_cards": len(groups) * len(directions),
                "checked_frames": frames + 1,
                "view_changes": changed,
                "baked_cards": sum(
                    flat.mask is not None for group in assets for flat, _ in group
                ),
                "views": rows,
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()

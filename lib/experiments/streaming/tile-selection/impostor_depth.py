"""Reproject cached building fields with flat and quadtree depth support."""

import argparse
import json
import math
import sqlite3
import struct
import subprocess
import xml.etree.ElementTree as ET
import zlib
from contextlib import closing
from pathlib import Path
from time import perf_counter

import numpy as np
from PIL import Image, ImageDraw
from tile_selection import camera_path, distance_from_segment, local_frame


def content(db, key):
    codec, checksum, offset, length, body = db.execute(
        """
        SELECT p.codec,p.crc,a.offset,a.bytes,coalesce(b.bytes,p.bytes)
        FROM assets a JOIN packages p ON p.key=a.package
        LEFT JOIN package_bodies b ON b.key=p.key WHERE a.key=?
        """,
        (key,),
    ).fetchone()
    assert zlib.crc32(body) == checksum
    if codec == 1:
        body = subprocess.run(
            ["zstd", "-q", "-dc"], input=body, capture_output=True, check=True
        ).stdout
    else:
        assert codec == 0
    return body[offset : offset + length]


def captures(database, anchor, radius, limit):
    search = tuple(float(x + d) for x in anchor for d in (radius, -radius))
    with closing(
        sqlite3.connect(database.resolve().as_uri() + "?mode=ro", uri=True)
    ) as db:
        candidates = db.execute(
            """
            SELECT a.key,a.parent
            FROM bounds b JOIN assets a ON a.id=b.id
            WHERE b.minx<=? AND b.maxx>=? AND b.miny<=? AND b.maxy>=?
              AND b.minz<=? AND b.maxz>=? AND a.kind='impostor-material-view-1'
              AND a.bytes BETWEEN 6144 AND 1048576
            ORDER BY a.bytes DESC, a.key LIMIT 512
            """,
            search,
        ).fetchall()
        origins = set()
        for key, parent in candidates:
            meta = content(db, parent)
            format_id, name = struct.unpack_from("<QI", meta)
            if format_id != 0x000231565046:
                continue
            offset = 12 + name + 48
            *centre, half, pixels, views = struct.unpack_from("<4d2I", meta, offset)
            if views != 1 or pixels < 16 or pixels > 128:
                continue
            direction = np.asarray(struct.unpack_from("<3d", meta, offset + 40))
            frame = (
                np.frombuffer(meta, "<f8", count=16, offset=offset + 64).reshape(4, 4).T
            )
            centre = frame[:3, 3] - anchor + frame[:3, :3] @ centre
            group = tuple(np.rint(centre / 2).astype(int))
            if group in origins:
                continue
            origins.add(group)
            payload = content(db, key)
            magic, version, _, width, count, materials = struct.unpack_from(
                "<QIQIII", payload
            )
            assert magic == 0x004E574F5243534F and width == pixels and count == 1
            assert version in (1, 2, 3, 4)
            offset = 64 + materials * (68 if version >= 3 else 52) + 24
            layout = np.dtype(
                [("normal", "<f4", 3), ("depth", "<f4"), ("surface", "<u4")]
            )
            texels = np.frombuffer(payload, layout, pixels * pixels, offset).reshape(
                pixels, pixels
            )
            right = np.cross([0, 1, 0], direction)
            if np.linalg.norm(right) == 0:
                right = np.asarray([1.0, 0.0, 0.0])
            right /= np.linalg.norm(right)
            axes = frame[:3, :3] @ np.stack(
                (right, -np.cross(direction, right), direction), axis=1
            )
            yield key, centre, axes, half, texels["depth"], texels["surface"] != 0
            if len(origins) == limit:
                return


def patches(depth, covered, allowance):
    height, width = depth.shape
    approximate = np.zeros_like(depth, dtype=np.float64)
    pending = [(0, 0, width, height)]
    count = 0
    while pending:
        x0, y0, x1, y1 = pending.pop()
        region = np.s_[y0:y1, x0:x1]
        occupied = covered[region]
        values = depth[region][occupied]
        if not len(values):
            continue
        lo, hi = values.min(), values.max()
        if hi - lo <= allowance * 2:
            approximate[region] = (float(lo) + float(hi)) / 2
            count += 1
            continue
        mx, my = (x0 + x1) // 2, (y0 + y1) // 2
        pending.extend(
            (left, top, right, bottom)
            for left, right in ((x0, mx), (mx, x1))
            for top, bottom in ((y0, my), (my, y1))
            if left < right and top < bottom
        )
    assert np.all(np.abs(approximate[covered] - depth[covered]) <= allowance)
    return approximate, count


def masks(depths, covered, centre, axes, half, eye, focal, dimensions):
    forward = centre - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, axes[:, 1])
    right /= np.linalg.norm(right)
    basis = np.stack((right, np.cross(forward, right), forward), axis=1)
    y, x = covered.nonzero()
    sides = np.asarray([[0, 0], [1, 0], [1, 1], [0, 1]])
    xy = (
        2 * (np.column_stack((x, y))[:, None] + sides) / np.asarray(covered.shape[::-1])
        - 1
    ) * half
    rays = (centre - eye) @ basis + xy @ (axes[:, :2].T @ basis)
    forward_axes = axes[:, 2] @ basis
    size = np.asarray(dimensions)
    reference = None
    projected = []
    for index, depth in enumerate(depths):
        positions = (
            rays + ((4 * depth[covered] - 2) * half)[:, None, None] * forward_axes
        )
        if np.any(positions[..., 2] <= 0):
            return None
        screen = positions[..., :2] * (focal / positions[..., 2:]) + size * 0.5
        if index == 0:
            reference = screen
        if index == 2:
            visible = ((reference >= 0) & (reference <= size)).all(axis=-1) | (
                (screen >= 0) & (screen <= size)
            ).all(axis=-1)
            assert np.all(
                np.abs(screen - reference).sum(axis=-1)[visible] <= 1.000000001
            )
        image = Image.new("1", dimensions)
        draw = ImageDraw.Draw(image)
        for quad in screen:
            draw.polygon(list(map(tuple, quad)), fill=1)
        projected.append(np.asarray(image))
    return projected


def motion_allowance(route, centre, axes, half, focal, dimensions, error_px):
    eye = (route - centre) @ axes
    extent = half * np.asarray([1, 1, 2])
    gap = np.maximum(0, np.maximum(eye.min(axis=0) - extent, -extent - eye.max(axis=0)))
    cone = math.hypot(1, *((px + 1) / (2 * focal) for px in dimensions))
    depth = np.linalg.norm(gap) / cone
    transverse = np.linalg.norm(np.abs(eye[:, :2]) + half, axis=1).max()
    # Invert the perspective displacement bound for two corresponding ray points.
    residual = (
        error_px * depth**2 / (math.sqrt(2) * focal * transverse + error_px * depth)
    )
    return min(1.0, residual / (4 * half))


def isotropic_allowance(route, centre, axes, half, focal, dimensions):
    extent = np.abs(axes) @ (half * np.asarray([1.0, 1.0, 2.0]))
    box = np.asarray([[centre - extent, centre + extent]])
    distance = distance_from_segment(box, *route)[0]
    tx, ty = ((px + 1) / (2 * focal) for px in dimensions)
    depth_scale = math.hypot(1, tx, ty)
    displacement = focal * math.hypot(math.sqrt(2), tx + ty)
    world_error = distance / (displacement * depth_scale + depth_scale + 1)
    if not world_error > 0:
        return 0.0
    _, exponent = math.frexp(world_error)
    return min(1.0, math.ldexp(1.0, exponent - 1) / (4 * half))


def mip(depth, covered, block):
    shape = (len(depth) // block, block, len(depth) // block, block)
    keep = covered.reshape(shape)
    coverage = keep.mean(axis=(1, 3))
    centre = np.where(keep, depth.reshape(shape), 0).max(axis=(1, 3))
    return centre, coverage >= 0.5


def preview_masks(sample, path):
    visible = np.stack(sample).any(axis=0)
    y, x = np.nonzero(visible)
    left, top = max(0, x.min() - 3), max(0, y.min() - 3)
    right, bottom = x.max() + 4, y.max() + 4
    preview = np.concatenate(
        tuple(p[top:bottom, left:right].astype(np.uint8) * 255 for p in sample), axis=1
    )
    image = Image.fromarray(preview)
    image.resize((image.width * 5, image.height * 5), Image.Resampling.NEAREST).save(
        path
    )


def measure(capture, route, focal, dimensions, image_path=None, isotropic=False):
    key, centre, axes, half, depth, covered = capture
    flat = np.full_like(depth, 0.5)
    allowance = motion_allowance(route, centre, axes, half, focal, dimensions, 1)
    if isotropic:
        allowance = isotropic_allowance(route, centre, axes, half, focal, dimensions)
    relief, count = patches(depth, covered, allowance)
    _, strict = patches(depth, covered, 1 / (16 * len(depth)))
    mip_variants = {}
    for block in (2, 4, 8):
        low, occupied = mip(depth, covered, block)
        low_relief, low_count = patches(low, occupied, allowance)
        mip_variants[block] = (low_relief, occupied, low_count)
    views = []
    mip_differences = {block: [] for block in mip_variants}
    for phase, eye in enumerate(np.linspace(*route, 7)):
        if np.dot(eye - centre, axes[:, 2]) <= 0:
            continue
        sample = masks(
            (depth, flat, relief), covered, centre, axes, half, eye, focal, dimensions
        )
        if sample is None or not sample[0].any():
            continue
        original, plane, patches_mask = sample
        views.append(
            [
                phase,
                int(original.sum()),
                int((plane ^ original).sum()),
                int((patches_mask ^ original).sum()),
            ]
        )
        for block, (low, occupied, _) in mip_variants.items():
            coarse = masks((low,), occupied, centre, axes, half, eye, focal, dimensions)
            if coarse is not None:
                mip_differences[block].append(int((coarse[0] ^ original).sum()))
        if image_path is not None and phase == 6:
            preview_masks(sample, image_path)
    return {
        "key": key[:12],
        "size": len(depth),
        "texels": int(covered.sum()),
        "triangles": count * 2,
        "strict_triangles": strict * 2,
        "allowed_depth_residual_m": float(allowance * 4 * half),
        "phase_mask_pixels_flat_xor_relief_xor": views,
        "mips": {
            block: {
                "triangles": low_count * 2,
                "xor_pixels": mip_differences[block],
            }
            for block, (_, _, low_count) in mip_variants.items()
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("database", type=Path)
    parser.add_argument("scenario", type=Path)
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("--image", type=Path)
    parser.add_argument("--count", type=int, default=16)
    parser.add_argument("--isotropic", action="store_true")
    args = parser.parse_args()
    if not 0 < args.count <= 128:
        parser.error("expected 1–128 samples")
    world = ET.parse(args.scenario).getroot().find("world")
    anchor, axes = local_frame(float(world.attrib["lat"]), float(world.attrib["lon"]))
    report = json.loads(args.benchmark.read_text())
    route = camera_path(report, anchor, axes) - anchor
    view = report["motion"]["start"]
    size = tuple(report["profile"][n] for n in ("width_px", "height_px"))
    focal = size[1] / (2 * math.tan(math.radians(view["perspective_fov_deg"]) / 2))
    started = perf_counter()
    results = []
    for capture in captures(args.database, anchor, 12000, args.count):
        if capture[-1].any():
            results.append(
                measure(
                    capture,
                    route,
                    focal,
                    size,
                    args.image if not results else None,
                    args.isotropic,
                )
            )
    if not results:
        raise ValueError("no covered building capture in cached sample")
    print(
        json.dumps(
            {
                "capture_count": len(results),
                "work_ms": (perf_counter() - started) * 1000,
                "results": results,
            }
        )
    )


if __name__ == "__main__":
    main()

"""Replay prepared flat, gable and shed roofs as analytic convex volumes."""

import argparse
import json
import math
import sqlite3
import subprocess
from contextlib import closing
from pathlib import Path
from time import perf_counter

import numpy as np
from building_proxy_budget import build_catalogue
from building_proxy_depth import raster_work
from PIL import Image, ImageDraw
from tile_selection import (
    camera_path,
    distance_from_segment,
    local_frame,
    panoramic_projection,
)

RECORD_WIDTH = 29
BOX_CORNERS = np.array(
    [[x, y, z] for x in (0, 1) for y in (0, 1) for z in (0, 1)], dtype=bool
)


def export_shapes(cache, exporter, path, latitude, longitude, radius):
    anchor, _ = local_frame(latitude, longitude)
    query = """
        SELECT p.key, f.key
        FROM bounds b JOIN assets f ON b.id=f.id
        JOIN assets p ON p.parent=f.parent AND p.kind='building-basis'
        WHERE f.kind='building-forms' AND f.level=1
        AND b.minx<=? AND b.maxx>=? AND b.miny<=? AND b.maxy>=? AND b.minz<=? AND b.maxz>=?
        AND EXISTS (SELECT 1 FROM asset_requests r WHERE r.asset_id=p.id)
        ORDER BY p.id DESC, f.id
    """
    limits = tuple(float(p + d) for p in anchor for d in (radius, -radius))
    with closing(
        sqlite3.connect(cache.resolve().as_uri() + "?mode=ro", uri=True)
    ) as db:
        keys = "".join(f"{basis} {form}\n" for basis, form in db.execute(query, limits))
    if not keys:
        raise ValueError("no height-qualified candidates in the cache query")
    with path.open("wb") as encoded:
        subprocess.run(
            [exporter, cache], input=keys, text=True, stdout=encoded, check=True
        )


def load_shapes(path, latitude, longitude):
    raw = np.fromfile(path, "<f8").reshape(-1, RECORD_WIDTH)
    anchor, axes = local_frame(latitude, longitude)
    base = np.einsum("ni,ij->nj", raw[:, 1:4] - anchor, axes)
    enu = np.einsum("nvi,ij->nvj", raw[:, 4:13].reshape(-1, 3, 3), axes)
    plan = raw[:, 13:21].reshape(-1, 4, 2)
    kind = raw[:, 0].astype(np.intp)
    bottom, eaves, rise, half_width = raw[:, 21:25].T
    centre, across = raw[:, 25:27], raw[:, 27:29]
    edges = np.roll(plan, -1, axis=1) - plan
    nxt = np.roll(edges, -1, axis=1)
    turns = edges[..., 0] * nxt[..., 1] - edges[..., 1] * nxt[..., 0]
    clockwise = (turns < 0).all(axis=1)
    convex = (turns > 0).all(axis=1) | clockwise
    plan[clockwise] = plan[clockwise, ::-1]
    v = np.einsum("nvi,ni->nv", plan - centre[:, None], across)
    pitched = (
        (half_width > 0)
        & (rise >= 0)
        & (np.abs(v) <= half_width[:, None] + 1e-8).all(axis=1)
    )
    use = (
        convex
        & (bottom < eaves)
        & np.isfinite(raw).all(axis=1)
        & ((kind == 0) | (np.isin(kind, [1, 3]) & pitched))
    )
    return tuple(
        x[use]
        for x in (
            base,
            enu,
            plan,
            kind,
            bottom,
            eaves,
            rise,
            half_width,
            centre,
            across,
        )
    )


def boxes(shapes):
    base, enu, plan, _, bottom, eaves, rise, _, _, _ = shapes
    ring = np.concatenate((plan, plan), axis=1)
    vertical = np.repeat(np.column_stack((bottom, eaves + rise)), 4, axis=1)
    corners = np.concatenate((ring, vertical[..., None]), axis=2)
    world = np.einsum("nvi,nij->nvj", corners, enu) + base[:, None]
    return np.stack((world.min(axis=1), world.max(axis=1)), axis=1)


def select(shapes, route, size_px, guard):
    began = perf_counter()
    bounds = boxes(shapes)
    diameter = np.linalg.norm(bounds[:, 1] - bounds[:, 0], axis=1)
    kind = shapes[3]
    # Keep each roof family in its own pages. Every page has one plane count.
    selected = np.zeros(len(bounds), dtype=bool)
    headers = []
    for family in (0, 1, 3):
        group = np.flatnonzero(kind == family)
        if not len(group):
            continue
        _, page_bounds, slot, support, _ = build_catalogue(
            (
                bounds[group][:, :, (0, 2)],
                np.full(len(group), 10),
                diameter[group],
                None,
            ),
            240,
        )
        headers.append((group, page_bounds, slot, support))
    built_ms = (perf_counter() - began) * 1000
    path = camera_path(route, np.zeros(3), np.eye(3))
    focal, diagonal, _ = panoramic_projection(route)
    pages = 0
    began = perf_counter()
    for group, page_bounds, slot, support in headers:
        distance = distance_from_segment(page_bounds, *path[:, (0, 2)]) - guard
        keep = support <= distance * size_px / (focal * diagonal**2)
        pages += int(keep.sum())
        selected[group] = keep[slot]
    return selected, pages, (perf_counter() - began) * 1000, built_ms, bounds


def plane_words(shapes):
    _, _, plan, kind, bottom, eaves, rise, half, centre, across = shapes
    count = len(plan)
    edge = np.roll(plan, -1, axis=1) - plan
    normals = np.zeros((count, 7, 3))
    limits = np.zeros((count, 7))
    normals[:, :4, 0], normals[:, :4, 1] = edge[..., 1], -edge[..., 0]
    limits[:, :4] = (normals[:, :4, :2] * plan).sum(axis=2)
    normals[:, 4, 2], limits[:, 4] = -1, -bottom
    normals[:, 5:, 2] = 1
    slope = np.divide(rise, half, out=np.zeros_like(rise), where=kind != 0)
    slope[kind == 3] *= -0.5
    horizontal = across * slope[:, None]
    normals[:, 5, :2] = horizontal
    normals[:, 6, :2] = -horizontal
    deck = eaves + np.where(kind == 3, rise * 0.5, rise)
    shift = np.einsum("ni,ni->n", horizontal, centre)
    limits[:, 5] = deck + shift
    limits[:, 6] = np.where(kind == 1, deck - shift, np.inf)
    return normals, limits


def analytic_depth(normals, limits, eye, ray):
    projected = np.einsum("nki,ni->nk", normals, ray)
    offset = limits - np.einsum("nki,ni->nk", normals, eye)
    parallel = projected == 0
    hit = np.divide(offset, projected, out=np.zeros_like(offset), where=~parallel)
    entry = np.where(projected < 0, hit, -np.inf).max(axis=1)
    exit = np.where(projected > 0, hit, np.inf).min(axis=1)
    valid = ((~parallel | (offset >= 0)).all(axis=1)) & (entry > 0) & (entry <= exit)
    return np.where(valid, entry, np.inf)


def ridge_split(plan, axis, centre, direction):
    result = []
    for p, q in zip(plan, np.roll(plan, -1, axis=0)):
        u, v = direction * ((p - centre) @ axis), direction * ((q - centre) @ axis)
        if u >= 0:
            result.append(p)
        if (u < 0 < v) or (v < 0 < u):
            result.append(p + (q - p) * (u / (u - v)))
    return np.asarray(result)


def shell_triangles(shape):
    _, _, plan, kind, bottom, eaves, rise, half, centre, across = shape
    roof_pieces = (plan,)
    sides = (plan - centre) @ across
    split = kind == 1 and np.any(sides < 0) and np.any(sides > 0)
    if split:
        roof_pieces = tuple(ridge_split(plan, across, centre, side) for side in (-1, 1))
    faces = []
    floor = np.column_stack((plan, np.full(len(plan), bottom)))
    faces.extend([floor[[0, 2, 1]], floor[[0, 3, 2]]])
    for piece in roof_pieces:
        if len(piece) < 3:
            continue
        height = np.full(len(piece), eaves + rise)
        if kind:
            v = (piece - centre) @ across / half
            height = eaves + rise * (1 - abs(v) if kind == 1 else (v + 1) * 0.5)
        top = np.column_stack((piece, height))
        faces.extend(top[[0, i, i + 1]] for i in range(1, len(top) - 1))
        # A ridge segment inside the footprint is not a wall.
        along = (piece - centre) @ across
        low = np.column_stack((piece, np.full(len(piece), bottom)))
        for i in range(len(piece)):
            j = (i + 1) % len(piece)
            if split and max(abs(along[i]), abs(along[j])) < 1e-8:
                continue
            faces.append([low[i], low[j], top[j]])
            faces.append([low[i], top[j], top[i]])
    return np.asarray(faces)


def triangle_depth(shapes, object_ids, eye, ray):
    used, inverse = np.unique(object_ids, return_inverse=True)
    triangles = tuple(shell_triangles([x[i] for x in shapes]) for i in used)
    width = max(map(len, triangles), default=0)
    packed = np.zeros((len(used), width, 3, 3))
    for at, triangle in enumerate(triangles):
        packed[at, : len(triangle)] = triangle
    depth = np.full(len(ray), np.inf)
    for at in range(width):
        a, b, c = packed[inverse, at].transpose(1, 0, 2)
        ab, ac = b - a, c - a
        cross = np.cross(ray, ac)
        determinant = np.einsum("ni,ni->n", ab, cross)
        valid = np.abs(determinant) > 1e-12
        here = eye - a
        back = np.cross(here, ab)
        scale = np.divide(1, determinant, out=np.zeros_like(determinant), where=valid)
        u = scale * np.einsum("ni,ni->n", here, cross)
        v = scale * np.einsum("ni,ni->n", ray, back)
        t = scale * np.einsum("ni,ni->n", ac, back)
        hit = valid & (t > 0) & (u >= 0) & (v >= 0) & (u + v <= 1)
        np.minimum(depth, np.where(hit, t, np.inf), out=depth)
    return depth


def compare(shapes, limits, normals, bounds, eye, basis, focal, viewport):
    # Bound projection and group validity come before reference mesh generation.
    corners = np.where(BOX_CORNERS[None], bounds[:, 1, None], bounds[:, 0, None])
    front, ordinal, rays, pixel, cards = raster_work(
        corners, eye, basis, focal, viewport
    )
    ids = front[ordinal]
    local_eye = np.einsum("ni,nji->nj", eye - shapes[0], shapes[1])[ids]
    local_ray = np.einsum("ni,nji->nj", rays, shapes[1][ids])
    began = perf_counter()
    actual = analytic_depth(normals[ids], limits[ids], local_eye, local_ray)
    ray_ms = (perf_counter() - began) * 1000
    expected = triangle_depth(shapes, ids, local_eye, local_ray)
    exact, hit = np.isfinite(expected), np.isfinite(actual)
    delta = abs(expected[hit & exact] - actual[hit & exact])
    lost, gained = int((exact & ~hit).sum()), int((hit & ~exact).sum())
    depth = np.full(viewport[0] * viewport[1], np.inf)
    np.minimum.at(depth, pixel, actual)
    return depth.reshape(viewport[::-1]), {
        "cards": int(cards),
        "fragments": len(actual),
        "hits": int(hit.sum()),
        "lost": lost,
        "gained": gained,
        "max_error_m": float(delta.max(initial=0)),
        "ray_ms": ray_ms,
    }


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("forms", type=Path)
    cli.add_argument("benchmark", type=Path)
    cli.add_argument("image", type=Path)
    cli.add_argument("--lat", type=float, required=True)
    cli.add_argument("--lon", type=float, required=True)
    cli.add_argument("--pixels", type=float, default=16)
    cli.add_argument("--cache", type=Path)
    cli.add_argument("--exporter", type=Path)
    cli.add_argument("--cache-radius-m", type=float, default=8200)
    args = cli.parse_args()
    if not math.isfinite(args.pixels) or args.pixels <= 0:
        cli.error("projected support must be finite and positive")
    if (args.cache is None) != (args.exporter is None):
        cli.error("--cache and --exporter go together")
    if args.cache:
        if not math.isfinite(args.cache_radius_m) or args.cache_radius_m <= 0:
            cli.error("cache radius must be finite and positive")
        export_shapes(
            args.cache,
            args.exporter,
            args.forms,
            args.lat,
            args.lon,
            args.cache_radius_m,
        )
    route = json.loads(args.benchmark.read_text())
    shapes = load_shapes(args.forms, args.lat, args.lon)
    take, pages, select_ms, built_ms, bounds = select(shapes, route, args.pixels, 64)
    shapes, bounds = tuple(x[take] for x in shapes), bounds[take]
    normals, limits = plane_words(shapes)
    focal, _, _ = panoramic_projection(route)
    profile = route["profile"]
    viewport = profile["width_px"], profile["height_px"]
    panel = (viewport[0] // 2, viewport[1] // 2)
    sheet = Image.new("RGB", viewport)
    views = []
    for i in range(4):
        pose = route["motion"]["finish" if i >= 2 else "start"]
        eye = np.asarray(pose["position_m"])
        basis = np.asarray(pose["world_from_camera"]).reshape(4, 4)[:3, :3]
        if i % 2:
            basis = basis @ np.diag((-1, 1, -1))
        depth, result = compare(
            shapes, limits, normals, bounds, eye, basis, focal, viewport
        )
        if result["lost"] or result["gained"] or result["max_error_m"] > 1e-6:
            raise AssertionError(result)
        value = np.where(np.isfinite(depth), 240 * np.exp(-depth / 11000), 0)
        frame = Image.fromarray(value.astype(np.uint8)).resize(panel)
        x, y = i % 2 * panel[0], i // 2 * panel[1]
        sheet.paste(frame.convert("RGB"), (x, y))
        ImageDraw.Draw(sheet).text(
            (x + 6, y + 6), f"{result['cards']} cards", fill="white"
        )
        views.append(result)
    sheet.save(args.image)
    counts = {
        roof: int((shapes[3] == kind).sum())
        for kind, roof in zip((0, 1, 3), ("flat", "gable", "shed"))
    }
    print(
        json.dumps(
            {
                "recipes": counts,
                "pages": pages,
                "catalogue_build_ms": built_ms,
                "select_ms": select_ms,
                "views": views,
            }
        )
    )


if __name__ == "__main__":
    main()

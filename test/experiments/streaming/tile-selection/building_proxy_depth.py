"""Compare two-triangle analytic prism proxies to the same extruded source walls."""

import argparse
import json
import math
from pathlib import Path
from time import perf_counter

import numpy as np
from building_proxy_budget import build_catalogue, proxy_profile
from group_footprints import read_source
from PIL import Image, ImageDraw
from tile_selection import camera_path, distance_from_segment, panoramic_projection

WALLS_AND_ROOF = np.array(
    [(i, (i + 1) % 4, 4 + i) for i in range(4)]
    + [(4 + i, (i + 1) % 4, 4 + (i + 1) % 4) for i in range(4)]
    + [(4, 5, 6), (4, 6, 7)]
)


def convex_quads(parts):
    return np.array(
        [
            i
            for i, part in enumerate(parts)
            if part.bottom_m == 0
            and not part.footprint.interiors
            and len(part.footprint.exterior.coords) == 5
            and part.footprint.equals(part.footprint.convex_hull)
        ],
        dtype=np.intp,
    )


def select_prisms(parts, route, pixels, page, guard):
    selection = convex_quads(parts)
    if not len(selection):
        return (), 0, 0
    quads = tuple(parts[i] for i in selection)
    # This catalogue is built once at asset publication from the same footprint.
    boxes, faces, diameter, _ = proxy_profile(quads)
    _, bounds, groups, support, _ = build_catalogue(
        (boxes, faces, diameter, None), page
    )
    path = camera_path(route, np.zeros(3), np.eye(3))[:, (0, 2)]
    focal, diagonal, _ = panoramic_projection(route)
    separation = distance_from_segment(bounds, *path) - guard
    selected = support <= separation * pixels / (focal * diagonal**2)
    ids = np.flatnonzero(selected[groups])
    return tuple(quads[i] for i in ids), int(selected.sum()), len(bounds)


def prism_vertices(parts, ground):
    if not parts:
        return np.empty((0, 8, 3))
    plan = np.array([part.footprint.exterior.coords[:4] for part in parts])
    edge = np.roll(plan, -1, axis=1)
    backwards = (plan[:, :, 0] * edge[:, :, 1] - edge[:, :, 0] * plan[:, :, 1]).sum(
        axis=1
    ) < 0
    plan[backwards] = plan[backwards, ::-1]
    vertices = np.zeros((len(parts), 8, 3))
    vertices[:, :, (0, 2)] = np.concatenate((plan, plan), axis=1)
    vertices[:, :4, 1] = ground
    vertices[:, 4:, 1] = ground + np.array([part.top_m for part in parts])[:, None]
    return vertices


def raster_work(vertices, eye, basis, focal, viewport):
    width, height = viewport
    camera = np.einsum("nvi,ci->nvc", vertices - eye, basis)
    guard = np.all(camera[:, :, 2] < 0, axis=1)
    straddles_eye = ~guard & np.any(camera[:, :, 2] < 0, axis=1)
    if straddles_eye.any():
        # A near-plane crossing needs a different proxy or the original geometry.
        slopes = np.array([width, height]) / (2 * focal)
        samples = camera[straddles_eye]
        sides = (samples[:, :, :2] + samples[:, :, 2:] * slopes >= 0).all(axis=1) | (
            samples[:, :, :2] - samples[:, :, 2:] * slopes <= 0
        ).all(axis=1)
        if not sides.any(axis=1).all():
            raise ValueError("camera plane crosses a potentially visible proxy")
    camera = camera[guard]
    screen = camera[:, :, :2] * np.array([-focal, focal]) / camera[:, :, 2:]
    screen += np.array(viewport) / 2
    low = np.ceil(screen.min(axis=1) - 0.5).clip(0, viewport).astype(int)
    high = (np.floor(screen.max(axis=1) - 0.5) + 1).clip(0, viewport).astype(int)
    dimensions = high - low
    count = dimensions.prod(axis=1)
    ordinal = np.repeat(np.arange(len(count)), count)
    at = np.arange(count.sum()) - np.repeat(count.cumsum() - count, count)
    columns = low[ordinal, 0] + at % dimensions[ordinal, 0]
    lines = low[ordinal, 1] + at // dimensions[ordinal, 0]
    direction = np.einsum(
        "nc,ci->ni",
        np.column_stack(
            (
                (columns + 0.5 - width / 2) / focal,
                (height / 2 - lines - 0.5) / focal,
                -np.ones(len(lines)),
            )
        ),
        basis,
    )
    return (
        vertices[guard],
        ordinal,
        direction,
        lines * width + columns,
        (count > 0).sum(),
    )


def plane_depth(vertices, origin, ordinal, direction):
    walls = np.roll(vertices[:, :4], -1, axis=1) - vertices[:, :4]
    normals = np.zeros((len(vertices), 5, 3))
    normals[:, :4, 0] = walls[:, :, 2]
    normals[:, :4, 2] = -walls[:, :, 0]
    normals[:, 4, 1] = 1
    offsets = np.einsum("ijk,ijk->ij", normals, vertices[:, :5] - origin)
    entry, exit = np.full(len(direction), -np.inf), np.full(len(direction), np.inf)
    for plane in range(5):
        projected = np.einsum("ij,ij->i", normals[ordinal, plane], direction)
        offset = offsets[ordinal, plane]
        parallel = np.abs(projected) <= 1e-12
        depth = np.divide(offset, projected, out=np.zeros_like(offset), where=~parallel)
        entry = np.maximum(entry, np.where(projected < -1e-12, depth, -np.inf))
        exit = np.minimum(exit, np.where(projected > 1e-12, depth, np.inf))
        exit[parallel & (offset < 0)] = -np.inf
    # The lower plane exposes no visible surface but clips intersections with walls.
    offset = vertices[ordinal, 0, 1] - origin[1]
    below = direction[:, 1] < -1e-12
    floor = np.divide(
        offset, direction[:, 1], out=np.full_like(offset, np.inf), where=below
    )
    return np.where((entry > 0) & (entry <= np.minimum(exit, floor)), entry, np.inf)


def triangle_depth(vertices, origin, ordinal, direction):
    nearest = np.full(len(direction), np.inf)
    for a, b, c in WALLS_AND_ROOF:
        p = vertices[ordinal, a]
        ab = vertices[ordinal, b] - p
        ac = vertices[ordinal, c] - p
        cross = np.cross(direction, ac)
        determinant = np.einsum("ij,ij->i", ab, cross)
        oriented = np.abs(determinant) > 1e-12
        reciprocal = np.divide(
            1, determinant, out=np.zeros_like(determinant), where=oriented
        )
        here = origin - p
        u = reciprocal * np.einsum("ij,ij->i", here, cross)
        across = np.cross(here, ab)
        v = reciprocal * np.einsum("ij,ij->i", direction, across)
        depth = reciprocal * np.einsum("ij,ij->i", ac, across)
        hit = oriented & (depth > 0) & (u >= 0) & (v >= 0) & (u + v <= 1)
        np.minimum(nearest, np.where(hit, depth, np.inf), out=nearest)
    return nearest


def compare_view(vertices, eye, basis, focal, viewport):
    work = raster_work(vertices, eye, basis, focal, viewport)
    front, ordinal, rays, pixels, cards = work
    started = perf_counter()
    actual = plane_depth(front, eye, ordinal, rays)
    planes_ms = (perf_counter() - started) * 1000
    started = perf_counter()
    expected = triangle_depth(front, eye, ordinal, rays)
    triangles_ms = (perf_counter() - started) * 1000
    finite = np.isfinite(expected)
    rendered = np.isfinite(actual)
    both = finite & rendered
    delta = np.abs(expected[both] - actual[both])
    depth = np.full(viewport[0] * viewport[1], np.inf)
    np.minimum.at(depth, pixels, actual)
    stats = {
        "cards": int(cards),
        "fragments": len(rays),
        "hits": int(finite.sum()),
        "missing_fragments": int((finite & ~rendered).sum()),
        "added_fragments": int((rendered & ~finite).sum()),
        "max_depth_delta_m": float(delta.max(initial=0)),
        "analytic_ms": planes_ms,
        "triangle_reference_ms": triangles_ms,
    }
    return stats, depth.reshape(viewport[::-1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("image", type=Path)
    parser.add_argument("--flat-ground-asl-m", required=True, type=float)
    parser.add_argument("--pixels", default=16, type=float)
    args = parser.parse_args()
    if not math.isfinite(args.flat_ground_asl_m) or not (
        math.isfinite(args.pixels) and args.pixels > 0
    ):
        parser.error("expected a finite ground height and positive finite pixel limit")
    route = json.loads(args.benchmark.read_text())
    place = tuple(read_source(args.source))
    parts, selected_pages, catalogue_pages = select_prisms(
        place, route, args.pixels, 240, 64
    )
    config = route["profile"]
    viewport = config["width_px"], config["height_px"]
    focal, _, _ = panoramic_projection(route)
    vertices = prism_vertices(parts, args.flat_ground_asl_m)
    size = (viewport[0] // 2, viewport[1] // 2)
    sheet = Image.new("RGB", (size[0] * 2, size[1] * 2), "#171b24")
    views = []
    for ordinal in range(4):
        pose = route["motion"]["finish" if ordinal >= 2 else "start"]
        eye = np.asarray(pose["position_m"])
        if eye[1] <= args.flat_ground_asl_m:
            raise ValueError("camera must be above model ground")
        basis = np.asarray(pose["world_from_camera"]).reshape(4, 4)[:3, :3]
        if ordinal % 2:
            basis = basis @ np.diag((-1, 1, -1))
        stats, depth = compare_view(vertices, eye, basis, focal, viewport)
        if (
            stats["missing_fragments"]
            or stats["added_fragments"]
            or stats["max_depth_delta_m"] > 1e-6
        ):
            raise AssertionError(stats)
        distance = np.where(np.isfinite(depth), 240 * np.exp(-depth / 11000), 0)
        panel = Image.fromarray(distance.astype(np.uint8)).resize(size)
        at = (ordinal % 2 * size[0], ordinal // 2 * size[1])
        sheet.paste(panel.convert("RGB"), at)
        ImageDraw.Draw(sheet).text(
            (at[0] + 6, at[1] + 6), f"{stats['cards']} cards", fill="white"
        )
        views.append(stats)
    sheet.save(args.image)
    print(
        json.dumps(
            {
                "selected_prisms": len(parts),
                "selected_pages": selected_pages,
                "catalogue_pages": catalogue_pages,
                "views": views,
            }
        )
    )


if __name__ == "__main__":
    main()

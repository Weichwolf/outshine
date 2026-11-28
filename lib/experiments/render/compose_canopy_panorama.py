"""Bake prepared canopy depth cards over a linear-colour, radial-depth cubemap."""

import argparse
import json
import sqlite3
from contextlib import closing
from pathlib import Path
from time import perf_counter

import numpy as np
from outshine.rendering.far_cubemap import DOWN, FORWARD, RIGHT, unit
from group_impostor_views import CARD_HEADER, PATCH, basis
from outshine.rendering.pack_far_panorama import write_product
from terrain_cached_page import package

MAXIMUM_BAKED_GROUP_KEYS = 512


def earth_frame(lon_lat):
    lon, lat = np.deg2rad(lon_lat)
    slon, clon, slat, clat = np.sin(lon), np.cos(lon), np.sin(lat), np.cos(lat)
    up = np.array([clat * clon, clat * slon, slat])
    east = [-slon, clon, 0.]
    south = [slat * clon, slat * slon, -clat]
    eccentricity2 = 6.6943799901413165e-3
    transverse_radius = 6378137.0 / np.sqrt(1 - eccentricity2 * slat**2)
    origin = transverse_radius * up * [1, 1, 1 - eccentricity2]
    return origin, np.column_stack((east, up, south))


def card_payload(raw, side, toward):
    magic, stored_side, count, *direction = CARD_HEADER.unpack_from(raw)
    mip_texels = (4 * side**2 - 1) // 3
    start = CARD_HEADER.size + count * PATCH.itemsize
    if (magic != b"OUTSHIV1" or stored_side != side
            or not np.array_equal(direction, toward) or len(raw) != start + 8 * mip_texels):
        raise ValueError("inconsistent canopy card")
    patches = np.frombuffer(raw, PATCH, count, CARD_HEADER.size)
    return patches, start, mip_texels


def lit_card(raw, side, toward, sun, lighting):
    patches, start, mip_texels = card_payload(raw, side, toward)
    rgba = np.frombuffer(raw, np.uint8, 4 * side**2, start).reshape(side, side, 4) / 255
    srgb = rgba[..., :3]
    premultiplied = np.where(srgb < .04045, srgb / 12.92, ((srgb + .055) / 1.055)**2.4)
    rgba[..., :3] = premultiplied
    normal_start = start + 4 * mip_texels
    normal = np.frombuffer(raw, np.uint8, 4 * side**2, normal_start).reshape(side, side, 4)
    xy = normal[..., :2] * (2 / 255) - 1
    z = 1 - np.abs(xy).sum(axis=-1)
    xy = np.where(z[..., None] < 0, (1 - np.abs(xy[..., ::-1])) * np.sign(xy), xy)
    normal = unit(np.dstack((xy, z)))
    sunlit = np.maximum(normal @ sun, 0)[..., None] * lighting["direct"]
    sky_weight = (.5 + .5 * normal[..., 1])[..., None]
    ambient = sky_weight * lighting["sky"] + (1 - sky_weight) * lighting["ground"]
    rgba[..., :3] *= sunlit + ambient
    return patches, rgba


def sample_card(texture, uv):
    side = len(texture)
    texel = np.clip(uv - .5, 0, side - 1)
    lower = texel.astype(int)
    upper = np.minimum(lower + 1, side - 1)
    x, y = (texel - lower).T[..., None]
    left, top = lower.T
    right, bottom = upper.T
    return ((1 - y) * ((1 - x) * texture[top, left] + x * texture[top, right])
            + y * ((1 - x) * texture[bottom, left] + x * texture[bottom, right]))


def patch_region(vertices, face, side, near):
    axial = vertices @ FORWARD[face]
    if axial.max() < near:
        return
    if axial.min() <= 0:
        return (0, 0), (side, side)
    projected = np.column_stack((vertices @ RIGHT[face], vertices @ DOWN[face]))
    pixels = (projected / axial[:, None] + 1) * (.5 * side)
    low = np.maximum(np.floor(pixels.min(axis=0)).astype(int), 0)
    high = np.minimum(np.ceil(pixels.max(axis=0)).astype(int), side)
    if np.all(high > low):
        return low, high


def patch_plane(patch, axes):
    left, top, right, bottom = patch["rect"].astype(int)
    intercept, dx, dy = patch["plane"]
    origin, across, down, toward = axes
    point = origin + left * across + top * down + intercept * toward
    along = across + dx * toward
    below = down + dy * toward
    corners = point + np.array([0, right-left, right-left, 0])[:, None] * along
    corners += np.array([0, 0, bottom-top, bottom-top])[:, None] * below
    return point, np.cross(along, below), corners


def paint_patch(target, patch, axes, texture, near, far, samples, owner):
    colour, depth, owners = target
    left, top, right, bottom = patch["rect"]
    origin, across, down, _ = axes
    point, normal, corners = patch_plane(patch, axes)
    side = depth.shape[-1]
    painted = False
    for face in range(len(FORWARD)):
        region = patch_region(corners, face, side, near)
        if region is None:
            continue
        (xmin, ymin), (xmax, ymax) = region
        grid_x, grid_y = np.meshgrid(np.arange(xmin, xmax), np.arange(ymin, ymax))
        for index, (sx, sy, cutoff) in enumerate(samples):
            x, y = ((grid_x + sx) * (2 / side) - 1, (grid_y + sy) * (2 / side) - 1)
            rays = unit(FORWARD[face] + x[..., None] * RIGHT[face] + y[..., None] * DOWN[face])
            denom = rays @ normal
            distance = np.divide(point @ normal, denom, out=np.zeros_like(denom),
                                 where=np.abs(denom) > 1e-10)
            old = depth[index, face, grid_y, grid_x]
            visible = ((distance * (rays @ FORWARD[face]) >= near) & (distance <= far)
                       & ((old == 0) | (distance < old)))
            yy, xx = np.nonzero(visible)
            if not len(yy):
                continue
            delta = distance[yy, xx, None] * rays[yy, xx] - origin
            u = delta @ across / (across @ across)
            v = delta @ down / (down @ down)
            inside = (u >= left) & (u < right) & (v >= top) & (v < bottom)
            if not inside.any():
                continue
            texel = sample_card(texture, np.column_stack((u[inside], v[inside])))
            covered = texel[:, 3] >= cutoff
            if covered.any():
                r, c = yy[inside][covered], xx[inside][covered]
                pixel = index, face, grid_y[r, c], grid_x[r, c]
                depth[pixel] = distance[r, c]
                colour[pixel] = texel[covered, :3] / texel[covered, 3, None]
                owners[pixel] = owner
                painted = True
    return painted


def card_axes(centre, half, side, rotation, toward):
    right, up, forward = (rotation @ basis(toward)).T
    return (centre + half * (up - right - 2 * forward),
            (2 * half / side) * right, (-2 * half / side) * up, 4 * half * forward)


def canopy_package(store, key, kind, max_size):
    raw, _ = package(store, key, kind=kind, max_bytes=max_size)
    return raw


def view_package(store, key):
    return canopy_package(store, key, "render-group-impostor-view-v1", 32 * 1024**2)


def card_side(manifest, view):
    return manifest["card_sides"][view] if "card_sides" in manifest else manifest["card_side"]


def outside_overlap(store, manifest, centre, rotation, panorama):
    axis = np.argmax(np.abs(centre))
    sign = np.sign(centre[axis])
    half = manifest["half_extent_m"]
    margin = panorama["maximum_eye_translation_m"]
    overlap = panorama["axial_overlap_end_m"] + margin
    far = panorama["maximum_range_m"] - margin
    if abs(centre[axis]) <= overlap:
        return False
    for view, (toward, key) in enumerate(zip(manifest["directions"], manifest["card_assets"])):
        side = card_side(manifest, view)
        patches, _, _ = card_payload(view_package(store, key), side, toward)
        axes = card_axes(centre, half, side, rotation, toward)
        for patch in patches:
            _, _, corners = patch_plane(patch, axes)
            if np.any(sign * corners[:, axis] <= overlap):
                return False
            if np.any(np.linalg.norm(corners, axis=1) >= far):
                return False
    return True


def draw_group(target, store, key, panorama, frame, samples, owner):
    manifest = json.loads(canopy_package(store, key, "render-canopy-impostor-v1", 32 * 1024))
    origin, local = earth_frame(manifest["surface"]["origin_lon_lat"])
    eye, world = frame
    rotation = world.T @ local
    centre = world.T @ (origin - eye) + rotation @ manifest["centre_m"]
    half = manifest["half_extent_m"]
    directions = np.asarray(manifest["directions"])
    views = np.argsort(directions @ (rotation.T @ centre), kind="stable")[:2]
    light = panorama["surface_lighting"]
    sun = rotation.T @ light["to_sun_eus"]
    exposure = panorama["reference_exposure"]
    lighting = {key: np.asarray(light[key]) / exposure for key in ("direct", "sky", "ground")}
    clip_start, clip_end = panorama["axial_near_m"], panorama["maximum_range_m"]
    painted = False
    for view in views:
        toward = directions[view]
        side = card_side(manifest, view)
        raw = view_package(store, manifest["card_assets"][view])
        patches, texture = lit_card(raw, side, toward, sun, lighting)
        axes = card_axes(centre, half, side, rotation, toward)
        for patch in patches:
            painted |= paint_patch(target, patch, axes, texture, clip_start, clip_end, samples, owner)
    return painted and outside_overlap(store, manifest, centre, rotation, panorama)


def canopy_keys(store, revision):
    with closing(sqlite3.connect(store.resolve().as_uri() + "?mode=ro", uri=True)) as db:
        return [key for key, in db.execute(
            "SELECT a.key FROM asset_requests r JOIN assets a ON a.id=r.asset_id "
            "JOIN asset_tiles t ON a.id=t.asset_id WHERE a.kind=? AND t.revision=? "
            "ORDER BY a.key", ("render-canopy-impostor-v1", revision))]


def compose(source, output, store, revision, samples):
    began = perf_counter()
    panorama = json.loads((source / "panorama.json").read_text())
    side = panorama["face_px"]
    shape = (len(FORWARD), side, side)
    rgba = np.fromfile(source / "colour.rgba16f", dtype="<f2").reshape(*shape, 4)
    radial = np.fromfile(source / "depth.r32f", dtype="<f4").reshape(shape)
    depths = np.repeat(radial[None], len(samples), axis=0)
    colours = np.repeat(rgba[None, ..., :3], len(samples), axis=0).astype(np.float32)
    owners = np.zeros(depths.shape, dtype=np.uint32)
    origin, world = earth_frame(panorama["origin_lon_lat"])
    frame = origin + world @ panorama["anchor_eus_m"], world
    groups = canopy_keys(store, revision)
    eligible = {owner: key for owner, key in enumerate(groups, 1)
                if draw_group((colours, depths, owners), store, key, panorama, frame, samples,
                              owner)}
    baked = [eligible[owner] for owner in np.unique(owners) if owner in eligible]
    baked = baked[:MAXIMUM_BAKED_GROUP_KEYS]
    panorama["baked_group_keys"] = baked
    hits = owners != 0
    affected = hits.any(axis=0)
    filtered = colours.mean(axis=0)[affected]
    rgba[..., :3][affected] = np.minimum(filtered, np.finfo(np.float16).max)
    depth = np.min(np.where(hits, depths, np.inf), axis=0)
    foreground = hits.sum(axis=0) >= (len(samples) + 1) // 2
    radial[foreground] = depth[foreground]
    rgba[..., 3] = radial > 0
    write_product(output, panorama, rgba, radial)
    print(json.dumps({"groups": len(groups), "far_only": len(baked), "colour_texels": int(affected.sum()),
                      "foreground_texels": int(foreground.sum()),
                      "compose_ms": (perf_counter() - began) * 1000}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("store", type=Path)
    parser.add_argument("revision")
    parser.add_argument("--point", action="store_true")
    opts = parser.parse_args()
    samples = ((.5, .5, .5),) if opts.point else (
        (.25, .25, .125), (.75, .25, .625), (.25, .75, .875), (.75, .75, .375))
    compose(opts.source, opts.output, opts.store, opts.revision, samples)


if __name__ == "__main__":
    main()

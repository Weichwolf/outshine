"""Compare depth-card directions against held-out native orthographic captures."""

import argparse
import json
from pathlib import Path
from struct import Struct
from time import perf_counter

import numpy as np
from PIL import Image

from field_view_sampling import FieldGrid


TEXEL = np.dtype([("surface", "<u4"), ("depth", "<f4"), ("normal", "<f4", 3),
                 ("colour", "<f4", 3), ("roughness", "<f4"), ("metalness", "<f4")])
CARD_HEADER = Struct("<8sII3d")
PATCH = np.dtype([("rect", "<u2", 4), ("plane", "<f4", 3)])
HORIZONTAL = tuple((azimuth, 0) for azimuth in range(0, 360, 45))
UPPER = tuple((azimuth, 45) for azimuth in range(0, 360, 90)) + ((0, 90),)
PROBES = ((22.5, 0), (67.5, 0), (157.5, 0), (247.5, 0), (13, 20),
          (51, 28), (132, 35), (225, 40), (38, 60), (180, 70), (147, 85))
ANGLES = HORIZONTAL + UPPER + PROBES


def direction(azimuth, elevation):
    if elevation == 90:
        return np.asarray([0., 1., 0.])
    azimuth, elevation = np.deg2rad((azimuth, elevation))
    return np.asarray([np.sin(azimuth) * np.cos(elevation), np.sin(elevation),
                       np.cos(azimuth) * np.cos(elevation)])


def basis(vector):
    grid = FieldGrid((0., 0., 0.), tuple(vector), 1.)
    right, up = grid.axes()
    return np.stack((right, up, vector), axis=1)


def fields(folder, angles=ANGLES):
    info = json.loads((folder / "group.json").read_text())
    side = info["pixels"]
    expected = np.asarray([direction(*values) for values in angles])
    if not np.allclose(info["directions"], expected, rtol=0, atol=1e-8):
        raise ValueError("capture manifest does not contain the requested views")
    depth, alpha, albedo, normals = [], [], [], []
    for at in range(len(angles)):
        data = np.fromfile(folder / f"{at}.field", TEXEL).reshape(side, side)
        valid = data["surface"] != 0
        if not np.array_equal(valid, data["depth"] > 0):
            raise ValueError("native coverage and depth disagree")
        depth.append(data["depth"])
        alpha.append(valid.astype(float))
        albedo.append(data["colour"])
        normals.append(data["normal"])
    return info, expected, tuple(zip(depth, alpha, albedo, normals))


def reduce_field(field, block):
    depth, alpha, colour, normal = field
    if block == 1:
        return field
    side = len(depth) // block
    shape = side, block, side, block
    cells = alpha.reshape(shape)
    coverage = cells.mean(axis=(1, 3))
    foreground = np.max(np.where(cells > 0, depth.reshape(shape), 0), axis=(1, 3))
    premultiplied = (colour * alpha[..., None]).reshape(*shape, 3).mean(axis=(1, 3))
    rgb = np.divide(premultiplied, coverage[..., None], out=np.zeros_like(premultiplied),
                    where=coverage[..., None] > 0)
    axes = (normal * alpha[..., None]).reshape(*shape, 3).sum(axis=(1, 3))
    lengths = np.linalg.norm(axes, axis=-1, keepdims=True)
    axes = np.divide(axes, lengths, out=np.zeros_like(axes), where=lengths > 0.)
    return foreground, coverage, rgb, axes


def depth_patches(field, pixels_error):
    depth, alpha, colour, normal = field
    side = len(depth)
    fitted = depth.copy()
    slopes = np.zeros((*depth.shape, 2))
    pending = [(0, 0, side, side)]
    patches = 0
    while pending:
        left, top, right, bottom = pending.pop()
        row, col = np.nonzero(alpha[top:bottom, left:right] > 0.)
        if not len(row):
            continue
        row, col = row + top, col + left
        left, top, right, bottom = col.min(), row.min(), col.max() + 1, row.max() + 1
        x, y = col - left, top - row
        z = (4. * depth[row, col] - 2.) * (.5 * side)
        design = np.column_stack((x, y, np.ones_like(x)))
        if len(row) == 1:
            a, b, c = 0., 0., z[0]
        else:
            a, b, c = np.linalg.lstsq(design, z, rcond=None)[0]
        residual = np.max(abs(design @ (a, b, c) - z))
        if residual <= pixels_error and np.hypot(a, b) <= 2.:
            fitted[row, col] = (design @ (a, b, c)) / (2. * side) + .5
            slopes[row, col] = (a, b)
            patches += 1
            continue
        x_split, y_split = (left + right) // 2, (top + bottom) // 2
        pending.extend((x0, y0, x1, y1)
                       for x0, x1 in ((left, x_split), (x_split, right))
                       for y0, y1 in ((top, y_split), (y_split, bottom))
                       if x0 < x1 and y0 < y1)
    return (fitted, alpha, colour, normal), slopes, patches


def native_card(path, expected_side, view):
    data = path.read_bytes()
    magic, side, count, *axis = CARD_HEADER.unpack_from(data)
    if magic != b"OUTSHIV1" or side != expected_side or not np.allclose(axis, view):
        raise ValueError("inconsistent native card header")
    patches = np.frombuffer(data, PATCH, count, CARD_HEADER.size)
    mip_texels = (4 * side * side - 1) // 3
    pixel_offset = CARD_HEADER.size + count * PATCH.itemsize
    if len(data) != pixel_offset + 8 * mip_texels:
        raise ValueError("inconsistent native card size")
    base = np.frombuffer(data, np.uint8, 4 * side * side, pixel_offset)
    rgba = base.reshape(side, side, 4) / 255.
    srgb, alpha = rgba[..., :3], rgba[..., 3]
    linear = np.where(srgb < .04045, srgb / 12.92, ((srgb + .055) / 1.055)**2.4)
    colour = np.divide(linear, alpha[..., None], out=np.zeros_like(linear),
                       where=alpha[..., None] > 0.)
    depth = np.zeros_like(alpha)
    slopes = np.zeros((*alpha.shape, 2))
    for (left, top, right, bottom), (origin, sx, sy) in patches:
        x, y = np.meshgrid(np.arange(right - left) + .5, np.arange(bottom - top) + .5)
        depth[top:bottom, left:right] = origin + sx * x + sy * y
        slopes[top:bottom, left:right] = 2 * side * np.asarray((sx, -sy))
    if np.any((alpha > 0.) & (depth <= 0.)):
        raise ValueError("native patch failed to cover an occupied texel")
    return (depth, alpha, colour, np.empty_like(colour)), slopes, count


def raster_card(field, source, target, output_side, *, flat=False, tangent=False, planes=None):
    depth, alpha, colour, normals = field
    side = len(depth)
    if side < output_side:
        raise ValueError("card raster supports matching or downsampled output sizes")
    y, x = np.nonzero(alpha > 0.)
    rotation = source.T @ target
    target_depth = np.full(output_side**2, -np.inf)
    target_alpha = np.zeros(output_side**2)
    target_colour = np.zeros((output_side**2, 3))
    if len(x):
        z = np.zeros_like(x, dtype=float) if flat else 4. * depth[y, x] - 2.
        world = np.column_stack((2. * (x + .5) / side - 1.,
                                 1. - 2. * (y + .5) / side, z))
        slopes = np.zeros((len(x), 2))
        if planes is not None:
            slopes = planes[y, x]
        elif tangent:
            normal = normals[y, x] @ source
            np.divide(-normal[:, :2], normal[:, 2:3], out=slopes,
                      where=abs(normal[:, 2:3]) >= .5)
        differentials = rotation[:2] + slopes[..., None] * rotation[2]
        jacobian = differentials[:, :, :2] * (output_side / side)
        faceable = np.abs(np.linalg.det(jacobian)) > 1e-8
        jacobian = np.where(faceable[:, None, None], jacobian, np.eye(2))
        projected = world @ rotation
        xy = (projected[:, :2] + 1.) * .5 * output_side
        origin = np.floor(xy).astype(np.int64)
        reach = range(-2, 3) if tangent or planes is not None else range(-1, 2)
        offsets = np.asarray([(i, j) for j in reach for i in reach])
        test = origin[:, None] + offsets
        inverse = np.linalg.inv(jacobian)
        local = (test + .5 - xy[:, None]) @ inverse
        in_footprint = np.max(abs(local), axis=-1) <= .5 + 1e-7
        in_footprint &= faceable[:, None]
        valid = in_footprint & np.all((test >= 0) & (test < output_side), axis=-1)
        sample, corner = np.nonzero(valid)
        vx, vy = test[sample, corner].T
        destination = (output_side - 1 - vy) * output_side + vx
        z_depth = projected[sample, 2] + np.sum(
            local[sample, corner] * differentials[sample, :, 2], axis=-1) * (2. / side)
        np.maximum.at(target_depth, destination, z_depth)
        front = z_depth == target_depth[destination]
        target_alpha[destination[front]] = alpha[y[sample[front]], x[sample[front]]]
        target_colour[destination[front]] = colour[y[sample[front]], x[sample[front]]]
    sizes = (output_side, output_side)
    return target_depth.reshape(sizes), target_alpha.reshape(sizes), target_colour.reshape(*sizes, 3)


def foreground(first, second):
    take_second = second[0] > first[0]
    return tuple(np.where(take_second[..., None] if data.ndim == 3 else take_second, value, data)
                 for data, value in zip(first, second))


def statistics(guess, reference, half):
    depth, alpha, _ = guess
    actual_depth, actual_alpha, _ = reference
    union = np.maximum(alpha, actual_alpha).sum()
    area = actual_alpha.sum()
    if area <= 0.:
        raise ValueError("empty held-out capture")
    shared = (alpha >= .5) & (actual_alpha >= .5)
    return {
        "visible_area": float(area),
        "coverage_iou": float(np.minimum(alpha, actual_alpha).sum() / union),
        "missing_fraction": float(np.maximum(actual_alpha - alpha, 0).sum() / area),
        "extra_fraction": float(np.maximum(alpha - actual_alpha, 0).sum() / area),
        "shared_depth_p95_m": (float(np.quantile(abs(depth[shared] - actual_depth[shared]), .95)
                                     * half) if np.any(shared) else None),
    }


def preview(fields, output):
    panels = []
    for _, alpha, colour in fields:
        srgb = np.where(colour < .0031308, 12.92 * colour, 1.055 * colour**(1 / 2.4) - .055)
        background = np.array([.16, .19, .23])
        rgb = srgb * alpha[..., None] + background * (1. - alpha[..., None])
        panels.append((255 * np.clip(rgb, 0, 1)).astype(np.uint8))
    image = Image.fromarray(np.concatenate(panels, axis=1))
    image.resize((image.width * 3, image.height * 3), Image.Resampling.NEAREST).save(output)


def evaluate(folder, output):
    began = perf_counter()
    info, vectors, data = fields(folder)
    all_bases = [basis(vector) for vector in vectors]
    n = len(HORIZONTAL)
    views = n + len(UPPER)
    rows = []
    for block in (1, 2, 4):
        side = info["pixels"] // block
        reduced = [reduce_field(item, block) for item in data]
        fitted = [depth_patches(item, .5) for item in reduced[:views]]
        native = ([native_card(folder / f"{at}.card", side, vectors[at]) for at in range(views)]
                  if side == info.get("card_side") else None)
        for probe, current in enumerate(vectors[views:], start=views):
            target = all_bases[probe]
            order = np.argsort(-(vectors[:views] @ current))
            ring = next(index for index in order if index < n)
            best, next_best = order[:2]
            reference = raster_card(reduced[probe], target, target, side)
            if not np.array_equal(reference[1], reduced[probe][1]):
                raise ValueError("matching-view raster changed coverage")
            single = raster_card(reduced[best], all_bases[best], target, side)
            dual = foreground(single, raster_card(reduced[next_best], all_bases[next_best],
                                                   target, side))
            tilted = raster_card(reduced[best], all_bases[best], target, side, tangent=True)
            dual_tilted = foreground(tilted, raster_card(
                reduced[next_best], all_bases[next_best], target, side, tangent=True))
            planar = [raster_card(fitted[index][0], all_bases[index], target, side,
                                  planes=fitted[index][1]) for index in (best, next_best)]
            candidates = {
                "ring_flat": raster_card(reduced[ring], all_bases[ring], target, side, flat=True),
                "ring_depth": raster_card(reduced[ring], all_bases[ring], target, side),
                "nearest_depth": single,
                "dual_depth": dual,
                "nearest_tangent": tilted,
                "dual_tangent": dual_tilted,
                "dual_patches": foreground(*planar),
            }
            if native:
                cards = [raster_card(native[index][0], all_bases[index], target, side,
                                     planes=native[index][1]) for index in (best, next_best)]
                candidates["native_patches"] = foreground(*cards)
            for method, result in candidates.items():
                row = {"method": method, "side": side, "view": ANGLES[probe],
                       **statistics(result, reference, info["half_extent_m"])}
                if method == "dual_patches":
                    row["triangles"] = 2 * (fitted[best][2] + fitted[next_best][2])
                if method == "native_patches":
                    row["triangles"] = 2 * (native[best][2] + native[next_best][2])
                rows.append(row)
            if block == 2 and ANGLES[probe] in ((22.5, 0), (51, 28), (147, 85)):
                preview((reference, candidates["ring_flat"], dual, dual_tilted,
                         candidates["dual_patches"]),
                        output / f"{ANGLES[probe][0]}-{ANGLES[probe][1]}.png")
    summary = {"native": info, "reference_ms": (perf_counter() - began) * 1000, "rows": rows}
    (output / "comparison.json").write_text(json.dumps(summary, indent=2) + "\n")
    for side in sorted({item["side"] for item in rows}, reverse=True):
        for method in dict.fromkeys(item["method"] for item in rows if item["side"] == side):
            selected = [item for item in rows if item["side"] == side and item["method"] == method]
            values = [item["coverage_iou"] for item in selected]
            triangles = ""
            if "triangles" in selected[0]:
                triangles = f" max_triangles={max(item['triangles'] for item in selected)}"
            print(side, method,
                  f"min_iou={min(values):.3f} median_iou={np.median(values):.3f}{triangles}")


def compose_stand(tree, tree_info, stand_info, axis):
    depth, coverage, colour, _ = tree
    side = stand_info["pixels"]
    stand_half = stand_info["half_extent_m"]
    tree_half = tree_info["half_extent_m"]
    roots = np.asarray(stand_info["roots_m"])
    offsets = (roots + tree_info["centre_m"] - stand_info["centre_m"]) @ axis
    coordinate = (2. * (np.arange(side) + .5) / side - 1.) * stand_half
    col, row = np.meshgrid(coordinate, -coordinate)
    output_z = np.full_like(col, -np.inf)
    output_coverage = np.zeros_like(col)
    output_rgb = np.zeros((*col.shape, 3))
    tree_side = len(depth)
    for x, y, z in offsets:
        u = np.floor((1. + (col - x) / tree_half) * (.5 * tree_side)).astype(np.int64)
        v = np.floor((1. - (row - y) / tree_half) * (.5 * tree_side)).astype(np.int64)
        inside = (u >= 0) & (u < tree_side) & (v >= 0) & (v < tree_side)
        u, v = np.clip(u, 0, tree_side - 1), np.clip(v, 0, tree_side - 1)
        distance = ((4. * depth[v, u] - 2.) * tree_half + z) / stand_half
        front = inside & (coverage[v, u] > 0.) & (distance > output_z)
        output_z[front] = distance[front]
        output_coverage[front] = coverage[v[front], u[front]]
        output_rgb[front] = colour[v[front], u[front]]
    return output_z, output_coverage, output_rgb


def hierarchical(tree_capture, stand_capture, output, angles=ANGLES):
    tree, _, layers = fields(tree_capture, angles)
    stand, vectors, originals = fields(stand_capture, angles)
    if len(tree["roots_m"]) != 1 or len(stand["roots_m"]) < 2:
        raise ValueError("a representative tree and a multi-tree reference stand are required")
    began = perf_counter()
    rows = []
    previews = []
    for angle, vector, layer, original in zip(angles, vectors, layers, originals):
        axis = basis(vector)
        reference = raster_card(original, axis, axis, stand["pixels"])
        composed = compose_stand(layer, tree, stand, axis)
        rows.append({"view": angle, **statistics(composed, reference, stand["half_extent_m"])})
        if angle in ((22.5, 0), (51, 28), (147, 85)):
            previews.append((angle, reference, composed))
    duration = (perf_counter() - began) * 1000
    for angle, reference, composed in previews:
        preview((reference, composed), output / f"{angle[0]}-{angle[1]}.png")
    (output / "hierarchical.json").write_text(json.dumps({
        "tree": tree, "stand": stand, "reference_ms": duration, "rows": rows,
    }, indent=2) + "\n")
    values = [item["coverage_iou"] for item in rows]
    print(f"composite_ms={duration:.3f} min_iou={min(values):.3f} median_iou={np.median(values):.3f}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    modes.add_parser("directions")
    check = modes.add_parser("evaluate")
    check.add_argument("capture", type=Path)
    check.add_argument("output", type=Path)
    stand = modes.add_parser("hierarchical")
    stand.add_argument("tree", type=Path)
    stand.add_argument("stand", type=Path)
    stand.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.mode == "directions":
        for angle in ANGLES:
            print(*direction(*angle))
        return
    args.output.mkdir()
    if args.mode == "evaluate":
        evaluate(args.capture, args.output)
    else:
        hierarchical(args.tree, args.stand, args.output)


if __name__ == "__main__":
    main()

"""Compare shared index LODs on exported native terrain pages; no runtime/GPU claim."""

import argparse
import csv
import hashlib
import json
import resource
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from pathlib import Path

import numpy as np
import shapely


def edge_steps(side, stride, keep_boundary, boundary_steps):
    steps = (
        (1 if keep_boundary else stride,) * 4
        if boundary_steps is None
        else tuple(boundary_steps)
    )
    if (
        side < 2
        or stride <= 0
        or stride & (stride - 1)
        or (side - 1) % stride
        or len(steps) != 4
        or any(s <= 0 or stride % s for s in steps)
    ):
        raise ValueError(
            "four boundary steps must divide a power-of-two stride on the grid"
        )
    return steps


def triangles(side, stride, keep_boundary=True, boundary_steps=None):
    result = []
    top, right, bottom, left = edge_steps(side, stride, keep_boundary, boundary_steps)

    def at(x, y):
        return y * side + x

    for y in range(0, side - 1, stride):
        for x in range(0, side - 1, stride):
            edge = stride > 1 and (
                (x == 0 and left < stride)
                or (y == 0 and top < stride)
                or (x + stride == side - 1 and right < stride)
                or (y + stride == side - 1 and bottom < stride)
            )
            if not edge:
                a, b, c, d = (
                    at(x, y),
                    at(x + stride, y),
                    at(x + stride, y + stride),
                    at(x, y + stride),
                )
                result.extend(((a, d, c), (a, c, b)))
                continue
            perimeter = []
            perimeter.extend(
                at(i, y) for i in (range(x, x + stride, top) if y == 0 else (x,))
            )
            perimeter.extend(
                at(x + stride, j)
                for j in (
                    range(y, y + stride, right) if x + stride == side - 1 else (y,)
                )
            )
            perimeter.extend(
                at(i, y + stride)
                for i in (
                    range(x + stride, x, -bottom)
                    if y + stride == side - 1
                    else (x + stride,)
                )
            )
            perimeter.extend(
                at(x, j)
                for j in (range(y + stride, y, -left) if x == 0 else (y + stride,))
            )
            centre = at(x + stride // 2, y + stride // 2)
            result.extend(
                (centre, perimeter[(i + 1) % len(perimeter)], value)
                for i, value in enumerate(perimeter)
            )
    return np.asarray(result, dtype=np.int32)


def barycentric(points, faces):
    a, b, c = faces[:, 0], faces[:, 1], faces[:, 2]
    p, b, c = points - a, b - a, c - a
    det = b[:, 0] * c[:, 1] - b[:, 1] * c[:, 0]
    assert np.all(det != 0)
    v = (p[:, 0] * c[:, 1] - p[:, 1] * c[:, 0]) / det
    w = (b[:, 0] * p[:, 1] - b[:, 1] * p[:, 0]) / det
    return np.column_stack((1 - v - w, v, w))


def stencil(fractions, stride, keep_boundary=True, vertical=None, boundary_steps=None):
    side = len(fractions)
    vertical = fractions if vertical is None else vertical
    if len(vertical) != side:
        raise ValueError("both grid axes must have the same node count")
    xy = np.stack(np.meshgrid(fractions, vertical), axis=-1).reshape(-1, 2)
    fine, coarse = (
        triangles(side, 1),
        triangles(side, stride, keep_boundary, boundary_steps),
    )
    fine_polygons, coarse_polygons = (
        shapely.polygons(xy[fine]),
        shapely.polygons(xy[coarse]),
    )
    assert np.isclose(shapely.area(coarse_polygons).sum(), 1, rtol=0, atol=1e-12)
    union = shapely.union_all(coarse_polygons)
    assert union.is_valid and abs(union.area - 1) < 1e-12
    edges = np.sort(coarse[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
    unique, counts = np.unique(edges, axis=0, return_counts=True)
    assert np.all((counts == 1) | (counts == 2))
    required = []
    for edge, step in enumerate(
        edge_steps(side, stride, keep_boundary, boundary_steps)
    ):
        for i in range(0, side - 1, step):
            required.append(
                (
                    (i, i + step),
                    (i * side + side - 1, (i + step) * side + side - 1),
                    ((side - 1) * side + i, (side - 1) * side + i + step),
                    (i * side, (i + step) * side),
                )[edge]
            )
    assert set(map(tuple, unique[counts == 1])) == set(required)
    coarse_ids, fine_ids = shapely.STRtree(fine_polygons).query(
        coarse_polygons, predicate="intersects"
    )
    overlaps = shapely.intersection(
        fine_polygons[fine_ids], coarse_polygons[coarse_ids]
    )
    selected = shapely.area(overlaps) > 0
    fine_ids, coarse_ids, overlaps = (
        fine_ids[selected],
        coarse_ids[selected],
        overlaps[selected],
    )
    points, owners = shapely.get_coordinates(overlaps, return_index=True)
    _, retained = np.unique(points, axis=0, return_index=True)
    points, owners = points[retained], owners[retained]
    f, c = fine[fine_ids[owners]], coarse[coarse_ids[owners]]
    retained = ~np.any(np.all(points[:, None, :] == xy[c], axis=2), axis=1)
    points, f, c = points[retained], f[retained], c[retained]
    fw, cw = barycentric(points, xy[f]), barycentric(points, xy[c])
    assert min(fw.min(), cw.min()) >= -1e-12
    return (
        np.concatenate((f, c), axis=1),
        np.concatenate((fw, -cw), axis=1),
        len(coarse),
    )


def ecef(lon, lat, height=0):
    lon, lat = np.deg2rad(lon), np.deg2rad(lat)
    n = 6378137 / np.sqrt(1 - 0.0066943799901413165 * np.sin(lat) ** 2)
    return np.stack(
        (
            (n + height) * np.cos(lat) * np.cos(lon),
            (n + height) * np.cos(lat) * np.sin(lon),
            (n * (1 - 0.0066943799901413165) + height) * np.sin(lat),
        ),
        axis=-1,
    )


def read_page(row, directory):
    side, halo = int(row["side"]), int(row["halo"])
    height = np.fromfile(directory / (row["page"] + ".f32"), dtype="<f4").reshape(
        side + 2 * halo, -1
    )
    assert halo == 1 and np.isfinite(height).all()
    fractions = (
        np.loadtxt(directory / (row["page"] + ".fractions"))
        .astype("float32")
        .astype(float)
    )
    return height, fractions


def page_frame(row):
    z, x, y = (int(row[k]) for k in ("zoom", "x", "y"))
    scale = 2**z
    lon = np.array((x, x + 1)) / scale * 360 - 180
    lat = np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * np.array((y, y + 1)) / scale))))
    mid_lon, mid_lat = lon.mean(), lat.mean()
    l, p = np.deg2rad((mid_lon, mid_lat))
    axes = np.array(
        (
            (-np.sin(l), np.cos(l), 0),
            (-np.sin(p) * np.cos(l), -np.sin(p) * np.sin(l), np.cos(p)),
            (np.cos(p) * np.cos(l), np.cos(p) * np.sin(l), np.sin(p)),
        )
    )
    centre = ecef(mid_lon, mid_lat)
    corners = np.einsum(
        "ij,kj->ik", ecef(lon[[0, 1, 0, 1]], lat[[0, 0, 1, 1]]) - centre, axes[:2]
    )
    return centre, axes, corners.astype("float32").astype(float)


def page_steps(corners, side):
    step_e = float(
        np.float32(
            0.5
            * ((corners[1, 0] - corners[0, 0]) + (corners[3, 0] - corners[2, 0]))
            / (side - 1)
        )
    )
    step_n = float(
        np.float32(
            0.5
            * ((corners[0, 1] - corners[2, 1]) + (corners[1, 1] - corners[3, 1]))
            / (side - 1)
        )
    )
    return step_e, step_n


def page_geometry(row, directory):
    height, fractions = read_page(row, directory)
    side = int(row["side"])
    centre, axes, corners = page_frame(row)
    u, v = np.meshgrid(fractions, fractions)
    en = (1 - v[..., None]) * (
        (1 - u[..., None]) * corners[0] + u[..., None] * corners[1]
    ) + v[..., None] * ((1 - u[..., None]) * corners[2] + u[..., None] * corners[3])
    sag_inv = float(np.float32(1 / np.linalg.norm(centre)))
    h = height[1:-1, 1:-1].astype(float) - 0.5 * np.sum(en * en, axis=-1) * sag_inv
    local = np.concatenate((en, h[..., None]), axis=-1).reshape(-1, 3)
    step_e, step_n = page_steps(corners, side)
    de = (height[1:-1, 2:] - height[1:-1, :-2]) / (2 * max(step_e, 0.001))
    dn = (height[:-2, 1:-1] - height[2:, 1:-1]) / (2 * max(step_n, 0.001))
    normals = (
        np.stack((-de, -dn, np.ones_like(de)), axis=-1).reshape(-1, 3).astype(float)
    )
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    world = np.einsum("ij,jk->ik", local, axes) + centre
    assert (
        np.isfinite(local).all()
        and np.isfinite(world).all()
        and np.isfinite(normals).all()
    )
    fingerprint = hashlib.sha256(height.tobytes() + fractions.tobytes()).digest()
    return fractions, local, normals, world, fingerprint


def camera(scenario, rows, directory):
    root = ET.parse(scenario).getroot()
    view = root.find("views/view[@id='station']")
    at = view.find("at")
    lat, lon, height = (float(at.get(k)) for k in ("lat", "lon", "heightM"))
    if at.get("samplesHeight") == "yes":
        for index in sorted(range(len(rows)), key=lambda i: -int(rows[i]["zoom"])):
            row = rows[index]
            scale = 2 ** int(row["zoom"])
            u = (lon + 180) / 360 * scale - int(row["x"])
            v = (1 - np.arcsinh(np.tan(np.deg2rad(lat))) / np.pi) / 2 * scale - int(
                row["y"]
            )
            if not (0 <= u <= 1 and 0 <= v <= 1):
                continue
            h, f = read_page(row, directory)
            h = h[1:-1, 1:-1]
            i, j = np.clip(np.searchsorted(f, (u, v), side="right") - 1, 0, len(f) - 2)
            a, b = (u - f[i]) / (f[i + 1] - f[i]), (v - f[j]) / (f[j + 1] - f[j])
            height += (
                h[j, i]
                + a * (h[j, i + 1] - h[j, i])
                + b * (h[j + 1, i + 1] - h[j, i + 1])
                if a >= b
                else h[j, i]
                + b * (h[j + 1, i] - h[j, i])
                + a * (h[j + 1, i + 1] - h[j + 1, i])
            )
            break
        else:
            raise ValueError("camera has no native cached height page")
    return (
        ecef(lon, lat, height),
        float(view.get("fovDeg")),
        {"lat": lat, "lon": lon, "height_m": float(height)},
    )


def raster_view(scenario, rows, pages, width, height, yaw):
    eye, fov, _ = camera(scenario, rows, pages)
    at = ET.parse(scenario).getroot().find("views/view[@id='station']/at")
    lat, lon = np.deg2rad([float(at.get("lat")), float(at.get("lon"))])
    east = np.array([-np.sin(lon), np.cos(lon), 0])
    north = np.array(
        [-np.sin(lat) * np.cos(lon), -np.sin(lat) * np.sin(lon), np.cos(lat)]
    )
    up = np.cross(east, north)
    bearing, pitch = np.deg2rad(
        [float(at.get("bearingDeg", 0)) + yaw, float(at.get("pitchDeg", 0))]
    )
    forward = (
        np.cos(pitch) * (np.cos(bearing) * north + np.sin(bearing) * east)
        + np.sin(pitch) * up
    )
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    return (
        eye,
        np.stack([right, np.cross(right, forward), forward]),
        height / (2 * np.tan(np.deg2rad(fov) / 2)),
        width,
        height,
    )


def compare(directory, scenario, keep_boundary=True):
    started = time.perf_counter()
    with (directory / "pages.csv").open() as source:
        rows = list(csv.DictReader(source))
    eye, fov, pose = camera(scenario, rows, directory)
    fingerprint = hashlib.sha256((directory / "pages.csv").read_bytes())
    strides = (1, 2, 4, 8, 16, 32)
    errors, normal_errors, distances, counts = [], [], [], None
    templates = {}
    for row in rows:
        fractions, local, normals, world, page_hash = page_geometry(row, directory)
        fingerprint.update(page_hash)
        assert len(fractions) == 33
        low, high = world.min(axis=0), world.max(axis=0)
        distances.append(
            max(
                0,
                np.linalg.norm((low + high) / 2 - eye)
                - np.linalg.norm((high - low) / 2),
            )
        )
        bounds, normal_bounds, counts = [0.0], [0.0], [2304]
        for stride in strides[1:]:
            key = (fractions.tobytes(), stride)
            if key not in templates:
                templates[key] = stencil(fractions, stride, keep_boundary)
            indices, weights, triangles_count = templates[key]
            delta = np.einsum("ijk,ij->ik", local[indices], weights)
            n_delta = np.einsum("ijk,ij->ik", normals[indices], weights)
            bounds.append(float(np.linalg.norm(delta, axis=1).max()))
            normal_bounds.append(float(np.linalg.norm(n_delta, axis=1).max()))
            counts.append(triangles_count + (256 if keep_boundary else 256 // stride))
        errors.append(bounds)
        normal_errors.append(normal_bounds)
    errors, normal_errors, distances = (
        np.asarray(errors),
        np.asarray(normal_errors),
        np.asarray(distances),
    )
    focal = 720 / (2 * np.tan(np.deg2rad(fov / 2)))
    cosine = 1 / np.sqrt(1 + (1 + (1280 / 720) ** 2) * np.tan(np.deg2rad(fov / 2)) ** 2)
    denominator = cosine * (distances[:, None] * cosine - errors)
    projected = np.divide(
        focal * errors,
        denominator,
        out=np.full_like(errors, np.inf),
        where=denominator > 0,
    )
    projected[:, 0] = 0
    comparisons = []
    for pixels in (0.25, 0.5, 1.0, 2.0, 4.0):
        selected = np.max(
            np.where(projected <= pixels, np.arange(len(strides)), 0), axis=1
        )
        comparisons.append(
            {
                "pixel_allowance": pixels,
                "triangles_all_resident": int(np.asarray(counts)[selected].sum()),
                "pages_by_stride": {
                    str(s): int((selected == i).sum()) for i, s in enumerate(strides)
                },
                "max_normal_vector_deviation": float(
                    normal_errors[np.arange(len(rows)), selected].max()
                ),
            }
        )
    return {
        "pages": len(rows),
        "camera": pose,
        "profile": "1280x720",
        "boundary_mode": "original"
        if keep_boundary
        else "coarse; neighbor transitions unproved",
        "fov_deg": fov,
        "strides": strides,
        "triangles_per_patch": counts,
        "baseline_triangles_all_resident": 2304 * len(rows),
        "comparisons": comparisons,
        "seconds": time.perf_counter() - started,
        "max_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "input_sha256": fingerprint.hexdigest(),
        "template_count": len(templates),
        "geometry_error_quantiles_m": np.percentile(
            errors, (0, 50, 95, 100), axis=0
        ).tolist(),
        "nearest_sphere_distance_quantiles_m": np.percentile(
            distances, (0, 50, 95, 100)
        ).tolist(),
        "stencil_points": {
            str(key[1]): len(value[0]) for key, value in templates.items()
        },
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "scope": "CPU common-refinement geometry bound for existing native height pages including sag; "
        "boundary mode and skirt counts are explicit. All-resident counts are not submitted "
        "GPU work. Float shader rounding, projected runtime images, normal/material quality, "
        "shadow/collision and device costs remain unproved.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pages", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--coarse-boundary", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("output must be in system temp")
    output.unlink(missing_ok=True)
    report = compare(args.pages, args.scenario, not args.coarse_boundary)
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()

"""Build a near-canopy mesh from a native bark mesh and clustered needle shoots."""

import argparse
import hashlib
import json
from pathlib import Path
from time import perf_counter

import numpy as np
from outshine.rendering.far_cubemap import unit
from pack_surface_tile import HEADER, INDEX, VERTEX


def mesh(folder):
    return (np.fromfile(folder / "positions.f32", "<f4").reshape(-1, 3),
            np.fromfile(folder / "normals.f32", "<f4").reshape(-1, 3),
            np.fromfile(folder / "indices.u32", "<u4").reshape(-1, 3))


def shoot_samples(prototype):
    points, _, indices = mesh(prototype / "twig")
    triangles = points[indices]
    edges = triangles[:, 1:] - triangles[:, :1]
    area = .5 * np.linalg.norm(np.cross(edges[:, 0], edges[:, 1]), axis=1)
    centroid = np.average(triangles.mean(axis=1), weights=area, axis=0)
    frames = np.fromfile(prototype / "placements.f32", "<f4").reshape(-1, 4, 4)
    positions = frames[:, 3, :3] + centroid @ frames[:, :3, :3]
    blade_scale = np.linalg.norm(frames[:, 2, :3], axis=1)
    return positions, unit(frames[:, 2, :3]), blade_scale**2 * area.sum()


def voxel_planes(positions, normals, area, cell, coverage_gain):
    coordinates = np.floor(positions / cell).astype(np.int32)
    occupied, inverse = np.unique(coordinates, axis=0, return_inverse=True)
    count = len(occupied)
    cell_area = cell**2
    axes = np.eye(3)
    dominant = np.argmax(np.abs(normals), axis=1)
    rng = np.random.default_rng(0)
    meshes = []
    for axis in range(3):
        weights = np.where(dominant == axis, area * np.abs(normals[:, axis]), 0)
        mass = np.bincount(inverse, weights, minlength=count)
        coverage = -np.expm1(-coverage_gain * mass / cell_area)
        active = coverage >= 1 / 255
        if not active.any():
            continue
        centre = np.column_stack([np.bincount(inverse, weights * v, minlength=count)[active]
                                  for v in positions.T]) / mass[active, None]
        signed = weights * np.where(normals[:, axis] < 0, -1, 1)
        shading = np.column_stack([np.bincount(inverse, signed * v, minlength=count)[active]
                                   for v in normals.T])
        shading = unit(shading)
        tangent = unit(np.cross(shading, axes[(axis + 1) % 3]))
        bitangent = np.cross(shading, tangent)
        angle = rng.random(len(shading))[:, None] * (2 * np.pi)
        dx = tangent * np.cos(angle) + bitangent * np.sin(angle)
        dy = np.cross(shading, dx)
        size = .5 * cell * np.sqrt(coverage[active] / shading[:, axis])
        corners = np.stack((-dx-dy, dx-dy, dx+dy, -dx+dy), axis=1)
        points = centre[:, None, :] + size[:, None, None] * corners
        faces = np.arange(len(centre), dtype=np.uint32)[:, None, None] * 4 + [
            [0, 1, 2], [0, 2, 3]]
        meshes.append((points.reshape(-1, 3), np.repeat(shading, 4, axis=0),
                       faces.reshape(-1, 3)))
    return meshes


def canopy_mesh(prototype, cell, coverage_gain):
    metadata = json.loads((prototype / "prototype.json").read_text())
    positions, normals, area = shoot_samples(prototype)
    wood = mesh(prototype / "bark")
    foliage = voxel_planes(positions, normals, area, cell, coverage_gain)
    parts = [(wood, metadata["bark"]["colour"]), *[
        (plane, metadata["twig"]["colour"]) for plane in foliage]]
    offset = 0
    xyz, normal, faces, colours = [], [], [], []
    for (points, shades, indices), colour in parts:
        xyz.append(points)
        normal.append(shades)
        faces.append(indices + offset)
        colours.append(np.tile(np.rint(255 * np.clip(colour, 0, 1)), (len(points), 1)))
        offset += len(points)
    return (np.concatenate(xyz), np.concatenate(normal), np.concatenate(faces),
            np.concatenate(colours).astype("u1"))


def placed(geometry, roots, scales):
    points, normals, faces, colours = geometry
    count = len(roots)
    xyz = roots[:, None, :] + scales[:, None, None] * points
    shading = np.tile(normals, (count, 1))
    faces = faces + (len(points) * np.arange(count, dtype=np.uint32))[:, None, None]
    return (xyz.reshape(-1, 3), shading, faces.reshape(-1, 3), np.tile(colours, (count, 1)))


def prepare_tile(surface):
    raw = surface.read_bytes()
    header = list(HEADER.unpack_from(raw))
    if header[0] != b"OUTSHST5":
        raise ValueError("the preview requires a packed surface with a material grid")
    vertex_count, index_count = header[-2:]
    start = HEADER.size + 2 * INDEX.size
    end_vertices = start + vertex_count * VERTEX.size
    end_indices = end_vertices + index_count * INDEX.size
    if len(raw) != end_indices + vertex_count * 4:
        raise ValueError("unexpected packed surface span")
    return header, raw[HEADER.size:start], raw[start:end_vertices], raw[end_vertices:end_indices], raw[end_indices:]


def overlay(packed, output, geometry):
    header, split, vertices, indices, colours = packed
    xyz, normal, faces, rgba = geometry
    data = np.column_stack((xyz, normal)).astype("<f4")
    index = (faces + header[-2]).astype("<u4")
    header[-2] += len(xyz)
    header[-1] += index.size
    if header[-2] > 65536 or header[-1] > 6 * 65536:
        raise ValueError("expanded control exceeds the surface tile format")
    header[7:10] = np.minimum(header[7:10], data[:, :3].min(axis=0))
    header[10:13] = np.maximum(header[10:13], data[:, :3].max(axis=0))
    stamp = hashlib.sha256(header[6])
    stamp.update(data)
    stamp.update(index)
    stamp.update(rgba)
    header[6] = stamp.hexdigest().encode("ascii")
    with output.open("xb") as file:
        for chunk in (HEADER.pack(*header), split, vertices, data, indices, index, colours, rgba):
            file.write(chunk)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prototype", type=Path)
    parser.add_argument("surface", type=Path)
    parser.add_argument("layout", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--cell-m", type=float, required=True)
    parser.add_argument("--coverage-gain", type=float, default=1)
    opts = parser.parse_args()
    rates = np.array([opts.cell_m, opts.coverage_gain])
    if not (np.isfinite(rates) & (rates > 0)).all():
        raise ValueError("positive cell size and foliage area factor required")
    began = perf_counter()
    geometry = canopy_mesh(opts.prototype, opts.cell_m, opts.coverage_gain)
    layout = json.loads(opts.layout.read_text())
    roots, scales = (np.asarray(layout[k]) for k in ("roots_m", "tree_scales"))
    packed = prepare_tile(opts.surface)
    header = packed[0]
    if (list(header[1:4]) != layout["surface"]["coverage"]
            or list(header[4:6]) != layout["surface"]["origin_lon_lat"]
            or header[6].decode("ascii") != layout["surface"]["revision"]):
        raise ValueError("roots must match the exact source terrain version")
    detail = placed(geometry, roots, scales)
    overlay(packed, opts.output, detail)
    print(json.dumps({"instances": len(roots), "prototype_triangles": len(geometry[2]),
                      "overlay_triangles": len(detail[2]),
                      "prepare_ms": (perf_counter() - began) * 1000}))


if __name__ == "__main__":
    main()

"""Joint first-hit normal measures, independent of stored original triangle count."""

import itertools
import json
from pathlib import Path

import numpy as np
from opaque_sample_grid import samples as raster_samples
from triangle_image_reference import intersect_source, prepare_source


def frame(positions, observers, up):
    forward = np.mean(positions, axis=0) - np.mean(observers, axis=0)
    forward /= np.linalg.norm(forward)
    right = np.cross(up, forward)
    if np.linalg.norm(right) < 1e-8:
        right = np.cross(np.eye(3)[np.argmin(abs(forward))], forward)
    right /= np.linalg.norm(right)
    return np.stack((right, np.cross(forward, right), forward))


def grid(source, observer_box, up, focal, *, texel_px=0.5, samples=4):
    if not np.isfinite(focal) or focal <= 0 or texel_px != 0.5 or samples != 4:
        raise ValueError(
            "declared half-projector-pixel/4x4 quadrature profile required"
        )
    active_axes = np.flatnonzero(np.ptp(observer_box, axis=0) > 0)
    bits = np.array(list(itertools.product((0, 1), repeat=len(active_axes))), np.uint8)
    corners = np.zeros((len(bits), 3), np.uint8)
    corners[:, active_axes] = bits
    eyes = observer_box[0] + corners * np.ptp(observer_box, axis=0)
    basis = frame(source["positions"], eyes, up)
    angular = []
    for eye in eyes:
        points = (source["positions"].astype(float) - eye) @ basis.T
        if points[:, 2].min() <= 0.2:
            raise ValueError("group needs a different directional domain")
        angular.append(points[:, :2] / points[:, 2, None])
    q = np.concatenate(angular)
    pitch = texel_px / focal
    low = q.min(0) - 2 * pitch
    shape = np.ceil(np.ptp(q, axis=0) / pitch).astype(int) + 4
    if len(eyes) * int(np.prod(shape)) * samples**2 > 4194304:
        raise ValueError(
            "group exceeds bounded quadrature work; smaller source groups required"
        )
    return eyes, corners, basis, low, shape, pitch


def histogram(texels, words, size, denominator):
    keys = (np.asarray(texels, np.uint64) << np.uint64(32)) | np.asarray(
        words, np.uint64
    )
    unique, counts = np.unique(keys, return_counts=True)
    owners = (unique >> np.uint64(32)).astype(np.int64)
    offsets = np.r_[
        0, np.cumsum(np.bincount(owners, minlength=size), dtype=np.uint32)
    ].astype(np.uint32)
    return offsets, unique.astype(np.uint32), (counts / denominator).astype(np.float32)


def cook(
    source,
    observer_box,
    up,
    profile,
    source_records,
    path,
    provenance,
    *,
    opaque_control=False,
    sampling="raster",
):
    if opaque_control is not True:
        raise ValueError("explicit homogeneous opaque control required")
    observer_box = np.asarray(observer_box, float)
    if (
        observer_box.shape != (2, 3)
        or not np.isfinite(observer_box).all()
        or np.any(observer_box[0] > observer_box[1])
    ):
        raise ValueError("finite ordered actual observer domain required")
    if not np.all(
        source["normal"][source["indices"]]
        == source["normal"][source["indices"][:, :1]]
    ):
        raise ValueError(
            "this probe requires the supplied flat packed-normal triangles"
        )
    eyes, corners, basis, low, shape, pitch = grid(
        source, observer_box, up, profile["focal_px"]
    )
    width, height = map(int, shape)
    size, samples = width * height, 4
    yy, xx = np.indices((height * samples, width * samples))
    texels = (yy // samples * width + xx // samples).ravel()
    q = low + (np.column_stack((xx.ravel(), yy.ravel())) + 0.5) * (pitch / samples)
    directions = np.column_stack((q, np.ones(len(q)))) @ basis
    if sampling not in ("raster", "ray"):
        raise ValueError("explicit raster or independent ray sampler required")
    prepared = prepare_source(source) if sampling == "ray" else None
    order = np.argsort([r["index"] for r in source_records])
    records = [source_records[i] for i in order]
    table = np.zeros(
        len(records),
        dtype=[("ordinal", "<u4"), ("kind", "u1"), ("id", "<u8"), ("samples", "<u4")],
    )
    for i, record in enumerate(records):
        table[i] = (record["index"], record["kind"], record["id"], 0)
    z0 = float(np.mean(source["positions"], axis=0) @ basis[2])
    bins, words, first, second = [], [], [], []
    work = {
        "rays": 0,
        "first_hits": 0,
        "source_triangles": len(source["indices"]),
        "source_vertices": len(source["positions"]),
        "triangle_pixel_candidates": 0,
    }
    for i, eye in enumerate(eyes):
        origins = np.broadcast_to(eye, directions.shape)
        if sampling == "ray":
            depth, faces = intersect_source(
                source, origins, directions, prepared=prepared
            )
        else:
            depth, faces, raster_work = raster_samples(
                source, eye, basis, low, np.repeat(pitch / samples, 2), shape * samples
            )
            work["triangle_pixel_candidates"] += raster_work[
                "triangle_pixel_candidates"
            ]
        hit = faces >= 0
        bins.append(texels[hit] + i * size)
        words.append(source["normal"][source["indices"][faces[hit], 0]])
        z = eye @ basis[2] + depth[hit] - z0
        first.append(np.bincount(texels[hit], weights=z, minlength=size) / samples**2)
        second.append(
            np.bincount(texels[hit], weights=z * z, minlength=size) / samples**2
        )
        rows = np.searchsorted(table["ordinal"], source["binding"][faces[hit], 1])
        if np.any(rows >= len(table)) or not np.array_equal(
            table["ordinal"][rows], source["binding"][faces[hit], 1]
        ):
            raise ValueError("normal sample has no validated original source identity")
        table["samples"] += np.bincount(rows, minlength=len(table)).astype(np.uint32)
        work["rays"] += len(depth)
        work["first_hits"] += int(hit.sum())
    offsets, normal_words, weights = histogram(
        np.concatenate(bins), np.concatenate(words), len(eyes) * size, samples**2
    )
    arrays = {
        "offsets": offsets,
        "normal_words": normal_words,
        "weights": weights,
        "depth_mass": np.asarray(first, np.float32).ravel(),
        "depth2_mass": np.asarray(second, np.float32).ravel(),
        "basis": basis,
        "low": low,
        "shape": shape.astype(np.uint32),
        "pitch": np.array(pitch),
        "eyes": eyes,
        "corner_bits": corners,
        "z0": np.array(z0),
        "sources": table,
    }
    path = Path(path)
    path.mkdir(parents=True, exist_ok=False)
    np.savez_compressed(path / "measure.npz", **arrays)
    header = {
        "format": "filtered-visible-normal-measure-1",
        "provenance": provenance,
        "observer_bounds": observer_box.tolist(),
        "profile": profile,
        "quadrature_per_texel": samples**2,
        "projector_texel_px": 0.5,
        "sampler": sampling,
        "source_records": len(records),
        "unsampled_sources": [
            {
                "ordinal": int(r["ordinal"]),
                "reason": "no first hit at training observer corners/quadrature; original logical/caster source retained",
            }
            for r in table
            if not r["samples"]
        ],
        "arrays": {k: a.nbytes for k, a in arrays.items()},
        "stored_array_bytes": sum(a.nbytes for a in arrays.values()),
        "container_bytes": (path / "measure.npz").stat().st_size,
        "work": work,
        "scope": "homogeneous opaque GGX controls, empirical first-hit coverage/normal distribution; no native material bindings, UV/code interpolation, external depth composition, shadows or world/device admission",
    }
    (path / "header.json").write_text(json.dumps(header, indent=2) + "\n")
    return header

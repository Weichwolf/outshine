"""Relight a finished first-hit measure without original geometry or baked light."""

import json
from pathlib import Path

import numpy as np
from packed_normals import unpack
from viewcell_product import require_domain
from visible_normal_material import shade


def load_product(path, observers, provenance):
    path = Path(path)
    header = json.loads((path / "header.json").read_text())
    if (
        header["format"] != "filtered-visible-normal-measure-1"
        or header["provenance"] != provenance
    ):
        raise ValueError("visible normal measure identity mismatch")
    require_domain(header["observer_bounds"], observers)
    with np.load(path / "measure.npz", allow_pickle=False) as archive:
        arrays = {k: archive[k] for k in archive.files}
    normals = unpack(arrays["normal_words"])
    rows = np.repeat(np.arange(len(arrays["offsets"]) - 1), np.diff(arrays["offsets"]))
    coverage = np.bincount(
        rows, weights=arrays["weights"], minlength=len(arrays["offsets"]) - 1
    )
    mean = np.zeros((len(coverage), 3))
    np.add.at(mean, rows, normals * arrays["weights"][:, None])
    counts = np.diff(arrays["offsets"])
    return {
        "header": header,
        "arrays": arrays,
        "normals": normals,
        "coverage": coverage,
        "mean_mass": mean,
        "counts": counts,
        "derived_cpu_bytes": normals.nbytes
        + coverage.nbytes
        + mean.nbytes
        + counts.nbytes,
        "work": {
            "rays": 0,
            "texel_queries": 0,
            "mode_evaluations": 0,
            "largest_mode_pairs": 0,
        },
    }


def texel_pairs(product, origins, directions):
    a = product["arrays"]
    d = directions @ a["basis"].T
    high = a["low"] + a["shape"] * a["pitch"]
    active = (
        (d[:, 2] > 0)
        & np.all(d[:, :2] >= (a["low"] - a["pitch"]) * d[:, 2, None], axis=1)
        & np.all(d[:, :2] <= (high + a["pitch"]) * d[:, 2, None], axis=1)
    )
    q = np.divide(
        d[:, :2], d[:, 2, None], out=np.zeros((len(d), 2)), where=active[:, None]
    )
    at = (q - a["low"]) / a["pitch"] - 0.5
    first = np.floor(at).astype(np.int64)
    fraction = at - first
    domain = np.array(product["header"]["observer_bounds"])
    width = np.ptp(domain, axis=0)
    position = np.divide(
        origins - domain[0], width, out=np.zeros_like(origins), where=width > 0
    )
    w, h = map(int, a["shape"])
    for eye, bits in enumerate(a["corner_bits"]):
        weight = np.where(bits, position, 1 - position)
        weight[:, width == 0] = 1
        eye_weight = weight.prod(1)
        for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
            xy = first + [x, y]
            good = (
                active
                & (eye_weight > 0)
                & np.all(xy >= 0, axis=1)
                & (xy[:, 0] < w)
                & (xy[:, 1] < h)
            )
            rays = np.flatnonzero(good)
            coefficient = (
                eye_weight[rays]
                * (fraction[rays, 0] if x else 1 - fraction[rays, 0])
                * (fraction[rays, 1] if y else 1 - fraction[rays, 1])
            )
            rows = eye * w * h + xy[rays, 1] * w + xy[rays, 0]
            yield rays, rows, coefficient


def query(product, origins, directions, light, roughness, energy):
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    require_domain(product["header"]["observer_bounds"], origins)
    lengths = np.linalg.norm(directions, axis=1)
    profile = product["header"]["profile"]
    maximum = np.sqrt(
        1
        + (profile["width"] / (2 * profile["focal_px"])) ** 2
        + (profile["height"] / (2 * profile["focal_px"])) ** 2
    )
    if (
        directions.shape != origins.shape
        or not np.isfinite(directions).all()
        or not np.isfinite(lengths).all()
        or np.any(lengths == 0)
        or np.any(lengths > maximum * (1 + 1e-12))
    ):
        raise ValueError("finite nonzero rays required")
    if roughness not in (0.12, 0.45):
        raise ValueError("declared homogeneous GGX control roughness required")
    a = product["arrays"]
    rgb = np.zeros((len(origins), 3))
    coverage, depth_mass, depth2_mass = (np.zeros(len(origins)) for _ in range(3))
    mean_mass = np.zeros_like(rgb)
    work = product["work"]
    work["rays"] += len(origins)
    for begin in range(0, len(origins), 256):
        o, d = origins[begin : begin + 256], directions[begin : begin + 256]
        for rays, rows, coefficient in texel_pairs(product, o, d):
            target = begin + rays
            coverage[target] += coefficient * product["coverage"][rows]
            depth_mass[target] += coefficient * a["depth_mass"][rows]
            depth2_mass[target] += coefficient * a["depth2_mass"][rows]
            mean_mass[target] += coefficient[:, None] * product["mean_mass"][rows]
            offsets, counts = a["offsets"][rows], product["counts"][rows]
            rr = np.repeat(rays, counts)
            starts = np.repeat(np.cumsum(counts, dtype=np.int64) - counts, counts)
            modes = (np.repeat(offsets, counts) + np.arange(len(rr)) - starts).astype(
                np.int64
            )
            weights = np.repeat(coefficient, counts) * a["weights"][modes]
            work["texel_queries"] += len(rays)
            work["mode_evaluations"] += len(rr)
            work["largest_mode_pairs"] = max(work["largest_mode_pairs"], len(rr))
            if len(rr):
                response = shade(
                    product["normals"][modes], -d[rr], light, roughness, energy
                )
                np.add.at(rgb, begin + rr, weights[:, None] * response)
    covered = coverage > 0
    mean = np.divide(
        mean_mass, coverage[:, None], out=np.zeros_like(rgb), where=covered[:, None]
    )
    length = np.linalg.norm(mean, axis=1)
    valid_mean = covered & (length > 0)
    averaged = np.zeros_like(rgb)
    averaged[valid_mean] = (
        shade(
            mean[valid_mean],
            -directions[valid_mean],
            light,
            roughness,
            energy,
            mean_length=length[valid_mean],
        )
        * coverage[valid_mean, None]
    )
    depth = np.full(len(origins), np.inf)
    zmean = np.divide(depth_mass, coverage, out=np.zeros(len(origins)), where=covered)
    dz = directions @ a["basis"][2]
    depth[covered] = (a["z0"] + zmean[covered] - origins[covered] @ a["basis"][2]) / dz[
        covered
    ]
    variance = np.maximum(
        0,
        np.divide(depth2_mass, coverage, out=np.zeros(len(origins)), where=covered)
        - zmean**2,
    )
    return {
        "rgb": rgb,
        "mean_rgb": averaged,
        "coverage": coverage,
        "mean_depth": depth,
        "depth_variance": variance,
        "undefined_mean": int((covered & ~valid_mean).sum()),
    }

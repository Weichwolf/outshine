"""Nonnegative geometry-only cosine exchange, sampled once for both sheet sides."""

import numpy as np
from diffuse_scatter import sample_sheet, uniform_samples
from scipy.sparse import csr_matrix


def build_links(
    scene, points, normals, samples, map_source, seed=997, packet=8192, bias=2e-6
):
    count = len(points)
    if (
        points.shape != normals.shape
        or points.shape != (count, 3)
        or not 0 < count <= 500000
        or not np.isfinite(points).all()
        or not np.isfinite(normals).all()
        or np.any(abs(np.linalg.norm(normals, axis=1) - 1) > 1e-10)
        or type(samples) is not int
        or not 1 <= samples <= 65536
        or samples & (samples - 1)
        or count * 2 * samples > 33554432
        or type(packet) is not int
        or not 1 <= packet <= 65536
        or not np.isfinite(bias)
        or not 1e-7 <= bias <= 1e-3
    ):
        raise ValueError(
            "bounded finite basis, power-of-two samples and explicit query budget required"
        )
    links = np.full((count * 2, samples), -1, np.int32)
    for first in range(0, links.size, packet):
        ids = np.arange(first, min(first + packet, links.size))
        row = ids // samples
        node = row // 2
        pole = normals[node] * np.where(row % 2 == 0, 1, -1)[:, None]
        optical = np.c_[np.ones((len(ids), 3)), np.zeros((len(ids), 3))]
        direction, _ = sample_sheet(
            optical, pole, pole, uniform_samples(seed, first, len(ids))
        )
        hits, where, hit_normals, _ = scene.nearest(
            points[node] + bias * pole, direction, scene.index.scale + 1
        )
        chosen = hits >= 0
        mapped = map_source(hits[chosen], where[chosen])
        if np.any((mapped < 0) | (mapped >= count)):
            raise ValueError("all hit sources must belong to the complete basis")
        side = np.sum(hit_normals[chosen] * (-direction[chosen]), axis=1) < 0
        links.ravel()[ids[chosen]] = 2 * mapped + side
    return links


def validate_links(links):
    if (
        links.ndim != 2
        or links.dtype.kind not in "iu"
        or len(links) % 2
        or links.size > 33554432
        or not links.shape[1]
        or links.min(initial=0) < -1
        or links.max(initial=-1) >= len(links)
    ):
        raise ValueError(
            "bounded complete two-sided link rows with explicit misses required"
        )


def prepare_csr(links):
    validate_links(links)
    valid = links >= 0
    pointer = np.r_[0, np.cumsum(valid.sum(1))].astype(np.int32)
    indices = links[valid].astype(np.int32)
    data = np.full(len(indices), 1 / links.shape[1])
    result = csr_matrix((data, indices, pointer), shape=(len(links), len(links)))
    result.sum_duplicates()
    result.sort_indices()
    return result


def gather_apply(links, values, rows=2048):
    validate_links(links)
    if (
        values.shape != (len(links), 3)
        or not np.isfinite(values).all()
        or type(rows) is not int
        or not 1 <= rows <= 65536
    ):
        raise ValueError("matching finite RGB values required")
    result = np.empty_like(values)
    rows = min(rows, max(1, 65536 // links.shape[1]))
    for first in range(0, len(links), rows):
        ids = links[first : first + rows]
        gathered = values[np.maximum(ids, 0)]
        gathered[ids < 0] = 0
        result[first : first + rows] = gathered.mean(1)
    return result


def mix_sides(incoming, optical):
    if incoming.shape != (len(optical) * 2, 3):
        raise ValueError("both incoming hemispheres per optical node required")
    incident = incoming.reshape(-1, 2, 3)
    r, t = optical[:, :3], optical[:, 3:]
    result = np.empty_like(incident)
    result[:, 0] = r * incident[:, 0] + t * incident[:, 1]
    result[:, 1] = t * incident[:, 0] + r * incident[:, 1]
    return result.reshape(-1, 3)

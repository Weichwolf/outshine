"""Full captured-weight material composition; actual class selection stays native."""

from functools import partial

import numpy as np
from probe_terrain_material_substrate import prepared_sources
from terrain_material_lattice import analytic, evaluate, prepare
from terrain_material_litter import compose, wearing
from terrain_material_noise import MATRICES, raw_noise, rock_response
from terrain_material_plane import hash_words


def mean(palette, rows, which):
    row = which if 0 <= which < rows else rows
    return palette[4 + row * 4 : 8 + row * 4]


def counted_words(source, work):
    key = (
        "analytic_hash_words_queried"
        if source is hash_words
        else "prepared_words_queried"
    )

    def read(cells):
        work[key] += len(cells)
        return source(cells)

    return read


def substrate(capture, which, world, dx, dy, *, prepared, work):
    palette, rows = capture.palette, int(capture.palette_words[0])
    material = which if 0 <= which < rows else rows
    unmapped = capture.metadata["unmapped_material_row_i32"]
    row = material if material < rows else unmapped if 0 <= unmapped < rows else rows
    offset = 4 * (rows + 2) + rows + 1 + 8 * row
    layers = int(capture.palette_words[3])
    recipe = palette[offset : offset + 4]
    coverage = float(palette[offset + 7]) if layers and material < rows else 0
    difference = (
        palette[layers + material * 8 : layers + material * 8 + 4]
        if coverage
        else np.zeros(4)
    )
    litter = (
        palette[layers + material * 8 + 4 : layers + material * 8 + 8]
        if coverage
        else recipe
    )
    read = counted_words(hash_words, work)
    args = {"base_words": (read,) * 3, "litter_words": (read,) * 3, "mask_words": read}
    if prepared:
        bounds = np.repeat(world, 2, axis=0)
        footprint = max(np.linalg.norm(dx[0]), np.linalg.norm(dy[0]))
        mask_needed = 0 < coverage < 1 and recipe[2] > 0 and footprint < 2 * recipe[2]
        base, _, base_fields = prepared_sources(
            bounds, footprint, recipe, enabled=coverage < 1, mask_needed=mask_needed
        )
        top, _, top_fields = prepared_sources(
            bounds, footprint, litter, enabled=coverage > 0
        )
        args = {
            "base_words": tuple(counted_words(s, work) for s in base),
            "litter_words": tuple(counted_words(s, work) for s in top),
            "mask_words": counted_words(base[0] if mask_needed else hash_words, work),
        }
        work["singleton_plane_bytes_constructed"] += sum(
            f.retained_bytes for f in base_fields + top_fields
        )
    return compose(
        world,
        dx,
        dy,
        mean(palette, rows, which),
        recipe,
        float(np.clip(palette[offset + 4], 0, 1)),
        litter,
        difference,
        coverage,
        **args,
    )[0]


def rock(capture, world, dx, dy, *, prepared, work):
    palette, rows = capture.palette, int(capture.palette_words[0])
    row = min(int(capture.palette_words[1]), rows)
    at = 4 * (rows + 2) + rows + 1 + 8 * row
    _, amplitude, coarse, fine = palette[at : at + 4]
    if coarse <= 0 or fine <= 0:
        return np.r_[mean(palette, rows, row), 0, 0]
    fields = []
    for matrix, wavelength in zip(MATRICES, (12 * coarse, coarse, fine), strict=True):
        tx, ty = dx @ matrix.T / wavelength, dy @ matrix.T / wavelength
        if max(float(np.sum(tx * tx)), float(np.sum(ty * ty))) >= 0.64:
            fields.append(np.zeros((1, 4)))
            work["rock_octaves_skipped_before_noise"] += 1
            continue
        query = analytic
        work["active_rock_octaves"] += 1
        if prepared:
            normalized = world @ (matrix / wavelength).T
            lattice = prepare(np.repeat(normalized, 2, axis=0))
            query = partial(evaluate, lattice)
            work["singleton_lattice_bytes_constructed"] += lattice.retained_bytes
            work["prepared_words_queried"] += 8
        else:
            work["analytic_hash_words_queried"] += 8
        fields.append(raw_noise(world, matrix, wavelength, noise_query=query))
    detail = rock_response(
        fields,
        dx,
        dy,
        coarse_m=coarse,
        fine_m=fine,
        height_m=amplitude,
        contrast=float(np.clip(palette[at + 4], 0, 1)),
    )
    return wearing(mean(palette, rows, row), detail)[0]


def evaluate_capture(capture, *, prepared=False, prune_support=False):
    """Pointwise quality proof, not a field-residency planner or prepared query cost.

    Singleton preparation is deliberately inside this finite lookup control.
    Its bytes are neither a world working-set estimate nor an amortized hit.
    """
    results = []
    work = {
        "active_rock_octaves": 0,
        "rock_octaves_skipped_before_noise": 0,
        "singleton_lattice_bytes_constructed": 0,
        "singleton_plane_bytes_constructed": 0,
        "substrate_calls": 0,
        "rock_calls": 0,
        "first_substrate_calls": 0,
        "other_substrate_calls": 0,
        "analytic_hash_words_queried": 0,
        "prepared_words_queried": 0,
    }
    rows = int(capture.palette_words[0])
    rock_row = min(int(capture.palette_words[1]), rows)
    for point in capture.points:
        world, dx, dy = point[4:7][None], point[8:11][None], point[12:15][None]
        first_rock, second_rock, class_weight = point[28:31]
        which, runner = map(int, point[52:54])
        other_used = point[32] < float(np.float32(1e30)) and class_weight < 1
        first_used = not other_used or class_weight > 0
        needs_rock = (
            (first_used and first_rock > 0) or (other_used and second_rock > 0)
            if prune_support
            else max(first_rock, second_rock) > 0
        )
        rock_surface = np.r_[mean(capture.palette, rows, rock_row), 0, 0]
        if needs_rock:
            rock_surface = rock(capture, world, dx, dy, prepared=prepared, work=work)
            work["rock_calls"] += 1
        composed = rock_surface.copy()
        if first_rock < 1 and (first_used or not prune_support):
            base = substrate(
                capture,
                which,
                world[:, [0, 2]],
                dx[:, [0, 2]],
                dy[:, [0, 2]],
                prepared=prepared,
                work=work,
            )
            composed = base * (1 - first_rock) + rock_surface * first_rock
            work["substrate_calls"] += 1
            work["first_substrate_calls"] += 1
        if other_used:
            other = rock_surface.copy()
            if second_rock < 1:
                base = substrate(
                    capture,
                    runner,
                    world[:, [0, 2]],
                    dx[:, [0, 2]],
                    dy[:, [0, 2]],
                    prepared=prepared,
                    work=work,
                )
                other = base * (1 - second_rock) + rock_surface * second_rock
                work["substrate_calls"] += 1
                work["other_substrate_calls"] += 1
            composed = other * (1 - class_weight) + composed * class_weight
        composed[:3] = np.clip(composed[:3], 0, 1)
        composed[3] = np.clip(composed[3], 0.04, 1)
        results.append(composed)
    return np.asarray(results), work

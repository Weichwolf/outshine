"""CPU input generation and numerical references for native ground materials."""

from pathlib import Path

import numpy as np

from ground_detail import grains, noise2
from ground_gradients import grain_gradient, noise_gradient

ROOT = Path(__file__).resolve().parents[3]


DTYPE = np.dtype([("point_dx", "<f4", (4,)), ("dy", "<f4", (2,)),
                  ("which", "<i4"), ("mode", "<u4")])


def make_probes(palette):
    rows = int(palette[:1].view("<u4")[0])
    surfaces = 4 * (rows + 2) + rows + 1
    rng = np.random.default_rng(196)
    controls = []
    for row in range(rows + 2):
        scale = float(palette[surfaces + 8 * min(row, rows - 1) + 2]) or 1.0
        for width in (0., .0001, .05, .25, .8, 1., 1.5, 2., 8.):
            for skew in (False, True):
                p = np.zeros(8, dtype=DTYPE)
                p["point_dx"][:, :2] = rng.uniform(-4, 4, (8, 2)) * scale
                p["point_dx"][:, 2:] = [width * scale, width * scale * .25 if skew else 0.]
                p["dy"] = [-width * scale * .125 if skew else 0., width * scale * .5 if skew else width * scale]
                p["which"] = row if row < rows else -1 if row == rows else rows + 17
                controls.append(p)
    controls = np.concatenate(controls)
    surfaces_probes = controls.copy()
    surfaces_probes["mode"] = 1
    probes = [controls, surfaces_probes]
    panels = []
    for row in (0, 1, 3):
        scale = float(palette[surfaces + 8 * row + 2])
        for width in (.002, .8, 2.):
            p = np.zeros(128 * 96, dtype=DTYPE)
            x, y = np.meshgrid(np.linspace(-3, 3, 128), np.linspace(-2.25, 2.25, 96))
            p["point_dx"][:, :2] = np.stack([x.ravel(), y.ravel()], axis=-1) * scale
            p["point_dx"][:, 2] = width * scale
            p["dy"][:, 1] = width * scale
            p["which"], p["mode"] = row, 1
            start = sum(len(part) for part in probes)
            panels.append((row, width, start))
            probes.append(p)
    return np.concatenate(probes), len(controls), panels


def box_reference(at, dx, dy, scale):
    width = (np.abs(dx) + np.abs(dy)) / scale[:, None]
    centre = at / scale[:, None]
    width = np.where(width <= .01, 0., width)
    total = np.zeros(len(at))
    # Independent midpoint quadrature, not the production primitive/corner weights.
    active = width.max(axis=-1) < 2
    for y in range(64):
        for x in range(64):
            shift = (np.array([x, y]) + .5) / 64 - .5
            total[active] += noise2(centre[active] + width[active] * shift)
    fade = np.clip(width.max(axis=-1) - 1, 0, 1)
    return np.where(active, (total / 4096 - .5) * (1 - fade * fade * (3 - 2 * fade)), 0.)


def detail(at, dx, dy, recipe, contrast):
    grain, height, coarse, fine = map(float, recipe)
    footprint = np.maximum(np.linalg.norm(dx, axis=-1), np.linalg.norm(dy, axis=-1))
    result = np.zeros((len(at), 4))
    result[:, 0] = 1
    if coarse <= 0 or fine <= 0:
        return result
    def filtered(scale):
        if scale <= 0:
            return np.zeros(len(at)), np.zeros_like(at)
        t = np.clip((footprint / scale - .25) / .55, 0, 1)
        visibility = 1 - t * t * (3 - 2 * t)
        n, gradient = noise_gradient(np.column_stack([at / scale, np.zeros(len(at))]))
        return (n - .5) * visibility, gradient[:, :2] * (visibility / scale)[:, None]
    c, _ = filtered(coarse)
    f, gradient = filtered(fine)
    tint = np.ones(len(at))
    if grain > 0 and height > 0:
        parameters = dict(grainSizeM=grain, heightAmplitudeM=height)
        _, grain_derivative = grain_gradient(at, parameters)
        t = np.clip((footprint / grain - .25) / .55, 0, 1)
        gradient = height * gradient + grain_derivative * (1 - t * t * (3 - 2 * t))[:, None]
        for i, size in enumerate(footprint):
            tint[i] = grains(at[i:i + 1], size, parameters)[0][0]
    else:
        gradient *= height
    result[:, 0] = (1 + contrast * (.7 * c + .3 * f)) * tint
    result[:, 1] = .06 * c + .04 * f
    result[:, 2:] = np.column_stack([np.sum(gradient * dx, axis=-1), np.sum(gradient * dy, axis=-1)])
    result[footprint >= max(coarse, fine, grain) * .8] = [1, 0, 0, 0]
    return result


def validate(palette, probes, native, legacy, neutral, count, field_reference=box_reference,
             reference_description="64x64 independent midpoint integration"):
    rows = int(palette[:1].view("<u4")[0])
    layers = int(palette[3:4].view("<u4")[0])
    surfaces = 4 * (rows + 2) + rows + 1
    p = probes[:count]
    # Unmapped substrate uses its native row, while wearing remains the explicit fallback.
    unmapped = rows - 1
    indices = np.where((p["which"] >= 0) & (p["which"] < rows), p["which"], unmapped)
    scales = palette[surfaces + 8 * indices + 2].astype(float)
    scales[scales <= 0] = 1
    field = field_reference(p["point_dx"][:, :2].astype(float), p["point_dx"][:, 2:].astype(float),
                            p["dy"].astype(float), scales)
    field_error = float(np.max(np.abs(native[:count, 0] - field)))
    assert field_error < .00015, field_error
    full_error, neutral_error, old_error = 0., 0., 0.
    per_channel = np.zeros(6)
    for row in range(rows + 2):
        selected = np.flatnonzero(p["which"] == (row if row < rows else -1 if row == rows else rows + 17))
        q = p[selected]
        at, dx, dy = q["point_dx"][:, :2].astype(float), q["point_dx"][:, 2:].astype(float), q["dy"].astype(float)
        substrate = row if row < rows else unmapped
        recipe = palette[surfaces + 8 * substrate:surfaces + 8 * substrate + 4]
        contrast = palette[surfaces + 8 * substrate + 4]
        mean = palette[4 + 4 * min(row, rows):8 + 4 * min(row, rows)].astype(float)
        coverage = palette[surfaces + 8 * substrate + 7] if row < rows else 0.
        delta = palette[layers + 8 * min(row, rows):layers + 8 * min(row, rows) + 4].astype(float)
        weight = coverage + 2 * min(coverage, 1 - coverage) * field[selected]
        first = mean - coverage * delta
        expected_neutral = first + weight[:, None] * delta
        actual = neutral[count + selected]
        neutral_error = max(neutral_error, float(np.max(np.abs(actual[:, :4] - expected_neutral))),
                            float(np.max(np.abs(actual[:, 4:]))))
        base = detail(at, dx, dy, recipe, contrast)
        litter_recipe = palette[layers + 8 * min(row, rows) + 4:layers + 8 * min(row, rows) + 8]
        litter = detail(at, dx, dy, litter_recipe, contrast) if coverage > 0 else base
        def wearing(optics, micro):
            return np.column_stack([optics[:3] * micro[:, :1], optics[3] + micro[:, 1], micro[:, 2:]])
        expected = wearing(first, base) * (1 - weight[:, None]) + wearing(first + delta, litter) * weight[:, None]
        residual = np.abs(native[count + selected, :6] - expected)
        per_channel = np.maximum(per_channel, residual.max(axis=0))
        full_error = max(full_error, float(residual.max()))
        old_error = max(old_error, float(np.max(np.abs(legacy[count + selected, :6] - expected))))
        if coverage == 0:
            np.testing.assert_array_equal(native[count + selected], legacy[count + selected])
    assert neutral_error < .0001, neutral_error
    assert full_error < .0002, full_error
    assert old_error > .01, "old constant composition must fail the same reference"
    return dict(control_points=count, field_max_abs_error=field_error,
                neutral_composition_max_abs_error=neutral_error, full_surface_max_abs_error=full_error,
                full_surface_channel_max_abs_error=per_channel.tolist(),
                legacy_max_abs_error_against_same_reference=old_error,
                reference=reference_description + "; CPU analytic micro-gradients",
                scope="Shader component parameters only; no Place, lighting, frame-time or A18-Pro proof")

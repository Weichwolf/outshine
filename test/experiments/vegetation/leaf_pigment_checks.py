"""Independent gamut constraints and spatial field invariants."""

import colour
import numpy as np
from leaf_pigment_fields import patch_field
from leaf_pigment_optics import gamut_map, rgb_weights


def pigment_checks():
    rng = np.random.default_rng(93)
    rgb = rng.uniform(-0.2, 1.2, (1000, 2, 3))
    y = rgb @ np.array([0.2126, 0.7152, 0.0722])
    valid = (y.min(axis=1) >= 0) & (y.sum(axis=1) < 1)
    rgb, y = rgb[valid][:80], y[valid][:80]
    mapped, factors = gamut_map(rgb)
    assert (
        mapped.min() >= 0
        and np.max(mapped[:, :3].max(axis=1) + mapped[:, 3:].max(axis=1)) <= 1 + 1e-12
    )
    assert np.allclose(
        mapped.reshape(-1, 2, 3) @ [0.2126, 0.7152, 0.0722], y, atol=1e-14, rtol=0
    )
    for original, neutral, factor in zip(rgb, y, factors):
        low, high = 0.0, 1.0
        for _ in range(45):
            middle = (low + high) / 2
            value = neutral[:, None] + middle * (original - neutral[:, None])
            if value.min() >= 0 and value.max(axis=1).sum() <= 1:
                low = middle
            else:
                high = middle
        assert abs(low - factor) <= 1e-12
    assert np.allclose(rgb_weights().sum(axis=0), 1, atol=1e-14, rtol=0)
    shape = colour.SpectralShape(400, 780, 1)
    cmfs = colour.MSDS_CMFS["CIE 1931 2 Degree Standard Observer"].copy().align(shape)
    illuminant = colour.SDS_ILLUMINANTS["D65"].copy().align(shape)
    matrix = colour.RGB_COLOURSPACES["sRGB"].matrix_XYZ_to_RGB
    converted = []
    spectrum = np.linspace(0.1, 0.8, 381)
    for values in (spectrum, np.ones(381)):
        sd = colour.SpectralDistribution(dict(zip(range(400, 781), values)))
        xyz = colour.sd_to_XYZ(sd, cmfs, illuminant, method="Integration")
        converted.append(matrix @ xyz)
    assert np.allclose(
        spectrum @ rgb_weights(), converted[0] / converted[1], atol=1e-13, rtol=0
    )
    p = rng.uniform(-0.1, 0.1, (2000, 2))
    field = patch_field(p, 7)
    assert np.array_equal(
        field, np.concatenate([patch_field(p[:537], 7), patch_field(p[537:], 7)])
    )
    assert np.array_equal(field[::-1], patch_field(p[::-1], 7))
    assert not np.array_equal(field, patch_field(p, 8)) and np.max(abs(field)) <= 1
    return {
        "gamut_bisections": len(rgb),
        "preserved_luminance": True,
        "bounded_rgb_mixture": True,
        "white_normalization": True,
        "independent_colour_integration": True,
        "spatial_batch_order_and_seed": True,
    }

"""Independent geometry, integral and advection controls for the layer comparison."""

from dataclasses import replace

import numpy as np

from cloud_field import Layer, altitude, density, segments, transmittance, wind


def controls():
    layer = Layer()
    origin = np.array([0.0, 1772.0, 0.0])
    vertical = np.array([0.0, 1.0, 0.0])
    np.testing.assert_allclose(segments(origin, vertical, layer),
                               [[800.0, 1400.0], [0.0, 0.0]], atol=1e-8)
    assert np.sum(np.diff(segments(origin, -vertical, layer), axis=-1)) == 0.0
    above = np.array([0.0, 4000.0, 0.0])
    np.testing.assert_allclose(segments(above, -vertical, layer)[0], [828.0, 1428.0], atol=1e-8)
    inside = np.array([0.0, 2800.0, 0.0])
    np.testing.assert_allclose(segments(inside, vertical, layer)[0], [0.0, 372.0], atol=1e-8)
    tangent = np.array([1.0, 0.0, 0.0])
    tangent_span = np.diff(segments(origin, tangent, layer), axis=-1).sum()
    exact_tangent = (np.sqrt((layer.radius_m + 3172.0)**2 - (layer.radius_m + 1772.0)**2)
                     - np.sqrt((layer.radius_m + 2572.0)**2 - (layer.radius_m + 1772.0)**2))
    assert abs(tangent_span - exact_tangent) < 1e-7
    points = np.random.default_rng(193).uniform([-15000.0, 2650.0, -15000.0],
                                               [15000.0, 3000.0, 15000.0], (256, 3))
    for bearing, expected in ((0.0, [0.0, 0.0, 8.0]), (90.0, [-8.0, 0.0, 0.0]),
                              (180.0, [0.0, 0.0, -8.0]), (270.0, [8.0, 0.0, 0.0])):
        np.testing.assert_allclose(wind(replace(layer, wind_from_deg=bearing)), expected, atol=1e-12)
    shifted = points + wind(layer) * 120.0
    old_height = (altitude(points, layer.radius_m) - layer.base_m) / layer.thickness_m
    new_height = (altitude(shifted, layer.radius_m) - layer.base_m) / layer.thickness_m
    expected = density(points, layer) * new_height * (1.0 - new_height) / (old_height * (1.0 - old_height))
    np.testing.assert_allclose(density(shifted, layer, 120.0), expected, rtol=0.0, atol=1e-14)
    # Advection is horizontal in the local frame; spherical height remains physical.
    flat = replace(layer, radius_m=1e14)
    np.testing.assert_allclose(density(points + wind(flat) * 120.0, flat, 120.0),
                               density(points, flat), rtol=0.0, atol=1e-10)
    np.testing.assert_allclose(altitude(np.array([[0.0, 0.0, 0.0], [0.0, 1772.0, 0.0]]),
                                      layer.radius_m), [0.0, 1772.0], atol=1e-10)
    clear = replace(layer, cover=0.0)
    assert transmittance(origin, vertical, clear, 8) == 1.0
    print('PASS: shell geometry, horizon, inside/above, clear, wind-from and advection controls')


if __name__ == '__main__':
    controls()

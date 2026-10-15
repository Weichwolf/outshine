"""Independent numerical RTE and segment composition for the vertical closed form."""

import unittest

import numpy as np
from air_vertical_reference import VerticalAir


def medium(haze=1.0):
    return VerticalAir(
        np.array(
            [
                6360,
                6460,
                8,
                1.2,
                0.005802,
                0.013558,
                0.0331,
                0.03996 * haze,
                0.000650,
                0.001881,
                0.000085,
                0.0444 * haze,
                25,
                15,
                0.8,
                0,
                0.3,
                0.3,
                0.3,
                0,
            ],
            dtype=np.float64,
        )
    )


def numerical_nodes(air, height, distance, count=64):
    ozone_edges = air.values[12] + air.values[13] * np.array([-1, 0, 1]) - height
    edges = np.unique(np.clip(np.r_[0, ozone_edges, distance], 0, distance))
    nodes, weights = np.polynomial.legendre.leggauss(count)
    spans = np.diff(edges)
    points = edges[:-1, None] + spans[:, None] * (nodes + 1) / 2
    return points.ravel(), (spans[:, None] * weights / 2).ravel()


def numerical_density(air, height):
    p = air.values
    rayleigh = np.exp(-height / p[2])[:, None] * p[4:7]
    mie = np.exp(-height / p[3])
    ozone = np.clip(1 - abs(height - p[12]) / p[13], 0, 1)
    return rayleigh, mie, rayleigh + mie[:, None] * p[11] + ozone[:, None] * p[8:11]


def numerical_depth(air, height, distance):
    points, weights = numerical_nodes(air, height, distance)
    return np.sum(numerical_density(air, height + points)[2] * weights[:, None], axis=0)


class VerticalReference(unittest.TestCase):
    def test_columns_match_independent_piecewise_quadrature(self):
        air = medium(4)
        for height, distance in ((0.005, 99), (1, 0.001), (10, 60), (25, 74)):
            np.testing.assert_allclose(
                air.optical_depth(height, distance),
                numerical_depth(air, height, distance),
                rtol=2e-12,
                atol=2e-14,
            )

    def test_full_single_scattering_matches_independently_integrated_rte(self):
        air = medium(1)
        height, distance = 0.005, 95.0
        positions, weights = numerical_nodes(air, height, distance)
        scattering_r, mie, _ = numerical_density(air, height + positions)
        g = air.values[14]
        phase_m = (
            3 / (8 * np.pi) * (1 - g * g) / (2 + g * g) * 2 / (1 + g * g - 2 * g) ** 1.5
        )
        source = scattering_r * 3 / (8 * np.pi) + mie[:, None] * air.values[7] * phase_m
        for index, along in enumerate(positions):
            source[index] *= np.exp(
                -numerical_depth(air, height, along)
                - numerical_depth(
                    air, height + along, air.top_height_km - height - along
                )
            )
        numerical = np.sum(source * weights[:, None], axis=0)
        np.testing.assert_allclose(
            air.transfer(height, distance)[0], numerical, rtol=2e-12
        )

    def test_partial_segments_compose_with_their_own_local_sun_column(self):
        for haze in (0, 0.25, 1, 4):
            air = medium(haze)
            first_l, first_t = air.transfer(0.005, 1.1)
            last_l, last_t = air.transfer(1.105, 25)
            whole_l, whole_t = air.transfer(0.005, 26.1)
            np.testing.assert_allclose(first_l + first_t * last_l, whole_l, rtol=2e-14)
            np.testing.assert_allclose(first_t * last_t, whole_t, rtol=2e-14)

    def test_zero_distance_and_vacuum_are_identities(self):
        for air in (medium(), medium(0)):
            light, through = air.transfer(1, 0)
            np.testing.assert_array_equal(light, np.zeros(3))
            np.testing.assert_array_equal(through, np.ones(3))
        empty = medium(0)
        empty.values[4:12] = 0
        light, through = empty.transfer(1, 90)
        np.testing.assert_array_equal(light, np.zeros(3))
        np.testing.assert_array_equal(through, np.ones(3))

    def test_sun_and_eye_transmittance_product_is_constant_along_ray(self):
        air = medium(4)
        height = 0.005
        whole = np.exp(-air.optical_depth(height, air.top_height_km - height))
        for distance in (0, 0.1, 1, 5, 25, 40, 99):
            actual = np.exp(-air.optical_depth(height, distance)) * np.exp(
                -air.optical_depth(
                    height + distance, air.top_height_km - height - distance
                )
            )
            np.testing.assert_allclose(actual, whole, rtol=1e-14)


if __name__ == "__main__":
    unittest.main()

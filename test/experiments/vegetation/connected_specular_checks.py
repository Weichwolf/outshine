"""Array roughness GGX consumer compared to the unchanged existing gloss reference."""

import unittest

import numpy as np
from connected_material_specular import response
from foliage_selection import directions
from leaf_gloss import specular_response


class ConnectedSpecularChecks(unittest.TestCase):
    def test_existing_scalar_response_and_two_sided_orientation_are_unchanged(self):
        normals = directions(31)
        for view in directions(7):
            for light in directions(7):
                expected = specular_response(normals, 0.55, view, light)
                np.testing.assert_allclose(
                    response(normals, 0.55, view, light), expected, rtol=0, atol=1e-15
                )

    def test_per_pixel_roughness_matches_individual_existing_responses(self):
        normals = directions(17)
        roughness = np.linspace(0.1, 1, 17)
        view, light = directions(7)[[0, 1]]
        expected = np.array(
            [
                specular_response(n[None], r, view, light)[0]
                for n, r in zip(normals, roughness, strict=True)
            ]
        )
        np.testing.assert_allclose(
            response(normals, roughness, view, light), expected, rtol=0, atol=1e-15
        )

    def test_opposed_light_has_no_half_vector_and_normal_incidence_is_analytic(self):
        normal = np.array(((0.0, 0, 1.0),))
        view = normal[0]
        np.testing.assert_array_equal(response(normal, 0.55, view, -view), 0)
        self.assertAlmostEqual(
            response(normal, 0.55, view, view)[0],
            0.04 / (4 * np.pi * 0.55**4),
            places=14,
        )


if __name__ == "__main__":
    unittest.main()

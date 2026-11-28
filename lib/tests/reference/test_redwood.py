import unittest
from types import SimpleNamespace

import numpy as np
import shapely
from outshine.vegetation.redwood import (
    HEIGHT,
    MAX_RADIUS_MULTIPLIER,
    OLD_GROWTH_REDWOOD,
    RADIUS,
    REDWOOD_FOLIAGE,
    REDWOOD_TAXON,
    redwood_geometry,
    redwood_mask,
    redwood_stem,
)
from outshine.vegetation.species_catalog import load_species_catalog
from outshine.vegetation.understory import sword_fern


class RedwoodTest(unittest.TestCase):
    def test_boles_are_closed_tapered_and_within_the_root_reservation(self):
        for form, height in enumerate(HEIGHT):
            for detailed in (False, True):
                with self.subTest(form=form, detailed=detailed):
                    vertices, faces = redwood_stem(form, detailed)
                    self.assertAlmostEqual(vertices[:, 2].max(), height)
                    foot = vertices[np.isclose(vertices[:, 2], 0), :2]
                    foot_radius = np.linalg.norm(foot, axis=1)
                    self.assertLess(
                        foot_radius.max(), MAX_RADIUS_MULTIPLIER * RADIUS[form]
                    )
                    self.assertGreater(foot_radius.max() / RADIUS[form], 1.35)
                    edges = np.sort(
                        np.concatenate((faces[:, :2], faces[:, 1:], faces[:, (2, 0)])),
                        axis=1,
                    )
                    self.assertTrue(
                        np.all(np.unique(edges, axis=0, return_counts=True)[1] == 2)
                    )
                    crown, _ = redwood_geometry(form, detailed, False)[REDWOOD_FOLIAGE]
                    self.assertGreater(np.asarray(crown)[:, 2].min(), 0.30 * height)

    def test_regional_crowns_share_the_positions_of_retained_sprays(self):
        for form in range(len(HEIGHT)):
            close, _ = redwood_geometry(form, True, False)[REDWOOD_FOLIAGE]
            far, _ = redwood_geometry(form, False, False)[REDWOOD_FOLIAGE]
            close = np.asarray(close).reshape(-1, 10, 8, 3)[:, ::2]
            far = np.asarray(far).reshape(-1, 5, 8, 3)
            # Both central spray vertices are directly above the same branch location.
            np.testing.assert_allclose(close[:, :, 6, :2], far[:, :, 6, :2])

    def test_native_taxon_and_park_observation_are_independent_inputs(self):
        catalog = load_species_catalog()
        self.assertIn(REDWOOD_TAXON, catalog.candidate_ids(("CAL",)))
        source = SimpleNamespace(lon=-124, lat=41, metres=np.array([84000, 111132]))
        atlas = SimpleNamespace(
            observed_cells=lambda latitude, longitude: (
                np.where(longitude > source.lon, 1, 2),
                None,
            ),
            observation_mask=lambda indices, codes: (
                indices == 1 if OLD_GROWTH_REDWOOD in codes else np.zeros_like(indices)
            ),
        )
        xy = shapely.get_coordinates(shapely.points([(-5, 0), (5, 0)]))
        np.testing.assert_array_equal(redwood_mask(atlas, source, xy), [False, True])

    def test_sword_fern_detail_levels_remain_finite_and_bounded(self):
        for form in (0, 1):
            detailed, detailed_faces = sword_fern(form, True)
            coarse, coarse_faces = sword_fern(form, False)
            for vertices in (detailed, coarse):
                self.assertTrue(np.isfinite(vertices).all())
                self.assertLess(np.linalg.norm(vertices[:, :2], axis=1).max(), 1.0)
            self.assertLess(len(coarse_faces), len(detailed_faces) / 3)

"""Flat, reproducible populations and actual woody/foliar demonstration products."""

import json
import unittest

import numpy as np
from biome_models import woody
from render_biome_tiles import HERE, population, reference_population


class BiomeTileChecks(unittest.TestCase):
    def test_grass_density_does_not_move_or_reseed_existing_woody_plants(self):
        recipes = json.loads((HERE / "biome_tiles.json").read_text())
        for name, recipe in recipes.items():
            old = [
                row
                for row in reference_population(name, recipe)
                if not row[0].startswith("grass-")
            ]
            new = [
                row
                for row in population(name, recipe)
                if not row[0].startswith("grass-")
            ]
            self.assertEqual([row[0] for row in old], [row[0] for row in new])
            for a, b in zip(old, new, strict=True):
                for x, y in zip(a[1:], b[1:], strict=True):
                    np.testing.assert_array_equal(x, y)

    def test_four_replayable_flat_populations_fit_their_tiles(self):
        recipes = json.loads((HERE / "biome_tiles.json").read_text())
        self.assertEqual(set(recipes), {"Laubwald", "Mischwald", "Steppe", "Heide"})
        for name, recipe in recipes.items():
            a, b = population(name, recipe), population(name, recipe)
            self.assertEqual([v[0] for v in a], [v[0] for v in b])
            for left, right in zip(a, b, strict=True):
                for x, y in zip(left[1:], right[1:], strict=True):
                    np.testing.assert_array_equal(x, y)
                _, origins, frames, scales = left
                self.assertTrue((abs(origins[:, :2]) <= recipe["size_m"] / 2).all())
                self.assertTrue((origins[:, 2] == 0).all())
                self.assertTrue((scales > 0).all())
                np.testing.assert_allclose(np.linalg.det(frames), 1, atol=1e-14)
            forms = {key: len(p) for key, p, _, _ in a}
            if name == "Mischwald":
                self.assertTrue(all(forms[key] > 0 for key in ("oak", "beech", "pine")))
            if name == "Heide":
                self.assertGreater(forms["heather"], 100)
            self.assertGreater(
                sum(count for key, count in forms.items() if key.startswith("grass")),
                300,
            )

    def test_heather_assembly_contains_physical_wood_leaves_and_bells(self):
        mesh = woody("heather", 718)
        v, f, n = mesh["vertices"], mesh["faces"], mesh["normals"]
        self.assertTrue(np.isfinite(v).all() and np.isfinite(n).all())
        self.assertGreater(v[:, 2].max(), 0.4)
        self.assertEqual(n.shape, (len(f) * 3, 3))
        self.assertEqual(mesh["uv"].shape, (len(f) * 3, 2))
        self.assertTrue({0, 1, 4} <= set(mesh["surface"]))
        face = v[f]
        geometric = np.cross(face[:, 1] - face[:, 0], face[:, 2] - face[:, 0])
        self.assertTrue((np.linalg.norm(geometric, axis=1) > 1e-16).all())
        np.testing.assert_allclose(np.linalg.norm(n, axis=1), 1, atol=2e-14)


if __name__ == "__main__":
    unittest.main()

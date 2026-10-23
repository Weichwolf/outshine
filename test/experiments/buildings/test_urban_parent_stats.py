"""Direct area statistics, hierarchy additivity and light-independent measures."""

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from test_urban_patches import source_document
from urban_parent_stats import UrbanParent, compile_parent
from urban_recipe import HEIGHT_CUMULATIVE, HEIGHTS_M


def example():
    return UrbanParent(
        np.array([[[1.0, 0.6, 0.4]]]),
        np.array([0, 0, 0.5, 1024, 32, 0.75, 0.15]),
        np.array(HEIGHTS_M[1:]),
        np.diff([0, *HEIGHT_CUMULATIVE, 1]),
    )


class UrbanParentTest(unittest.TestCase):
    def test_direct_parent_area_statistics_add_across_child_regions_without_bake(self):
        document = source_document()
        with patch(
            "urban_patch_bake.bake", side_effect=AssertionError("child generation")
        ):
            root, metadata = compile_parent(document, divisions=1)
            children, child_metadata = compile_parent(document, divisions=4)
        np.testing.assert_allclose(
            root.centre_fractions[0, 0],
            children.centre_fractions.mean(axis=(0, 1)),
            atol=1e-12,
        )
        np.testing.assert_allclose(
            root.surface_measures()[0, 0],
            children.surface_measures().mean(axis=(0, 1)),
            atol=1e-12,
        )
        self.assertEqual(metadata["child_cells_generated"], 0)
        self.assertEqual(child_metadata["parent_regions"], 16)
        self.assertEqual(child_metadata["area_intersections"], 48)
        self.assertEqual([row["id"] for row in metadata["source_records"]], [1, 2, 3])

    def test_surface_measures_preserve_six_box_orientations_and_height_prior(self):
        product = example()
        probability = 0.5 * 0.85
        roof = probability * 0.75**2
        wall = probability * 0.75 * 9.96 / 16
        np.testing.assert_allclose(
            product.surface_measures()[0, 0], [wall] * 4 + [roof] * 2
        )
        np.testing.assert_allclose(
            product.height_survival([0, 6, 6.01, 12, 24, 48, 49]),
            [1, 1, 0.38, 0.38, 0.10, 0.02, 0],
            atol=1e-15,
        )

    def test_unknown_area_stays_explicit_instead_of_world_empty(self):
        document = source_document()
        document["layers"]["landuse"]["features"][0]["properties"]["class"] = "unknown"
        product, metadata = compile_parent(document, divisions=2)
        self.assertEqual(product.centre_fractions.sum(), 0)
        self.assertEqual(metadata["status"], "inferred_statistics_not_world_empty")
        self.assertIn("not observed", metadata["height_model"])

    def test_stored_parent_measures_need_neither_source_geos_nor_child_compiler(self):
        code = """
import builtins, sys
import numpy as np
original = builtins.__import__
def checked(name, *args, **kwargs):
    if name.split('.')[0] in {'shapely', 'coarse_city_domains', 'urban_patch_bake'}:
        raise AssertionError(name)
    return original(name, *args, **kwargs)
builtins.__import__ = checked
from urban_parent_stats import UrbanParent
with np.load(sys.argv[1], allow_pickle=False) as data:
    p = UrbanParent(**{key: data[key] for key in UrbanParent.__annotations__})
assert p.surface_measures().shape == (1, 1, 6)
"""
        with tempfile.TemporaryDirectory(
            prefix="outshine-urban-parent-test-"
        ) as folder:
            path = Path(folder) / "parent.npz"
            np.savez_compressed(path, **example().arrays())
            subprocess.run(
                [sys.executable, "-B", "-W", "error", "-c", code, str(path)], check=True
            )


if __name__ == "__main__":
    unittest.main()

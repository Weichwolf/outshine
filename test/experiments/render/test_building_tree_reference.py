"""A labelled analytic source reference keeps every source and valid materials."""

import unittest
from pathlib import Path

import numpy as np
from building_mass_materials import policy
from building_material_reference import face_materials
from building_source_tree import source_bounds
from building_tree_reference import reference
from parent_surface_inputs import open_parent


class BuildingTreeReference(unittest.TestCase):
    def test_source_model_keeps_all_sources_without_native_field_guesses(self):
        path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view/original-inputs.tar.gz"
        )
        with open_parent(path) as parent:
            _, _, frame = source_bounds(parent.plans, {44})
        source, receipt = reference(path, 44, frame)
        np.testing.assert_array_equal(
            np.unique(source["binding"][:, 1]), frame["selected_sources"]
        )
        triangle = source["positions"].astype(float)[source["indices"]]
        area = np.cross(
            triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
        )
        self.assertTrue(np.any(area != 0, axis=1).all())
        self.assertTrue(np.isfinite(source["positions"]).all())
        fields, needs = face_materials(source, policy())
        self.assertFalse(needs)
        self.assertTrue(fields["supported"].all())
        self.assertIn("no native postweld", receipt["kind"])


if __name__ == "__main__":
    unittest.main()

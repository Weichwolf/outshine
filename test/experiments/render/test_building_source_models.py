"""Keep reference subsets explicit and material controls on unchanged native geometry."""

import unittest
from contextlib import contextmanager
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from building_tree_reference import reference
from compare_building_source_models import joined_sources, material_control
from compare_directed_groups import retained_subset


def native_fixture():
    binding = np.zeros((3, 6), np.int32)
    binding[:, 1] = [1, 1, 2]
    binding[:, 4] = [0, 1, 0]
    return {
        "positions": np.arange(27).reshape(9, 3),
        "indices": np.arange(9).reshape(3, 3),
        "binding": binding,
    }


class BuildingSourceModels(unittest.TestCase):
    def test_join_retains_whole_native_remainder_and_rejects_overlap_or_omission(self):
        native = native_fixture()
        analytic = retained_subset(native, [1])
        joined = joined_sources(analytic, native, [2])
        for name in native:
            np.testing.assert_array_equal(joined[name], native[name])
        with self.assertRaisesRegex(ValueError, "disjoint"):
            joined_sources(analytic, native, [1, 2])
        with self.assertRaisesRegex(ValueError, "all sources"):
            joined_sources(analytic, native, [])

    def test_material_control_preserves_geometry_and_native_semantic_stream(self):
        native = native_fixture()
        node = {
            "group_sources": np.array([1]),
            "group_offsets": np.array([0, 1]),
            "needs_sources": np.array([2]),
        }
        material = {
            "albedo": np.array([[[0.2, 0.3, 0.4], [0.6, 0.7, 0.8]]]),
            "roughness_weights": np.eye(2)[None],
            "roughness_palette": np.array([0.2, 0.8]),
            "supported": np.ones(1, bool),
        }
        for semantic in (False, True):
            mesh, product, appearance = material_control(
                native, node, material, semantic=semantic
            )
            np.testing.assert_array_equal(mesh["positions"], native["positions"])
            np.testing.assert_array_equal(mesh["indices"], native["indices"])
            np.testing.assert_array_equal(product["needs_sources"], [2])
            if semantic:
                np.testing.assert_array_equal(product["triangle_groups"], [0, 1])
                for role in (0, 1):
                    for normal_role in (0, 1):
                        np.testing.assert_array_equal(
                            appearance["albedo"][role, normal_role],
                            material["albedo"][0, role],
                        )
        native["binding"][1, 4] = 3
        with self.assertRaisesRegex(ValueError, "wall/roof stream"):
            material_control(native, node, material, semantic=True)

    def test_only_explicit_reference_sources_enter_patch_generation(self):
        @contextmanager
        def inputs(path):
            yield SimpleNamespace(
                iter_forms=lambda cells: [(33, [0, 1], [None, {"shapes": [{}]}])]
            )

        frame = {
            "selected_sources": np.array([1]),
            "basis": np.eye(3),
            "anchor": np.zeros(3),
        }
        with (
            patch("parent_surface_inputs.open_parent", inputs),
            patch(
                "source_material_patches.shape_material_patches",
                side_effect=RuntimeError("selected source reached"),
            ) as build,
        ):
            with self.assertRaisesRegex(RuntimeError, "selected source reached"):
                reference(Path("unused"), 33, frame)
            self.assertEqual(build.call_count, 1)
            self.assertEqual(build.call_args.args[2], 1)
            build.reset_mock()
            with self.assertRaisesRegex(
                ValueError, "whole source reference unavailable: 0"
            ):
                reference(
                    Path("unused"), 33, frame | {"selected_sources": np.array([0, 1])}
                )
            build.assert_not_called()


if __name__ == "__main__":
    unittest.main()

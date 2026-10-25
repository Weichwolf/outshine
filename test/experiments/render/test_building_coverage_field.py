"""Area, orientation, source binding and clear-sample work controls."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from build_building_coverage import read_field
from building_coverage_field import (
    area_field,
    build_field,
    sample_field,
    validate_field,
)
from building_mass_inputs import MassSource
from building_mass_mesh import extrude
from building_tree_transfer import transfers


class BuildingCoverageField(unittest.TestCase):
    def test_spatial_hole_is_clear_and_float32_area_is_conserved(self):
        proxy = shapely.box(0, 0, 8, 8)
        source = shapely.difference(proxy, shapely.box(2, 2, 6, 6))
        values, bounds, work = area_field(source, proxy)
        self.assertEqual(values.dtype, np.float32)
        self.assertEqual(float(values.sum()), 48)
        self.assertEqual(work["reconstructed_area_m2"], source.area)
        np.testing.assert_array_equal(values[2:6, 2:6], 0)
        field = {
            "values": np.stack([values] * 3)[None],
            "bounds": np.stack([bounds] * 3)[None],
        }
        points = np.array([[3, 3, 3], [1, 1, 1], [8, 8, 8]])
        for normal in np.eye(3):
            np.testing.assert_array_equal(
                sample_field(points, np.tile(normal, (3, 1)), field, 0), [0, 1, 1]
            )
            np.testing.assert_array_equal(
                sample_field(points, np.tile(-normal, (3, 1)), field, 0), [0, 1, 1]
            )

    def test_clipped_source_overlap_and_fractional_cell_are_paid(self):
        proxy = shapely.box(0, 0, 8, 8)
        source = shapely.box(-2, 0, 0.25, 8)
        values, _, work = area_field(source, proxy)
        np.testing.assert_array_equal(values[:, 0], 0.25)
        np.testing.assert_array_equal(values[:, 1:], 0)
        self.assertEqual(work["source_outside_proxy_m2"], 16)
        self.assertEqual(work["covered_area_m2"], 2)
        self.assertEqual(work["reconstructed_area_m2"], 2)

    def test_source_projections_and_parent_mesh_axes(self):
        sources = [MassSource(1, shapely.box(0, 0, 2, 4), (0, 0), (8, 8))]
        mesh, _ = extrude(shapely.box(0, 0, 8, 8), [0, 8])
        field, _ = build_field(sources, mesh["positions"], mesh["indices"])
        field = {k: v[None] for k, v in field.items()}
        validate_field(field, 1)
        points = np.array([[1, 3, 7], [3, 3, 7], [1, 5, 7]])
        for normal, expected in (
            ([0, 0, 1], [1, 0, 0]),
            ([0, 1, 0], [1, 0, 1]),
            ([1, 0, 0], [1, 1, 0]),
        ):
            np.testing.assert_array_equal(
                sample_field(points, np.tile(normal, (3, 1)), field, 0), expected
            )
        with self.assertRaises(ValueError):
            validate_field(field, 2)

    def test_reload_only_reads_fields_and_binds_original_node(self):
        import hashlib

        field = {
            "values": np.ones((1, 3, 8, 8), np.float32),
            "bounds": np.tile([[0, 0], [1, 1]], (1, 3, 1, 1)),
        }
        with tempfile.TemporaryDirectory(
            prefix="outshine-coverage-field-"
        ) as directory:
            root = Path(directory)
            node = root / "node.npz"
            node.write_bytes(b"opaque original retained")
            digest = np.frombuffer(hashlib.sha256(node.read_bytes()).digest(), np.uint8)
            np.savez(root / "field.npz", **field, source_sha256=digest)
            with patch(
                "build_building_coverage.build_field",
                side_effect=AssertionError("loading must not generate"),
            ):
                loaded, cost = read_field(root / "field.npz", node, 1)
            self.assertEqual(cost, node.stat().st_size)
            np.testing.assert_array_equal(loaded["values"], field["values"])
            node.write_bytes(b"changed source")
            with self.assertRaisesRegex(ValueError, "source node differs"):
                read_field(root / "field.npz", node, 1)

    def test_clear_parent_samples_skip_all_ggx_and_emit_identity_transfer(self):
        candidate = {
            "positions": np.array([[-2, -2, 4], [2, -2, 4], [0, 2, 4.0]]),
            "indices": np.array([[0, 1, 2]]),
        }
        product = candidate | {
            "triangle_groups": np.array([0]),
            "group_coverage": np.ones((1, 3)),
            "needs_sources": np.empty(0, np.uint32),
        }
        appearance = {
            "albedo": np.ones((1, 2, 3)),
            "roughness_weights": np.ones((1, 2, 1)),
            "roughness_palette": np.array([0.7]),
            "supported": np.ones(1, bool),
        }
        field = {
            "values": np.zeros((1, 3, 8, 8), np.float32),
            "bounds": np.tile([[-3, -3], [8, 8]], (1, 3, 1, 1)),
        }
        image, work = transfers(
            candidate,
            {"binding": np.empty((0, 6), int)},
            {},
            product,
            appearance,
            (np.zeros(3), np.eye(3), 1, 1, 1),
            np.array([[0, 0], [1, 1]]),
            {},
            1,
            coverage=field,
        )
        np.testing.assert_array_equal(image["C"], 0)
        np.testing.assert_array_equal(image["T"], 1)
        self.assertEqual(work["ggx_lobe_evaluations"], 0)
        self.assertEqual(work["zero_opacity_samples_skipped_before_shading"], 1)


if __name__ == "__main__":
    unittest.main()

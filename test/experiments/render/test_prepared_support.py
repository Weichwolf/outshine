"""Prepared bounds, outward arithmetic and unavailable-support counterexamples."""

import copy
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path
from types import SimpleNamespace

import numpy as np
from beam_intervals import exact
from prepared_source_support import (
    affine_ranges,
    build,
    load_product,
    store,
    world_corners,
)
from prepared_support_reference import check_vertices
from prepared_support_visibility import select_sources
from region_occlusion import compile_projection, project_ranges
from test_region_occlusion import BOX, walls


def fixture():
    parent = SimpleNamespace(
        parent="parent", archive_sha256="digest", cells=np.ones(4, np.uint8)
    )
    shape = {
        "ring": [[-2, -3], [2, -3], [2, 3], [-2, 3]],
        "holes": [],
        "seat_m": 10.0,
        "foot_m": 0.0,
        "eaves_m": 2.0,
        "rise_m": 1.0,
        "sole_m": 9.0,
    }
    surface = {"shapes": [shape], "sole_m": 9.0, "minimum_height_m": 0.0}
    frame = np.vstack((np.zeros(3), np.eye(3)))
    parent.plans = {
        "records": [{"source_id": [0, 0]} for _ in range(4)],
        "summaries": [
            {"frame": frame.copy(), "shell_error_m": x}
            for x in (0.25, 0.25, None, 0.25)
        ],
        "anchor_ecef": [1, 2, 3],
    }
    invalid = copy.deepcopy(surface)
    invalid["shapes"][0]["ring"] = [[float("nan"), 0]] * 3
    parent.iter_forms = lambda: [(1, np.arange(4), [surface, None, surface, invalid])]
    parent.form_reads, parent.form_bytes = 1, 100
    arrays, work = build(parent)
    return (
        parent,
        {
            "header": {
                "identity": {"parent": "parent", "archive_sha256": "digest"},
                "anchor_ecef": [1, 2, 3],
            },
            "arrays": arrays,
        },
        work,
    )


class PreparedSupportTests(unittest.TestCase):
    def test_form_states_and_anonymous_sources_are_retained(self):
        _, product, work = fixture()
        np.testing.assert_array_equal(product["arrays"]["state"], [1, 0, 2, 3])
        np.testing.assert_array_equal(product["arrays"]["sources"]["ordinal"], range(4))
        self.assertEqual(work["native_mesh_decodes"], 0)
        self.assertEqual(work["emitted_triangles"], 0)
        self.assertEqual(len(work["needs_original"]), 3)
        self.assertIsNone(world_corners(product, 0, lod="fine"))
        for ordinal in (-1, 1, 2, 3, 999):
            self.assertIsNone(world_corners(product, ordinal))

    def test_affine_intervals_enclose_rational_coordinates(self):
        rng = np.random.default_rng(17)
        frame = rng.uniform(-10000, 10000, (4, 3))
        points = rng.uniform(-20, 20, (40, 3))
        low, high = affine_ranges(exact(points), frame)
        for row, point in enumerate(points):
            for axis in range(3):
                value = Fraction(float(frame[0, axis])) + sum(
                    Fraction(float(point[k])) * Fraction(float(frame[k + 1, axis]))
                    for k in range(3)
                )
                self.assertLessEqual(Fraction(float(low[row, axis])), value)
                self.assertGreaterEqual(Fraction(float(high[row, axis])), value)

    def test_given_padding_proves_vertices_and_failure_never_refits_it(self):
        _, product, _ = fixture()
        source = {
            "positions": np.array(
                [[2.0006, 0, 11], [0, 0, 11], [0, 1, 11]], np.float32
            ),
            "indices": np.array([[0, 1, 2]]),
            "binding": np.zeros((1, 2), int),
        }
        self.assertEqual(check_vertices(product, source)["checked_used_vertices"], 3)
        product["arrays"]["padding"][0] = 0.0001
        with self.assertRaisesRegex(ValueError, "misses original"):
            check_vertices(product, source)
        self.assertEqual(product["arrays"]["padding"][0], 0.0001)

    def test_prefix_roundtrip_and_identity_check_before_payload(self):
        parent, product, _ = fixture()
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "prefix"
            store(product["arrays"], parent, path)
            loaded = load_product(path, product["header"]["identity"])
            for k, value in product["arrays"].items():
                np.testing.assert_array_equal(loaded["arrays"][k], value)
            (path / "support.npz").unlink()
            with self.assertRaisesRegex(ValueError, "identity"):
                load_product(path, {})

    def test_available_enclosure_culls_and_fine_unknown_keep_originals(self):
        _, product, _ = fixture()
        projection = compile_projection(
            walls(), BOX, certified_opaque=True, tight_hulls=True, plane=7.5
        )
        projection.update(
            support_identity=product["header"]["identity"], anchor_ecef=[1, 2, 3]
        )
        hidden, available, _ = select_sources(product, [0, 1, 2, 3, 999], projection)
        np.testing.assert_array_equal(hidden, [True, False, False, False, False])
        np.testing.assert_array_equal(available, [True, False, False, False, False])
        self.assertFalse(select_sources(product, [0], projection, lod="fine")[0].any())
        projection["anchor_ecef"] = [0, 0, 0]
        with self.assertRaisesRegex(ValueError, "anchors"):
            select_sources(product, [0], projection)

    def test_interval_projection_covers_all_uncertain_endpoints(self):
        low = np.array([[1.1, -2.1, 10]])
        high = low + [0.2, 0.3, 0.1]
        eyes = np.array([[0.5, -0.3, 0.1]])
        result = project_ranges((low, high), eyes, 5.0)
        for mask in np.ndindex(2, 2, 2):
            point = np.where(mask, high[0], low[0])
            value = eyes[0, :2] + (5 - eyes[0, 2]) / (point[2] - eyes[0, 2]) * (
                point[:2] - eyes[0, :2]
            )
            self.assertTrue(np.all(result[0][0, 0] <= value))
            self.assertTrue(np.all(result[1][0, 0] >= value))


if __name__ == "__main__":
    unittest.main()

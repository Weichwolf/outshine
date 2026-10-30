"""Physical endpads connect whole sources without inventing source topology."""

import copy
import io
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from elevated_rail_hardware import chains, conflict
from elevated_rail_hardware_verify import (
    arrays,
    float32_instances,
    instances,
    profile,
    source_axis,
)
from elevated_rail_joints import apply
from outshine_ref.contracts import Mesh
from outshine_ref.transport.rail_hardware import RailRecipe, build_rail_hardware
from transport_surface_colour import material_parameters


def source(key, points, nodes):
    return {
        "source_id": key,
        "xy": np.array(points, float),
        "original_mvt_xy": nodes,
        "width_m": 3.8,
    }


class HardwareArchiveChecks(unittest.TestCase):
    def test_small_member_cannot_claim_a_large_allocation_before_payload_admission(
        self,
    ):
        header = io.BytesIO()
        np.lib.format.write_array_header_1_0(
            header, {"descr": "<f8", "fortran_order": False, "shape": (1000000000, 3)}
        )
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.rail-header."
        ) as temporary:
            archive = Path(temporary) / "truncated.npz"
            with zipfile.ZipFile(archive, "w") as saved:
                saved.writestr("rail_vertices.npy", header.getvalue())
            with (
                patch(
                    "elevated_rail_hardware_verify.np.load",
                    side_effect=AssertionError("array decoder reached"),
                ),
                self.assertRaisesRegex(ValueError, "payload length"),
            ):
                arrays(archive, ("rail_vertices",))


class RaisedHardwareChecks(unittest.TestCase):
    def setUp(self):
        self.rows = [
            source("first", [[0, 0], [5, 0], [10, 0]], [[0, 0], [5, 0], [10, 0]]),
            source("second", [[10, 0], [15, 1], [20, 2]], [[10, 0], [15, 1], [20, 2]]),
        ]

    def test_constructed_joint_keeps_every_whole_source_segment_and_originals(self):
        apply(self.rows)
        before = copy.deepcopy(self.rows)
        products, omitted = chains(self.rows)
        self.assertEqual(omitted, [])
        self.assertEqual(len(products), 1)
        np.testing.assert_array_equal(
            products[0]["xy"], [[0, 0], [5, 0], [10, 0], [15, 1], [20, 2]]
        )
        self.assertEqual(
            products[0]["segment_sources"], ["first", "first", "second", "second"]
        )
        for a, b in zip(self.rows, before, strict=True):
            np.testing.assert_array_equal(a["xy"], b["xy"])
            self.assertEqual(a["original_mvt_xy"], b["original_mvt_xy"])
            self.assertEqual(a["junction_pads_xy_m"], b["junction_pads_xy_m"])

    def test_same_position_without_supplied_endpoint_identity_is_not_a_joint(self):
        self.rows[1]["original_mvt_xy"][0] = [100, 100]
        apply(self.rows)
        products, omitted = chains(self.rows)
        self.assertEqual(len(products), 2)
        self.assertEqual(omitted, [])
        self.assertEqual([len(p["members"]) for p in products], [1, 1])

    def test_missing_constructed_pad_duplicate_source_and_capacity_reject(self):
        with self.assertRaisesRegex(ValueError, "physical joint pad"):
            chains(self.rows)
        for rows in ([self.rows[0], self.rows[0]], self.rows * 5, []):
            with self.assertRaisesRegex(ValueError, "distinct whole"):
                chains(rows)

    def test_reversed_input_keeps_its_direction_mapping_and_all_segments(self):
        self.rows[1]["xy"] = self.rows[1]["xy"][::-1].copy()
        self.rows[1]["original_mvt_xy"] = self.rows[1]["original_mvt_xy"][::-1]
        apply(self.rows)
        products, omitted = chains(self.rows)
        self.assertEqual(omitted, [])
        self.assertEqual(
            products[0]["members"],
            [
                {"source_id": "first", "reversed": False},
                {"source_id": "second", "reversed": True},
            ],
        )
        np.testing.assert_array_equal(products[0]["xy"][-1], [20, 2])

    def test_positive_same_kind_volume_overlap_is_distinct_from_boundary_contact(self):
        previous = [
            [
                shapely.box(0, 0, 1, 1),
                shapely.box(3, 0, 4, 1),
                shapely.box(10, 0, 11, 1),
            ]
        ]
        self.assertTrue(
            conflict([shapely.box(0.5, 0, 2, 1), shapely.box(6, 0, 7, 1)], previous)
        )
        self.assertFalse(
            conflict([shapely.box(1, 0, 2, 1), shapely.box(6, 0, 7, 1)], previous)
        )
        self.assertFalse(
            conflict([shapely.box(10, 0, 11, 1), shapely.box(6, 0, 7, 1)], previous)
        )


class HardwareRepresentationChecks(unittest.TestCase):
    def test_independent_profile_detects_changed_web_even_with_unchanged_head(self):
        axis = np.array([[0, 0], [10, 0], [20, 2]], float)
        product, _ = build_rail_hardware(axis, 168.0)
        self.assertLess(profile(product.rails, axis, 168.0, RailRecipe()), 1e-9)
        vertices = product.rails.vertices.copy()
        vertices[3, 0] += 0.001
        with self.assertRaisesRegex(ValueError, "metric axis/profile"):
            profile(Mesh(vertices, product.rails.triangles), axis, 168.0, RailRecipe())

    def test_independent_instances_reject_phase_or_physical_size_changes(self):
        axis = np.array([[0, 0], [10, 0], [20, 2]], float)
        product, _ = build_rail_hardware(axis, 0)
        instances(product.sleeper_transforms, axis, RailRecipe())
        for column in (0, 3):
            changed = product.sleeper_transforms.copy()
            changed[0, 0, column] += 0.003
            with self.assertRaisesRegex(
                ValueError, "dimensions, orientation or source phase"
            ):
                instances(changed, axis, RailRecipe())

    def test_height_datum_avoids_the_actual_absolute_float32_contact_counterexample(
        self,
    ):
        datum = 168.85902944058444
        transforms = np.zeros((1, 3, 4))
        transforms[0, :, :3] = np.diag([2.6, 0.24, 0.18])
        transforms[0, 2, 3] = datum
        wrong = float32_instances(transforms)
        old_rail_foot = np.float32(datum + 0.18)
        self.assertTrue(np.all(wrong[:, 4:, 2] != old_rail_foot))
        transforms[0, 2, 3] = 0
        local = float32_instances(transforms)
        np.testing.assert_array_equal(
            local[:, 4:, 2], np.full((1, 4), np.float32(0.18))
        )
        world = np.add(local, np.array([0, 0, datum], "f4"), dtype="f4")
        rail_foot = np.float32(np.float32(0.18) + np.float32(datum))
        np.testing.assert_array_equal(world[:, 4:, 2], np.full((1, 4), rail_foot))

    def test_reader_does_not_turn_same_position_into_a_supplied_join(self):
        rows = [
            source("a", [[0, 0], [10, 0]], [[0, 0], [10, 0]]),
            source("b", [[10, 0], [20, 1]], [[10, 0], [20, 1]]),
        ]
        for row in rows:
            row["original_properties"] = {}
        record = {
            "members": [
                {"source_id": "a", "reversed": False},
                {"source_id": "b", "reversed": False},
            ],
            "axis_xy_m": [[0, 0], [10, 0], [20, 1]],
            "segment_sources": ["a", "b"],
        }
        originals = {r["source_id"]: r for r in rows}
        source_axis(record, originals, {frozenset(originals)})
        rows[1]["original_mvt_xy"][0] = [100, 100]
        with self.assertRaisesRegex(ValueError, "joint identity"):
            source_axis(record, originals, {frozenset(originals)})

    def test_pbr_defaults_stay_unchanged_and_bad_material_records_reject(self):
        self.assertEqual(material_parameters(1), [{"metallic": 0.0, "roughness": 0.86}])
        self.assertEqual(
            material_parameters(1, [{"metallic": 1, "roughness": 0.35}]),
            [{"metallic": 1, "roughness": 0.35}],
        )
        for value in (
            [],
            [{}],
            [{"metallic": True, "roughness": 0.3}],
            [{"metallic": 1, "roughness": float("nan")}],
            [{"metallic": 2, "roughness": 0.3}],
        ):
            with self.assertRaises(ValueError):
                material_parameters(1, value)


if __name__ == "__main__":
    unittest.main()

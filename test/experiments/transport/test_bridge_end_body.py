"""Actual immutable decks, exact whole-cap bounds and height-aware space probes."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from bound_bridge_end_surface import build_end_surface
from bridge_end_body import BodyRecipe, build_end_body
from bridge_end_body import build_body as generate_body
from bridge_end_body_checks import qualify_body
from bridge_end_surface_inputs import actual_end_inputs
from outshine_ref.contracts import HeightInterval, Mesh


def sloping_deck():
    return Mesh(
        np.array([[0.0, 0.0, 2.0], [4.0, 0.0, 4.0], [4.0, 2.0, 4.0], [0.0, 2.0, 2.0]]),
        np.array([[0, 1, 2], [0, 2, 3]], dtype=np.uint32),
    )


def build_body(surface, owner, **options):
    return generate_body(surface, owner, source_id="synthetic-source", **options)


def conflict(product, lower, upper):
    return shapely.union_all(
        [
            r.conflict_geometry(HeightInterval(lower, upper))
            for r in product["occupancies"]
        ]
    )


class BodyControls(unittest.TestCase):
    def test_whole_deck_has_only_external_walls_and_analytic_volume(self):
        product, report = build_body(sloping_deck(), "synthetic")
        checked = qualify_body(product, report)
        self.assertEqual(checked["closed_volume_m3"], 2.0)
        self.assertEqual(checked["boundary_walls"], 4)
        self.assertEqual(checked["faces"], 12)
        self.assertEqual(checked["occupied_volumes"], 1)
        self.assertEqual(checked["maximum_exact_cap_padding_m"], 0.0)

    def test_height_window_does_not_reserve_the_whole_projected_footprint(self):
        for method in ("whole", "faces"):
            product, report = build_body(sloping_deck(), "synthetic", method=method)
            qualify_body(product, report)
            self.assertAlmostEqual(conflict(product, 1.8, 1.9).area, 0.6, places=12)
            self.assertTrue(conflict(product, 1.0, 1.5).is_empty)
            self.assertTrue(conflict(product, 1.5, 1.75).is_empty)
            self.assertTrue(conflict(product, 4.0, 4.5).is_empty)
            self.assertEqual(conflict(product, 1.0, 5.0).area, 8.0)

    def test_headroom_is_explicit_and_preserves_physical_body(self):
        base, _ = build_body(sloping_deck(), "synthetic")
        used, report = build_body(
            sloping_deck(), "synthetic", recipe=BodyRecipe(headroom_m=2.5)
        )
        self.assertEqual(
            base["body"].vertices.tobytes(), used["body"].vertices.tobytes()
        )
        self.assertTrue(conflict(base, 4.5, 5.0).is_empty)
        self.assertGreater(conflict(used, 4.5, 5.0).area, 0.0)
        self.assertEqual(qualify_body(used, report)["maximum_exact_cap_padding_m"], 0.0)

    def test_non_affine_surface_is_never_flattened_into_one_occupied_plane(self):
        source = sloping_deck()
        vertices = source.vertices.copy()
        vertices[2, 2] += 0.01
        source = Mesh(vertices, source.triangles)
        before = source.vertices.tobytes()
        with self.assertRaisesRegex(ValueError, "actual affine cap"):
            build_body(source, "synthetic", method="whole")
        product, report = build_body(source, "synthetic", method="faces")
        self.assertEqual(
            before, product["body"].vertices[: len(source.vertices)].tobytes()
        )
        self.assertEqual(
            qualify_body(product, report)["covered_whole_top_bottom_cap_pairs"], 2
        )

    def test_input_and_complete_face_budgets_end_before_body_allocation(self):
        for recipe in (
            BodyRecipe(maximum_vertices=7),
            BodyRecipe(maximum_faces=11),
            BodyRecipe(maximum_volumes=1),
        ):
            with (
                patch(
                    "bridge_end_body.close_surface",
                    side_effect=AssertionError("allocated"),
                ),
                self.assertRaisesRegex(ValueError, "budget"),
            ):
                build_body(sloping_deck(), "synthetic", recipe=recipe, method="faces")

    def test_degenerate_reversed_and_duplicate_faces_are_rejected(self):
        source = sloping_deck()
        for faces in (
            np.array([[0, 0, 1]], dtype=np.uint32),
            source.triangles[:, ::-1],
            np.vstack((source.triangles, source.triangles[:1])),
        ):
            with self.assertRaises(ValueError):
                build_body(Mesh(source.vertices, faces), "synthetic")

    def test_invalid_recipes_inputs_and_methods_are_rejected(self):
        for options in (
            {"depth_m": 0},
            {"depth_m": np.nan},
            {"depth_m": 3},
            {"headroom_m": -1},
            {"headroom_m": np.inf},
            {"maximum_vertices": False},
            {"maximum_faces": 0},
            {"maximum_volumes": 8193},
        ):
            with self.assertRaises(ValueError):
                BodyRecipe(**options)
        with self.assertRaises(ValueError):
            build_body(sloping_deck(), "synthetic", method="unknown")
        with self.assertRaises(TypeError):
            build_body(None, "synthetic", method="faces")

    def test_missing_a_lower_body_face_is_not_hidden_by_valid_top_and_space(self):
        product, report = build_body(sloping_deck(), "synthetic")
        body = product["body"]
        product = product | {
            "body": Mesh(body.vertices, np.delete(body.triangles, 2, axis=0))
        }
        with self.assertRaises(ValueError):
            qualify_body(product, report)


class ActualEndBodies(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source, cls.index, cls.prepared, cls.ground, _ = actual_end_inputs()
        cls.groups = [
            v["source_terminal"]["group"]
            for v in cls.ground["terminal_values"]
            if any(
                cls.index.index[1][m.edge[:2]].bridge
                for m in cls.index.groups_by_node[
                    v["source_terminal"]["source_node"]
                ].members
            )
        ]
        cls.outputs = {
            g: build_end_body(cls.prepared, cls.ground, cls.index, g)
            for g in cls.groups
        }

    def test_both_actual_end_bodies_retain_entire_original_surfaces_and_caps(self):
        for group, (product, report) in self.outputs.items():
            surface, old = build_end_surface(
                self.prepared, self.ground, self.index, group
            )
            checked = qualify_body(product, report["body"])
            self.assertEqual(
                (checked["top_vertices_exact"], checked["top_faces_exact"]), (22, 20)
            )
            self.assertEqual((checked["vertices"], checked["faces"]), (44, 84))
            self.assertEqual(
                surface.vertices.tobytes(), product["surface"].vertices.tobytes()
            )
            self.assertEqual(
                surface.triangles.tobytes(), product["surface"].triangles.tobytes()
            )
            cap = list(report["near_ground_vertices"])
            self.assertEqual(
                self.ground["surface"].vertices[cap].tobytes(),
                product["body"].vertices[:2].tobytes(),
            )
            self.assertEqual(report["source_binding"], old["source_binding"])
            self.assertEqual(report["ground_binding"], old["ground_binding"])
            self.assertEqual(report["bridge_member"], old["bridge_member"])
            self.assertGreater(
                report["body"]["geometry"]["minimum_normal_thickness_m"], 0.24
            )
            self.assertEqual(report["qualified_contacts"], 0)
            self.assertEqual(report["qualified_world_joins"], 0)
            self.assertIn("terrain_and_crossing_clearance", report["pending"])
            self.assertIn("whole_bridge", report["pending"])

    def test_actual_face_control_has_same_body_and_height_query_regions(self):
        for group, (whole, report) in self.outputs.items():
            faces, other = build_end_body(
                self.prepared, self.ground, self.index, group, method="faces"
            )
            self.assertEqual(
                whole["body"].vertices.tobytes(), faces["body"].vertices.tobytes()
            )
            self.assertEqual(
                whole["body"].triangles.tobytes(), faces["body"].triangles.tobytes()
            )
            self.assertEqual(qualify_body(faces, other["body"])["occupied_volumes"], 20)
            self.assertLess(
                qualify_body(whole, report["body"])["maximum_exact_cap_padding_m"],
                1e-10,
            )
            low = whole["body"].vertices[:, 2].min()
            high = whole["body"].vertices[:, 2].max()
            for a, b in (
                (low - 1, low - 0.01),
                (high + 0.01, high + 1),
                (low - 1, high + 1),
                (low + 0.1, low + 0.2),
                ((low + high) / 2, (low + high) / 2 + 0.1),
            ):
                difference = conflict(whole, a, b).symmetric_difference(
                    conflict(faces, a, b)
                )
                self.assertLess(difference.area, 1e-8)

    def test_source_missing_or_changed_field_is_not_promoted_by_a_closed_body(self):
        for report in (
            self.ground | {"state": "Pending"},
            self.ground | {"binding": {}},
        ):
            with self.assertRaisesRegex(ValueError, "binding"):
                build_end_body(self.prepared, report, self.index, self.groups[0])

    def test_occupancy_preserves_capture_bound_local_streetway_and_frame(self):
        expected = {"489": 1452, "940": 1453}
        for group, (product, report) in self.outputs.items():
            way = expected[group.rsplit(":", 1)[-1]]
            self.assertEqual(report["bridge_source_way"], way)
            self.assertEqual(
                report["body"]["occupancy_sources"], ((f"{group}:StreetWay:{way}",),)
            )
            self.assertTrue(
                product["occupancies"][0].owner_id.startswith("bridge-end:" + group)
            )
        anchor = self.prepared[0]
        changed = ((anchor[0] + 0.01, anchor[1]), self.prepared[1])
        with self.assertRaisesRegex(ValueError, "frame_binding"):
            build_end_body(changed, self.ground, self.index, self.groups[0])

    def test_request_order_preserves_products_and_original_source_unknowns(self):
        unknown = self.source.unknown_nodes
        before = self.ground["surface"].vertices.tobytes()
        for group in self.groups[::-1]:
            product, _ = build_end_body(self.prepared, self.ground, self.index, group)
            self.assertEqual(
                product["body"].vertices.tobytes(),
                self.outputs[group][0]["body"].vertices.tobytes(),
            )
        self.assertEqual(unknown, self.source.unknown_nodes)
        self.assertEqual(before, self.ground["surface"].vertices.tobytes())


if __name__ == "__main__":
    unittest.main()

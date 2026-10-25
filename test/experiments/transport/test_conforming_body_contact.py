"""Whole contact extrema, indexed seams, original identity and bounded failure checks."""

import json
import unittest

import numpy as np
import shapely
from body_ground_support import prepare
from body_ground_support_experiment import control_body
from conforming_body_contact import conform, contact_error, grid, local_conform
from conforming_body_contact_experiment import heights
from world_stair_reference import FIXTURE, construct


class ConformingBodyContact(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case = construct(json.loads(FIXTURE.read_text()))
        cls.body = cls.case["data"]
        cls.saved = {
            k: v.copy() for k, v in cls.body.items() if isinstance(v, np.ndarray)
        }
        cls.model = prepare(cls.body, 1)
        low, high = (
            cls.body["body_vertices"][:, :2].min(axis=0),
            cls.body["body_vertices"][:, :2].max(axis=0),
        )
        cls.xy, cls.old_faces = grid(
            np.arange(low[0] - 10, high[0] + 10.25, 0.5),
            np.arange(low[1] - 10, high[1] + 10.25, 0.5),
        )
        cls.old_vertices = np.column_stack((cls.xy, heights(cls.model, cls.xy)))
        cls.vertices, cls.faces, cls.work = conform(
            cls.model, cls.xy, cls.old_faces, lambda p: heights(cls.model, p)
        )

    def test_whole_common_refinement_removes_cover_and_gap_that_point_queries_missed(
        self,
    ):
        old = contact_error(self.model, self.old_vertices, self.old_faces)
        new = contact_error(self.model, self.vertices, self.faces)
        self.assertGreater(old["maximum_above_underside_m"], 0.05)
        self.assertGreater(old["maximum_gap_below_underside_m"], 0.06)
        self.assertLess(new["maximum_above_underside_m"], 1e-7)
        self.assertLess(new["maximum_gap_below_underside_m"], 1e-7)
        self.assertAlmostEqual(new["compared_area_m2"], new["cap_area_m2"], places=10)
        self.assertTrue(new["positive_area_overlap"])

    def test_disjoint_terrain_has_no_contact_comparison_or_false_full_coverage(self):
        high = self.body["body_vertices"][:, :2].max(axis=0) + 5
        xy = high + np.array([[0, 0], [1, 0], [0, 1]])
        vertices = np.column_stack((xy, np.zeros(3)))
        result = contact_error(self.model, vertices, np.array([[0, 1, 2]]))
        self.assertFalse(result["positive_area_overlap"])
        self.assertEqual(result["common_refinement_vertices"], 0)
        self.assertEqual(result["compared_area_m2"], 0)
        self.assertGreater(result["cap_area_m2"], 0)
        self.assertEqual(result["maximum_above_underside_m"], 0)
        self.assertEqual(result["maximum_gap_below_underside_m"], 0)

    def test_indexed_interior_edges_have_two_incident_triangles_and_boundary_is_external(
        self,
    ):
        edges = np.sort(self.faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
        edges, count = np.unique(edges, axis=0, return_counts=True)
        self.assertTrue(np.all(count <= 2))
        boundary = self.vertices[edges[count == 1], :2]
        low, high = self.xy.min(axis=0), self.xy.max(axis=0)
        external = np.zeros(len(boundary), dtype=bool)
        for axis in range(2):
            external |= np.all(boundary[:, :, axis] == low[axis], axis=1)
            external |= np.all(boundary[:, :, axis] == high[axis], axis=1)
        self.assertTrue(external.all())
        self.assertTrue(
            shapely.coverage_is_valid(shapely.polygons(self.vertices[self.faces, :2]))
        )

    def test_original_grid_vertices_body_words_masks_and_port_arrays_are_preserved(
        self,
    ):
        for k, v in self.saved.items():
            np.testing.assert_array_equal(self.body[k], v)
        self.assertEqual(len(self.body["bottom_faces"]), 58)
        self.assertEqual(len(self.body["body_vertices"]), 198)
        shared = set(map(tuple, self.vertices[:, :2]))
        self.assertTrue(all(tuple(p) in shared for p in self.xy))
        np.testing.assert_array_equal(
            self.model.source_masks, self.body["face_sources"]
        )
        np.testing.assert_array_equal(
            self.model.sampler.mesh.vertices,
            self.body["body_vertices"][self.model.source_vertex_ids],
        )

    def test_local_matches_whole_world_geometry_without_rebuilding_remote_cells(self):
        vertices, faces, work = local_conform(
            self.model, self.xy, self.old_faces, lambda p: heights(self.model, p)
        )
        np.testing.assert_array_equal(vertices, self.vertices)

        def canonical(indices):
            indices = np.sort(indices, axis=1)
            return indices[np.lexsort(indices.T)]

        np.testing.assert_array_equal(canonical(faces), canonical(self.faces))
        self.assertLess(work["arranged_input_triangles"], work["input_triangles"])
        self.assertLess(work["input_line_segments"], self.work["input_line_segments"])
        errors = contact_error(self.model, vertices, faces)
        self.assertLess(errors["maximum_above_underside_m"], 1e-7)
        self.assertLess(errors["maximum_gap_below_underside_m"], 1e-7)

    def test_either_diagonal_of_a_translated_tilted_planar_control_keeps_contact(self):
        xy, faces = grid(np.arange(-1, 6, 0.5), np.arange(-1, 6, 0.5))
        for diagonal in (0, 1):
            body = control_body(diagonal)
            body["body_vertices"] += [0.13, 0.17, 23.0]
            model = prepare(body, 1)
            vertices, result, _ = conform(
                model, xy, faces, lambda p, m=model: heights(m, p)
            )
            errors = contact_error(model, vertices, result)
            self.assertLess(errors["maximum_above_underside_m"], 1e-7)
            self.assertLess(errors["maximum_gap_below_underside_m"], 1e-7)

    def test_hole_in_selected_underside_is_not_a_hole_in_complete_terrain(self):
        xy = np.array(
            [[0, 0], [4, 0], [4, 4], [0, 4], [1, 1], [3, 1], [3, 3], [1, 3]],
            dtype=float,
        )
        quads = np.array([[0, 1, 5, 4], [1, 2, 6, 5], [2, 3, 7, 6], [3, 0, 4, 7]])
        body = {
            "body_vertices": np.column_stack((xy, np.full(8, 25.0))),
            "bottom_faces": np.vstack((quads[:, [0, 1, 2]], quads[:, [0, 2, 3]])),
            "face_sources": np.ones(8, dtype=np.uint64),
        }
        model = prepare(body, 1)
        xy, faces = grid(np.arange(-1, 6, 0.5), np.arange(-1, 6, 0.5))
        vertices, result, _ = conform(model, xy, faces, lambda p: heights(model, p))
        domain = shapely.coverage_union_all(shapely.polygons(vertices[result, :2]))
        self.assertTrue(domain.covers(shapely.box(1, 1, 3, 3)))
        self.assertEqual(
            contact_error(model, vertices, result)["maximum_above_underside_m"], 0
        )

    def test_cancel_and_segment_budget_stop_before_callback_without_mutating_inputs(
        self,
    ):
        saved_xy, saved_faces = self.xy.copy(), self.old_faces.copy()
        for options in (
            {"cancelled": lambda: True},
            {"max_segments": 0},
            {"max_faces": 0},
        ):
            with self.assertRaises(ValueError):
                conform(
                    self.model,
                    self.xy,
                    self.old_faces,
                    lambda _: self.fail("unexpected height callback"),
                    **options,
                )
        np.testing.assert_array_equal(self.xy, saved_xy)
        np.testing.assert_array_equal(self.old_faces, saved_faces)

    def test_cancellation_after_noding_publishes_nothing_and_never_samples_heights(
        self,
    ):
        calls = []

        def cancelled():
            calls.append(1)
            return len(calls) == 2

        with self.assertRaisesRegex(ValueError, "after noding"):
            conform(
                self.model,
                self.xy,
                self.old_faces,
                lambda _: self.fail("unexpected height callback"),
                cancelled=cancelled,
            )

    def test_unresolved_height_callback_and_truncated_domain_do_not_publish_meshes(
        self,
    ):
        with self.assertRaisesRegex(ValueError, "unresolved heights"):
            conform(
                self.model, self.xy, self.old_faces, lambda p: np.full(len(p), np.nan)
            )
        with self.assertRaisesRegex(ValueError, "complete selected body"):
            conform(
                self.model,
                self.xy,
                self.old_faces[:1],
                lambda _: self.fail("unexpected height callback"),
            )


if __name__ == "__main__":
    unittest.main()

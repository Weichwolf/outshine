"""Canonical boundaries, coincident selections, bounded work and complete consumers."""

import copy
import json
import unittest
from unittest.mock import patch

import compare_mvt_native_body
import numpy as np
import shapely
import test_independent_flights
from canonical_station_cuts import refine
from native_local_station_cuts import FIXTURE, transformed
from stair_planar_lineage import overlap_faces


class CanonicalKernelTests(unittest.TestCase):
    def setUp(self):
        self.xy = np.array([[0, 0], [2, 0], [2, 2], [0, 2]], dtype=float)
        self.faces = np.array([[0, 1, 2], [0, 2, 3]], dtype=np.uint32)
        self.masks = np.array([1, 2], dtype=np.uint64)
        self.axes = [
            {
                "origin_xy_m": [0, 0],
                "tangent": [1, 0],
                "cuts_m": [1],
                "initial_faces": [0],
            }
        ]

    def run_cut(self, axes=None, **budgets):
        return refine(
            self.xy,
            self.faces,
            self.masks,
            self.axes if axes is None else axes,
            **budgets,
        )

    def test_only_actual_edge_neighbour_is_conformed(self):
        result = self.run_cut()
        self.assertEqual(len(result["xy_m"]), 6)
        self.assertEqual(len(result["triangles"]), 5)
        self.assertNotIn((1, 2), map(tuple, result["xy_m"]))
        self.assertTrue(
            shapely.coverage_is_valid(
                shapely.polygons(result["xy_m"][result["triangles"]])
            )
        )
        self.assertEqual(
            shapely.union_all(
                shapely.polygons(result["xy_m"][result["triangles"]])
            ).area,
            4,
        )
        np.testing.assert_array_equal(
            result["source_masks"], self.masks[result["parents"]]
        )
        selected = result["stations_m"][0][result["triangles"][result["parents"] == 0]]
        self.assertFalse(
            ((selected.min(axis=1) < 1) & (selected.max(axis=1) > 1)).any()
        )

    def test_same_frame_coincident_cuts_union_original_selections_once(self):
        axes = copy.deepcopy(self.axes) * 2
        axes[1] = dict(axes[1], initial_faces=[1])
        result = self.run_cut(axes)
        whole = self.run_cut([dict(self.axes[0], initial_faces=[0, 1])])
        self.assertEqual(result["work"]["unique_station_cuts"], 1)
        self.assertEqual(result["work"]["declared_station_cuts"], 2)
        for key in ("xy_m", "triangles", "parents", "source_masks"):
            np.testing.assert_array_equal(result[key], whole[key])
        np.testing.assert_array_equal(
            result["stations_m"][0].view(np.uint64),
            result["stations_m"][1].view(np.uint64),
        )

    def test_failed_limits_and_cancellation_leave_input_arrays_unchanged(self):
        before = [a.tobytes() for a in (self.xy, self.faces, self.masks)]
        axes = copy.deepcopy(self.axes)
        for budget in (
            {"max_vertices": 4},
            {"max_faces": 2},
            {"max_face_visits": 1},
            {"cancelled": lambda: True},
        ):
            with self.subTest(budget=budget):
                with self.assertRaises(ValueError):
                    self.run_cut(**budget)
                self.assertEqual(
                    before, [a.tobytes() for a in (self.xy, self.faces, self.masks)]
                )
                self.assertEqual(axes, self.axes)

    def test_invalid_selection_and_frame_are_refused(self):
        for axis in (
            dict(self.axes[0], initial_faces=[2]),
            dict(self.axes[0], tangent=[0, 0]),
            dict(self.axes[0], cuts_m=[float("nan")]),
        ):
            with self.assertRaises(ValueError):
                self.run_cut([axis])

    def test_highest_source_bit_and_empty_selection_are_preserved(self):
        self.masks[0] = np.uint64(1) << np.uint64(63)
        result = self.run_cut([dict(self.axes[0], initial_faces=[])])
        np.testing.assert_array_equal(result["source_masks"], self.masks)
        np.testing.assert_array_equal(result["xy_m"], self.xy)
        np.testing.assert_array_equal(result["triangles"], self.faces)
        self.assertEqual(result["work"]["face_visits"], 0)

    def test_fixed_native_input_keeps_whole_profiles_without_global_planes(self):
        data = json.loads(FIXTURE.read_text())
        for angle, translation in ((0, [0, 0]), (0, [800, -300]), (0.37, [0, 0])):
            with self.subTest(angle=angle, translation=translation):
                xy, axes, profiles = transformed(data, angle, translation)
                result = refine(
                    xy, np.array(data["triangles"]), data["source_masks"], axes
                )
                for profile in profiles:
                    selected = (result["source_masks"] & (1 << profile["owner"])) != 0
                    cells = shapely.polygons(
                        result["xy_m"][result["triangles"][selected]]
                    )
                    self.assertTrue(
                        shapely.union_all(cells).covers(
                            shapely.LineString(profile["xy_m"])
                        )
                    )
                np.testing.assert_array_equal(
                    result["source_masks"],
                    np.asarray(data["source_masks"])[result["parents"]],
                )

    def test_frozen_global_profile_loss_remains_a_rejected_control(self):
        data = json.loads(FIXTURE.read_text())
        axes = [dict(axis, initial_faces=None) for axis in data["axes"]]
        result = refine(
            data["xy_m"], np.array(data["triangles"]), data["source_masks"], axes
        )
        coverage = []
        for profile in data["profiles"]:
            selected = (result["source_masks"] & (1 << profile["owner"])) != 0
            cells = shapely.polygons(result["xy_m"][result["triangles"][selected]])
            coverage.append(
                shapely.union_all(cells).covers(shapely.LineString(profile["xy_m"]))
            )
        self.assertFalse(all(coverage))


class CanonicalFlightTests(test_independent_flights.IndependentFlightTests):
    cut_stage = "canonical"

    def test_height_correspondence_uses_the_same_refined_canonical_faces(self):
        for case in self.cases.values():
            plan = case["solver_plan"]
            mesh = plan["canonical_station_mesh"]
            mapped = overlap_faces(plan)
            for owner, part in enumerate(plan["parts"]):
                ids = plan["part_global_faces"][owner]
                np.testing.assert_array_equal(
                    plan["xy"][part], mesh["xy_m"][mesh["triangles"][ids]]
                )
                self.assertEqual(len(mapped[owner]), len(part))
            self.assertLess(len(mesh["xy_m"]), len(plan["xy"]))
        mesh = self.cases["stacked"]["solver_plan"]["canonical_station_mesh"]
        np.testing.assert_array_equal(
            mesh["stations_m"][0].view(np.uint64), mesh["stations_m"][1].view(np.uint64)
        )


class CanonicalOriginalTests(unittest.TestCase):
    def test_complete_original_consumes_local_canonical_cuts(self):
        prepare = compare_mvt_native_body.prepare_native_body_plan

        def canonical(case, sources, proposal, native, **kwargs):
            proposal = dict(
                proposal,
                station_cut_stage="canonical",
                station_cut_scope="flight_footprint",
            )
            return prepare(case, sources, proposal, native, **kwargs)

        with patch.object(
            compare_mvt_native_body, "prepare_native_body_plan", canonical
        ):
            plan, _data, _baseline, products, report = compare_mvt_native_body.run(
                "highs-ds"
            )
        self.assertEqual(report["orientation"]["invalid_float32_faces"], 0)
        self.assertLess(
            max(report["rendered_native_contacts"]["height_errors_m"].values()), 1e-10
        )
        self.assertEqual(len(products["external_contacts"]), 4)
        self.assertEqual(len(products["terrain_reservations"]), 3)
        self.assertEqual(plan["station_refinement"]["cut_stage"], "canonical")


if __name__ == "__main__":
    unittest.main()

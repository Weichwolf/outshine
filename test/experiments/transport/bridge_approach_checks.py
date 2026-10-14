"""Whole-face field error, shared ports, source contacts and bounded alternatives."""

import copy
import hashlib
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from adaptive_profile_surface import error_bounds, exact_heights, seed
from bridge_approach_recipe import build, height_field, oriented_path
from outshine_ref.contracts import Mesh
from outshine_ref.transport import close_surface, plan_vertical_transition
from projected_profile_surface import projected_mesh
from shared_bridge_checks import FIXTURE as ROOT_FIXTURE
from shared_bridge_recipe import build as build_root
from surface_height_checks import normal_gradients
from surface_solid_checks import audit_closed

FIXTURE = Path(__file__).with_name("fixtures") / "bridge-approaches-vienna.json"
PIN = "52ce1d0895a9bfd65a9a6ed4033ae6f65b08a9cf8294adf34161ccc492b346e4"


def error_probe(mesh, curve, origin, direction, first):
    barycentric = np.array(
        [
            [1, 0, 0],
            [0, 1, 0],
            [0, 0, 1],
            [0.5, 0.5, 0],
            [0.5, 0, 0.5],
            [0, 0.5, 0.5],
            [1 / 3] * 3,
            [0.2, 0.3, 0.5],
        ]
    )
    samples = np.einsum("pk,fkj->fpj", barycentric, mesh.vertices[mesh.triangles])
    stations = (samples[:, :, :2].reshape(-1, 2) - origin) @ direction
    expected = exact_heights(stations, curve, first).reshape(len(mesh.triangles), -1)
    return float(abs(samples[:, :, 2] - expected).max())


def rectangle_mesh(bounds, height):
    x0, y0, x1, y1 = bounds
    return Mesh(
        np.array(
            [[x0, y0, height], [x1, y0, height], [x1, y1, height], [x0, y1, height]]
        ),
        np.array([[0, 1, 2], [0, 2, 3]]),
    )


class ApproachChecks(unittest.TestCase):
    def test_station_slices_bound_whole_faces_on_a_rotated_domain_with_hole(self):
        domain = shapely.box(-10, -5, 310, 5).difference(shapely.box(130, -1, 170, 1))
        angle = 0.7
        matrix = np.array(
            [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
        )
        coordinates = shapely.get_coordinates(domain) @ matrix.T + [1200, -700]
        moved = shapely.set_coordinates(domain, coordinates)
        axis = matrix @ [1, 0]
        curve, _ = plan_vertical_transition(6, 0, 300, grade=0.04)
        from outshine_ref.transport import sample_vertical_transition

        table, note = sample_vertical_transition(curve)
        table = np.vstack(([-11, 6], table, [311, 0]))
        mesh, report = projected_mesh(moved, [1200, -700], axis, table)
        self.assertTrue(report["each_face_within_one_height_chord"])
        self.assertLessEqual(
            error_probe(mesh, curve, [1200, -700], axis, 0),
            note["maximum_height_chord_bound_m"] + 1e-7,
        )
        self.assertAlmostEqual(
            audit_closed(close_surface(mesh, 0.35)), moved.area * 0.35, places=6
        )

    def test_midpoint_certificate_alone_does_not_bound_facet_grade(self):
        xy = np.array([[0, 0], [1, 0.001], [2, 0]])
        # Quadratic C1 rise near its flat endpoint: tiny height errors amplify cross-grade.
        curve, _ = plan_vertical_transition(0, 1, 100, grade=0.025)
        heights = exact_heights(xy[:, 0], curve, 0)
        faces = np.array([[0, 2, 1]])
        bound = error_bounds(xy[:, 0], faces, curve, 0)[0]
        self.assertLess(bound, 0.01)
        gradient = normal_gradients(xy, faces, heights)
        self.assertGreater(np.linalg.norm(gradient[0]), 0.025)

    def test_end_contact_regions_are_constraints_in_both_constructions(self):
        domain = shapely.box(-100, -2, 400, 2)
        curve, _ = plan_vertical_transition(6, 0, 300, grade=0.04)
        xy, faces, _ = seed(domain, np.array([0, 0]), np.array([1, 0]), curve, 0)
        station = xy[:, 0]
        ranges = station[faces]
        self.assertTrue(
            np.all((ranges.max(axis=1) <= 1e-8) | (ranges.min(axis=1) >= -1e-8))
        )
        self.assertTrue(
            np.all(
                (ranges.max(axis=1) <= 300 + 1e-8) | (ranges.min(axis=1) >= 300 - 1e-8)
            )
        )

    def test_triangle_and_time_caps_do_not_return_an_unchecked_surface(self):
        domain = shapely.box(0, 0, 10, 10)
        for args in ({"max_faces": 1}, {"seconds": 1e-12}):
            with self.assertRaises(ValueError):
                projected_mesh(domain, [0, 0], [1, 0], [[-1, 0], [11, 1]], **args)
        with self.assertRaises(ValueError):
            projected_mesh(domain, [0, 0], [2, 0], [[-1, 0], [11, 1]])

    def test_incompatible_or_unjoined_sources_are_rejected(self):
        case = json.loads(FIXTURE.read_text())["approaches"][0]
        for kind in ("gap", "tunnel", "mode", "level"):
            altered = copy.deepcopy(case)
            if kind == "gap":
                altered["ground"]["xy"] = (
                    np.asarray(altered["ground"]["xy"]) + [1, 0]
                ).tolist()
            elif kind == "level":
                altered["ground"]["properties"]["layer"] = 1
            else:
                altered["ground"]["properties"][
                    "brunnel" if kind == "tunnel" else "subclass"
                ] = "tunnel" if kind == "tunnel" else "tram"
            with self.assertRaises(ValueError):
                oriented_path(altered)

    def test_ground_source_alone_is_too_short_for_the_existing_deck_height(self):
        captured = json.loads(FIXTURE.read_text())
        (root, _lower), _ = build_root(json.loads(ROOT_FIXTURE.read_text())["sources"])
        for case in captured["approaches"]:
            ground = case["ground"]
            length = shapely.LineString(ground["xy"]).length
            target = ground["end_heights_m"][1 - case["ground_join_endpoint"]]
            for method in ("parabolic", "cubic"):
                curve, report = plan_vertical_transition(
                    root.vertices[0, 2], target, length, grade=0.024, method=method
                )
                self.assertIsNone(curve)
                self.assertEqual(report["reason"], "insufficient_profile_length")

    def test_pinned_real_full_plan_all_alternatives(self):
        self.assertEqual(hashlib.sha256(FIXTURE.read_bytes()).hexdigest(), PIN)
        inputs = json.loads(FIXTURE.read_text())
        root_input = json.loads(ROOT_FIXTURE.read_text())
        self.assertEqual(
            inputs["root_fixture_sha256"],
            hashlib.sha256(ROOT_FIXTURE.read_bytes()).hexdigest(),
        )
        self.assertEqual(len(inputs["approaches"]), 2)
        self.assertEqual(len(inputs["conflicts"]), 7)
        self.assertEqual(
            sum(
                len(i["all_axis_ids"]) + len(i["polygon_ids"]) for i in inputs["inputs"]
            ),
            3259,
        )
        original = copy.deepcopy(inputs)
        (root, lower), _ = build_root(root_input["sources"])
        for method in ("parabolic", "cubic"):
            field, profile = height_field(inputs["approaches"], root, lower, method)
            domain, origin, direction, _table, curve = field
            for geometry in ("slices", "midpoints"):
                mesh, note = build(
                    inputs["approaches"],
                    root,
                    lower,
                    method=method,
                    geometry_method=geometry,
                )
                self.assertTrue(note["valid"], note)
                self.assertLessEqual(note["maximum_gradient_norm"], 0.025 * (1 + 1e-7))
                self.assertLessEqual(
                    note["geometry"]["whole_face_height_error_bound_m"], 0.0100001
                )
                self.assertLessEqual(
                    error_probe(
                        mesh,
                        curve,
                        origin,
                        direction,
                        profile["transition_station_m"][0],
                    ),
                    0.0100001,
                )
                self.assertEqual(note["maximum_existing_deck_contact_error_m"], 0)
                self.assertGreaterEqual(note["clearance"]["minimum_clearance_m"], 4.5)
                self.assertAlmostEqual(
                    audit_closed(close_surface(mesh, 0.35)),
                    domain.area * 0.35,
                    places=6,
                )
                self.assertAlmostEqual(
                    note["profile"]["maximum_far_port_height_change_m"], 0.08984375
                )
        self.assertEqual(inputs, original)

    def test_large_far_prior_difference_is_local_failure(self):
        inputs = json.loads(FIXTURE.read_text())
        root_input = json.loads(ROOT_FIXTURE.read_text())
        (root, lower), _ = build_root(root_input["sources"])
        case = inputs["approaches"][0]
        case["ground"]["end_heights_m"][1 - case["ground_join_endpoint"]] += 1
        mesh, report = build(inputs["approaches"], root, lower)
        self.assertIsNone(mesh)
        self.assertIn("far-port height correction", report["detail"])

    def test_products_keep_all_originals_and_explicit_consumer_rejection(self):
        from bridge_approach_experiment import inputs, originals, usage
        from shared_bridge_products import products

        captured, sources, base, _ = inputs()
        mesh, report = build(captured["approaches"], *base)
        rows = usage(captured, sources, report)
        self.assertEqual(len(rows), 3259)
        self.assertEqual(len({r["source_id"] for r in rows}), 3259)
        self.assertEqual(sum(r["status"] == "used" for r in rows), 27)
        self.assertEqual(
            sum(r["reason"] == "buildings_floor_surface_not_planned" for r in rows), 10
        )
        self.assertTrue(all(r["status"] == "used" or r["reason"] for r in rows))
        retained = originals(captured, sources)
        plan, binding = products(
            retained,
            (mesh, base[1]),
            report,
            top_roles=("deck", "approach_bridge", "approach_ground"),
            top_kind="joined_transport",
        )
        self.assertEqual(len(binding["source_originals"]), 44)
        self.assertEqual(len(plan.meshes), 2)
        represented = {s for c in binding["cells"] for s in c["source_ids"]}
        self.assertEqual(len(represented), 27)
        self.assertFalse(
            represented
            & {
                s["source_id"]
                for s in retained
                if s["role"] in ("deferred", "unresolved")
            }
        )
        self.assertEqual(
            {s["source_id"] for s in retained},
            {s["source_id"] for s in binding["source_originals"]},
        )


if __name__ == "__main__":
    unittest.main()

"""Original multi-loop coverage and whole-field controls without Source admission."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from ground_cyclic_checks import audit
from ground_cyclic_source import source_component
from ground_multicycle import generate
from outshine_ref.contracts import Mesh
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._geodesy import positions_in_frame
from outshine_ref.transport.canonical_ports._ground_domain import mesh_parts
from replay_ground_multicycle import check, prepare_inputs
from test_ground_cycle import index_for


def grid(cells):
    columns = cells + 1
    edges = [
        (r * columns + c, r * columns + c + 1) for r in (0, 1) for c in range(cells)
    ]
    edges += [(c, c + columns) for c in range(columns)]
    index = index_for(edges)
    index[0].update(
        {
            n: (16.37 + (n % columns) * 0.0003, 48.21 + (n // columns) * 0.0003)
            for n in index[0]
        }
    )
    return index


class BoundedMulticycleTopology(unittest.TestCase):
    def test_three_and_eight_loop_analytic_constant_or_affine_fields(self):
        for cells in (3, 8):
            index = grid(cells)
            anchor_node, _, _ = source_component(
                index,
                tuple(index[1]),
                minimum_cycles=3,
                maximum_cycles=8,
                maximum_spans=32,
                prefix="multicycle",
            )
            anchor = index[0][anchor_node]
            for affine in (False, True):

                def terrain(points, *, affine=affine, anchor=anchor):
                    slope = np.array([0.01, 0.015]) if affine else np.zeros(2)
                    base = positions_in_frame(anchor, points, np.zeros(len(points)))
                    up = positions_in_frame(anchor, points, np.ones(len(points))) - base
                    return (150 + base[:, :2] @ slope - base[:, 2]) / (
                        up[:, 2] - up[:, :2] @ slope
                    )

                result = generate(index, tuple(index[1]), terrain, PortBudget())
                checked = audit(result, expected_holes=cells, prefix="multicycle")
                self.assertEqual(checked["holes"], cells)
                self.assertEqual(checked["mesh_euler_characteristic"], 1 - cells)
                self.assertEqual(set(result["source_members"]), set(index[1]))
                self.assertLess(
                    result["quality"]["maximum_original_prior_vertex_error_m"], 1e-4
                )
                self.assertLess(
                    result["quality"]["finite_contacts"][
                        "maximum_finite_contact_grade_jump"
                    ],
                    1e-7,
                )

    def test_unsupported_topology_profile_and_selection_fail_before_work(self):
        cases = [grid(2), grid(9)]
        for field, value in (
            ("maximum_gradient", None),
            ("layer", 1),
            ("width_m", np.nan),
        ):
            index = grid(3)
            setattr(index[1][(0, 0)], field, value)
            cases.append(index)
        index = grid(3)
        index[2][0].append((99, 0))
        cases.append(index)
        with patch("ground_multicycle.plan_source_surface") as build:
            for index in cases:
                with self.assertRaises(ValueError):
                    generate(index, tuple(index[1]), self.fail, PortBudget())
            with self.assertRaisesRegex(ValueError, "selected_span_budget"):
                index = grid(3)
                generate(
                    index,
                    tuple(index[1]),
                    self.fail,
                    replace(PortBudget(), selected_spans=9),
                )
            build.assert_not_called()

    def test_new_hole_capacity_is_explicit_and_bounded_without_widening_default(self):
        for options in (
            {"expected_holes": 3},
            {"expected_holes": 3, "maximum_holes": True},
            {"expected_holes": 3, "maximum_holes": 9},
            {"expected_holes": 9, "maximum_holes": 8},
        ):
            with self.assertRaisesRegex(ValueError, "hole_budget"):
                mesh_parts([], [], [], [], [], [], **options)


class OriginalWholeMulticycle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.index, cls.cases, terrain, cls.budget, cls.metadata = prepare_inputs()
        cls.terrain = staticmethod(terrain)
        cls.case = next(c for c in cls.cases if c["seed"] == (2061326783104604389, 0))

    def test_all_original_cases_have_explicit_results_and_whole_source_coverage(self):
        self.assertEqual(len(self.cases), 4)
        for method in ("adaptive", "linear"):
            accepted = 0
            for case in self.cases:
                try:
                    result = generate(
                        self.index,
                        case["selected"],
                        self.terrain,
                        self.budget,
                        cut_method=method,
                    )
                except ValueError as error:
                    self.assertIn(
                        str(error),
                        (
                            "missing_source_profile_recipe",
                            "multicycle_parallel_or_self_span",
                        ),
                    )
                    continue
                accepted += 1
                checked = check(result)
                self.assertEqual(
                    (
                        len(result["source_members"]),
                        len(result["geometry"]["source_node_ids"]),
                    ),
                    (25, 20),
                )
                self.assertEqual(checked["holes"], 6)
                self.assertEqual(checked["whole_sections"], 54)
                self.assertTrue(result["quality"]["geometry_complete"])
                self.assertLessEqual(
                    result["quality"]["maximum_original_prior_vertex_error_m"], 0.5
                )
                self.assertEqual(set(result["source_members"]), set(case["selected"]))
                self.assertTrue(case["original_unknown_nodes"])
            self.assertEqual(accepted, 1)
        self.assertEqual(self.metadata["qualified_world_joins"], 0)

    def test_filled_free_regions_and_forged_source_axes_fail_the_independent_check(
        self,
    ):
        result = generate(self.index, self.case["selected"], self.terrain, self.budget)
        forged = dict(
            result,
            geometry=dict(
                result["geometry"],
                source_edges_local=result["geometry"]["source_edges_local"][:-1],
            ),
        )
        with self.assertRaisesRegex(ValueError, "original_plane_faces"):
            audit(forged, expected_holes=6, prefix="multicycle")
        vertices, triangles = result["mesh"].vertices, result["mesh"].triangles
        domain = shapely.coverage_union_all(shapely.polygons(vertices[triangles, :2]))
        self.assertEqual(len(domain.interiors), 6)
        self.assertTrue(
            all(shapely.Polygon(ring).area > 0 for ring in domain.interiors)
        )
        filled = shapely.get_coordinates(
            shapely.constrained_delaunay_triangles(shapely.Polygon(domain.interiors[0]))
        ).reshape(-1, 4, 2)[:, :3]
        lookup = {tuple(p): i for i, p in enumerate(vertices[:, :2])}
        faces = np.array([[lookup[tuple(p)] for p in face] for face in filled])
        q = vertices[faces]
        reversed_faces = np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0])[:, 2] < 0
        faces[reversed_faces] = faces[reversed_faces][:, [0, 2, 1]]
        forged = dict(result, mesh=Mesh(vertices, np.vstack((triangles, faces))))
        with self.assertRaisesRegex(ValueError, "hole_topology"):
            audit(forged, expected_holes=6, prefix="multicycle")

    def test_missing_dem_and_low_time_do_not_emit_an_unqualified_surface(self):
        with self.assertRaises(ValueError):
            generate(
                self.index,
                self.case["selected"],
                lambda p: np.full(len(p), np.nan),
                self.budget,
            )
        budget = replace(
            self.budget, surface=replace(self.budget.surface, time_limit_s=0)
        )
        with self.assertRaises(ValueError):
            generate(self.index, self.case["selected"], self.fail, budget)


if __name__ == "__main__":
    unittest.main()

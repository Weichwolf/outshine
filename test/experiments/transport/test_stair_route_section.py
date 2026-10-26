import json
import unittest

import numpy as np
from mvt_t_connection_inputs import FIXTURE, inputs
from outshine_ref.contracts import Mesh
from stair_route_audit import audit_section
from stair_route_section import trace_section
from test_mvt_native_body import construct


def rectangle(low, high, height=0):
    vertices = np.array(
        [[low, -1, height], [high, -1, height], [high, 1, height], [low, 1, height]],
        float,
    )
    return Mesh(vertices, np.array([[0, 1, 2], [0, 2, 3]]))


class ExactRouteSections(unittest.TestCase):
    def test_flat_profile_has_exact_endpoints_and_reverses_without_a_tolerance(self):
        mesh = rectangle(-2, 2)
        axis = np.array([[-1.0, 0.0], [1.0, 0.0]])
        forward = trace_section(mesh, axis)
        backward = trace_section(mesh, axis[::-1])
        np.testing.assert_array_equal(forward["xyz_m"][[0, -1], :2], axis)
        np.testing.assert_array_equal(backward["xyz_m"], forward["xyz_m"][::-1])
        np.testing.assert_array_equal(forward["xyz_m"][:, 2], 0)
        self.assertEqual(forward["arclength_m"][-1], 2)

    def test_connected_tread_and_vertical_riser_are_one_actual_chain(self):
        vertices = np.array(
            [
                [-1, -1, 0],
                [0, -1, 0],
                [0, 1, 0],
                [-1, 1, 0],
                [0, -1, 0.17],
                [1, -1, 0.17],
                [1, 1, 0.17],
                [0, 1, 0.17],
            ],
            float,
        )
        faces = np.array(
            [[0, 1, 2], [0, 2, 3], [1, 4, 7], [1, 7, 2], [4, 5, 6], [4, 6, 7]]
        )
        result = trace_section(Mesh(vertices, faces), [[-0.5, 0], [0.5, 0]])
        self.assertAlmostEqual(result["arclength_m"][-1], 1.17)
        vertical = np.diff(result["axis_parameter"]) == 0
        self.assertGreater(np.sum(vertical), 0)
        self.assertAlmostEqual(np.diff(result["xyz_m"][:, 2])[vertical].sum(), 0.17)
        np.testing.assert_array_equal(
            result["xyz_m"][[0, -1]], [[-0.5, 0, 0], [0.5, 0, 0.17]]
        )

    def test_identical_positions_with_unshared_topology_do_not_get_welded(self):
        left, right = rectangle(-2, 0), rectangle(0, 2)
        mesh = Mesh(
            np.vstack([left.vertices, right.vertices]),
            np.vstack([left.triangles, right.triangles + 4]),
        )
        with self.assertRaisesRegex(ValueError, "unbranched chain"):
            trace_section(mesh, [[-1, 0], [1, 0]])

    def test_a_gap_and_two_unselected_height_layers_do_not_become_a_route(self):
        for right in (rectangle(0.05, 2), rectangle(-2, 2, 3)):
            left = (
                rectangle(-2, -0.05) if right.vertices[0, 2] == 0 else rectangle(-2, 2)
            )
            mesh = Mesh(
                np.vstack([left.vertices, right.vertices]),
                np.vstack([left.triangles, right.triangles + 4]),
            )
            with self.assertRaisesRegex(ValueError, "unbranched chain"):
                trace_section(mesh, [[-1, 0], [1, 0]])

    def test_face_budget_precedes_exact_construction(self):
        with self.assertRaisesRegex(ValueError, "before exact work"):
            trace_section(rectangle(-2, 2), [[-1, 0], [1, 0]], max_faces=1)

    def test_independent_3d_audit_rejects_an_invented_support_height(self):
        mesh = rectangle(-2, 2)
        result = trace_section(mesh, [[-1, 0], [1, 0]])
        audit_section(mesh, result)
        result["xyz_m"] = result["xyz_m"] + [0, 0, 0.01]
        with self.assertRaisesRegex(ValueError, "actual 3D support"):
            audit_section(mesh, result)

    def test_coplanar_triangle_is_explicitly_unresolved(self):
        mesh = Mesh(np.array([[-1, 0, 0], [1, 0, 0], [0, 0, 1]]), np.array([[0, 1, 2]]))
        with self.assertRaisesRegex(ValueError, "coplanar"):
            trace_section(mesh, [[-0.5, 0], [0.5, 0]])

    def test_rounded_aliases_keep_the_exact_section_topology_and_provenance(self):
        top = np.nextafter(2.0, np.inf)
        vertices = np.array(
            [[1, 0, 0], [0, 0, 0], [2, 0, 0], [2, 2, 0], [2, top, 0], [0, top, 0]]
        )
        faces = np.array([[0, 2, 3], [0, 3, 4], [0, 4, 5], [0, 5, 1]])
        result = trace_section(Mesh(vertices, faces), [[0.5, 0.01], [1.5, 0.01]])
        self.assertGreater(result["work"]["rounded_zero_segments"], 0)
        self.assertIn([1], result["segment_face_ids"])
        self.assertEqual(len(result["segment_face_ids"]), len(result["xyz_m"]) - 1)
        self.assertAlmostEqual(result["arclength_m"][-1], 1)


class NativeRouteSections(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        case, _sources = inputs()
        native = json.loads(
            FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
        )
        result = construct(case, native)
        cls.data, cls.nav = result[4], result[8]["navigation"]
        cls.faces = np.vstack(
            [cls.data["top_faces"], cls.data["wall_faces"][cls.data["wall_riser"] == 1]]
        )

    def test_all_six_existing_edges_reach_both_endpoints_in_double_and_float32(self):
        for cast in (False, True):
            vertices = self.data["body_vertices"]
            nodes = np.array(self.nav["nodes_xyz_m"])
            if cast:
                vertices, nodes = (
                    vertices.astype("f4").astype(float),
                    nodes.astype("f4").astype(float),
                )
            mesh = Mesh(vertices, self.faces)
            actual_nodes = {}
            for edge in self.nav["edges"]:
                with self.subTest(cast=cast, edge=edge["nodes"]):
                    axis = nodes[edge["nodes"], :2]
                    selected = trace_section(mesh, axis)
                    scanned = trace_section(mesh, axis, method="scan")
                    np.testing.assert_array_equal(selected["xyz_m"], scanned["xyz_m"])
                    self.assertEqual(
                        selected["segment_face_ids"], scanned["segment_face_ids"]
                    )
                    np.testing.assert_array_equal(selected["xyz_m"][[0, -1], :2], axis)
                    for node, point in zip(
                        edge["nodes"], selected["xyz_m"][[0, -1]], strict=True
                    ):
                        if node in actual_nodes:
                            np.testing.assert_array_equal(actual_nodes[node], point)
                        actual_nodes[node] = point
                    self.assertTrue(np.all(np.diff(selected["arclength_m"]) >= 0))
                    self.assertGreater(selected["arclength_m"][-1], 0)
                    self.assertEqual(
                        selected["work"]["rounded_zero_segments"],
                        int(
                            np.sum(
                                np.all(np.diff(selected["xyz_m"], axis=0) == 0, axis=1)
                            )
                        ),
                    )
                    self.assertLess(
                        selected["work"]["candidate_faces"],
                        scanned["work"]["candidate_faces"],
                    )
                    audit_section(mesh, selected)


if __name__ == "__main__":
    unittest.main()

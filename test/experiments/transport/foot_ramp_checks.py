"""Whole field, true zero-mouth closure, access and atomic-edit counterexamples."""

import json
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface
from curb_experiment import builder
from foot_ramp_audit import affine_field, audit, shell_ownership
from foot_ramp_cut import cut
from foot_ramp_edit import edit
from foot_ramp_geometry import Ramp, build
from foot_ramp_sources import source_edges
from outshine_ref.contracts import Mesh


class FootRamps(unittest.TestCase):
    def setUp(self):
        self.triangle = np.array([[-20.0, -20, 0], [60, -20, 0], [-20, 60, 0]])
        self.ground = Mesh(self.triangle, [[0, 1, 2]])
        self.reference = ContactSurface(self.ground)
        self.ramp = build([0, 0], np.eye(2), self.triangle, 0.6)
        self.row = {
            "origin": [0, 0],
            "basis": np.eye(2).tolist(),
            "half": 0.6,
            "landing_end_m": 2.1,
            "triangle": self.triangle.tolist(),
            "shape": self.ramp.shape,
        }
        self.world = shapely.Polygon(self.triangle[:, :2])
        self.traffic = shapely.union_all(
            [shapely.box(-20, -20, 20, 0), shapely.box(-0.75, 0, 0.75, 20)]
        )

    def test_closed_positive_body_has_real_zero_mouth_and_no_buried_layer(self):
        proof = audit(
            self.ramp,
            self.row,
            self.reference,
            self.traffic,
            self.world,
            shapely.Polygon(),
        )
        self.assertGreater(proof["volume_m3"], 1)
        self.assertEqual(proof["maximum_mouth_step_m"], 0)
        self.assertAlmostEqual(self.ramp.body.vertices[:, 2].min(), 0)
        self.assertAlmostEqual(self.ramp.body.vertices[:, 2].max(), 0.12)
        self.assertLess(proof["maximum_grade"], 0.064)

    def test_equivalent_weld_methods_produce_same_actual_indexed_body(self):
        other = build([0, 0], np.eye(2), self.triangle, 0.6, method="scalar")
        np.testing.assert_array_equal(self.ramp.body.vertices, other.body.vertices)
        np.testing.assert_array_equal(self.ramp.body.triangles, other.body.triangles)

    def test_vertex_heights_do_not_prove_a_whole_piecewise_ramp_field(self):
        false = Mesh([[0, 0, 0], [3, 0, 0.12], [0, 1.9, 0.12]], [[0, 1, 2]])
        with self.assertRaisesRegex(ValueError, "whole affine height field"):
            affine_field(false, self.row)

    def test_unshared_zero_height_vertices_are_rejected(self):
        mesh = self.ramp.body
        vertices = np.vstack([mesh.vertices, mesh.vertices[0]])
        faces = mesh.triangles.copy()
        where = np.argwhere(faces == 0)[0]
        faces[tuple(where)] = len(mesh.vertices)
        bad = Ramp(
            Mesh(vertices, faces), self.ramp.lower, self.ramp.upper, self.ramp.shape
        )
        with self.assertRaisesRegex(ValueError, "not shared"):
            shell_ownership(bad)

    def test_existing_protected_structure_is_not_consumed_by_ramp(self):
        with self.assertRaisesRegex(ValueError, "structure exclusion"):
            audit(
                self.ramp,
                self.row,
                self.reference,
                self.traffic,
                self.world,
                self.ramp.shape,
            )

    def test_private_source_is_not_promoted_by_ramp_planning(self):
        raw = {
            "private": {
                "properties": {
                    "class": "path",
                    "subclass": "footway",
                    "access": "private",
                }
            },
            "normal": {"properties": {"class": "path", "subclass": "footway"}},
        }
        graph = {
            "sources": [["private"], ["normal"]],
            "half_widths": [0.75, 0.75],
            "lines": [
                shapely.LineString([[0, 0], [0, 10]]),
                shapely.LineString([[2, 0], [2, 10]]),
            ],
        }
        with TemporaryDirectory(prefix="outshine-tests.transport-ramp-") as tmp:
            p = Path(tmp) / "results/ViennaNetwork/components"
            p.mkdir(parents=True)
            (p / "source-usage.json").write_text(
                json.dumps([{"id": x, "status": "used"} for x in raw])
            )
            with patch(
                "foot_ramp_sources.read_window",
                return_value=({}, raw, [graph], None, {}),
            ):
                feet, _roads, ledger, _work = source_edges(Path(tmp))
        self.assertEqual([f["sources"] for f in feet], [["normal"]])
        self.assertTrue(
            any(
                r["sources"] == ["private"]
                and r["reason"] == "no_recipe_public_walk_access"
                for r in ledger
            )
        )

    def test_failure_after_one_real_staged_cut_does_not_mutate_original_state(self):
        source = builder(self.ground)
        shapes = [shapely.box(-10, 0, -1.25, 2), shapely.box(1.25, 0, 10, 2)]
        states = {
            i: [source.build(s, minimum_height_m=0.1199999)[0]]
            for i, s in enumerate(shapes)
        }
        original = {i: list(members) for i, members in states.items()}
        data = {
            "traffic": self.traffic,
            "world": self.world,
            "obstacles": shapely.Polygon(),
            "owners": [{"kind": "sidewalk"}, {"kind": "sidewalk"}],
            "tree": shapely.STRtree(shapes),
        }
        upper = ContactSurface(Mesh(self.triangle + [0, 0, 0.12], [[0, 1, 2]]))
        calls = []

        def fail_second(*args):
            calls.append(True)
            if len(calls) == 2:
                raise ValueError("injected second cut failure")
            return cut(*args)

        with (
            patch("foot_ramp_edit.solid_cut", side_effect=fail_second),
            self.assertRaisesRegex(ValueError, "second cut failure"),
        ):
            edit(
                self.row,
                data,
                states,
                source,
                self.reference,
                {},
                upper_reference=upper,
                method="solid",
            )
        self.assertEqual(len(calls), 2)
        for key, value in states.items():
            self.assertIs(value[0], original[key][0])


if __name__ == "__main__":
    unittest.main()

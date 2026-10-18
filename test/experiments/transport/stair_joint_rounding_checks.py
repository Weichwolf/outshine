"""Actual quantization failures, flat-owner propagation and bounded absence."""

import json
import unittest
from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget
from outshine_ref.transport.level_heights._rounding import repair
from stair_closed_height_rounding import normalized_grade
from stair_joint_height_checks import strip
from stair_joint_height_fit import level_groups
from stair_joint_height_rounding import fit_rounded_levels
from stair_seed_audit import audit
from stair_seed_crossing_checks import crossing_example
from stair_seed_geometry import build


def captured():
    path = (
        Path(__file__).parents[2]
        / "outshine/data/transport/joint-ground-height-quantization.json"
    )
    saved = json.loads(path.read_text())
    return (
        Mesh(np.array(saved["vertices_before_m"]), np.array([[0, 1, 2]])),
        np.array(saved["reserved_grade_frame"])[None],
    )


class LocalHeightRounding(unittest.TestCase):
    def test_actual_thin_face_needs_adjacent_codes_without_geometry_or_grade_relaxation(
        self,
    ):
        source, frames = captured()
        before = source.vertices.copy()
        self.assertGreater(normalized_grade(before, source.triangles, frames, True), 1)
        result, proof = repair(source, frames, [], [], seconds=1)
        self.assertIsNotNone(result)
        self.assertTrue(proof["valid"])
        self.assertEqual(proof["candidates"], 27)
        self.assertLessEqual(proof["maximum_change_m"], 0.000016)
        np.testing.assert_array_equal(source.vertices, before)
        np.testing.assert_array_equal(result.vertices[:, :2], before[:, :2])
        np.testing.assert_array_equal(
            result.vertices, result.vertices.astype("f4").astype(float)
        )
        for cast in (False, True):
            self.assertLessEqual(
                normalized_grade(result.vertices, source.triangles, frames, cast), 1
            )

    def test_flat_group_updates_vertices_outside_the_bad_face_and_rechecks_neighbors(
        self,
    ):
        source, frame = captured()
        extra = np.array([[-654, 360, source.vertices[0, 2]]])
        mesh = Mesh(
            np.vstack([source.vertices, extra]), np.array([[0, 1, 2], [0, 2, 3]])
        )
        frames = np.concatenate([frame, np.eye(2)[None]])
        group = np.array([0, 3])
        level = float(np.float32(mesh.vertices[0, 2]))
        result, proof = repair(mesh, frames, [group], [level], seconds=1)
        self.assertIsNotNone(result)
        self.assertEqual(result.vertices[0, 2], result.vertices[3, 2])
        self.assertNotEqual(result.vertices[3, 2], level)
        self.assertEqual(proof["batches"][0]["affected_faces"], 2)
        self.assertLessEqual(
            normalized_grade(result.vertices, mesh.triangles, frames, True), 1
        )

    def test_zero_time_candidates_and_unrepairable_neighborhood_return_no_mesh(self):
        source, frames = captured()
        for limits in ({"seconds": 0}, {"seconds": 1, "max_candidates": 0}):
            result, proof = repair(source, frames, [], [], **limits)
            self.assertIsNone(result)
            self.assertEqual(proof["reason"], "local_rounding_budget_exhausted")
        wrong = source.vertices.copy()
        wrong[0, 2] += 1
        result, proof = repair(
            Mesh(wrong, source.triangles), frames, [], [], seconds=1, max_candidates=27
        )
        self.assertIsNone(result)
        self.assertLessEqual(proof["candidates"], 27)
        for limit in (-1, 101, True):
            with self.assertRaises(ValueError):
                repair(source, frames, [], [], seconds=1, max_candidates=limit)

    def test_joint_levels_and_actual_source_reader_bind_the_repaired_surface(self):
        surface, plans, frames = strip()
        result, levels = fit_rounded_levels(surface, frames, plans, HeightBudget())
        self.assertIsNotNone(result.mesh)
        for group, height in zip(
            level_groups(surface.vertices[:, :2], plans), levels, strict=True
        ):
            np.testing.assert_array_equal(
                result.mesh.vertices[group, 2], np.full(len(group), height)
            )
        sources, rows, recipe, state, empty = crossing_example()
        recipe["junction_height_mode"] = "rounded_joint"
        data, _network, planning = build(
            rows, sources, empty, lambda xy: 100 + xy @ [0.01, 0.02], recipe=recipe
        )
        audit(state, recipe, data, rows, planning, empty, sources=sources)
        changed = dict(data, joint_level_heights_m=data["joint_level_heights_m"] + 1)
        with self.assertRaises(ValueError):
            audit(state, recipe, changed, rows, planning, empty, sources=sources)


if __name__ == "__main__":
    unittest.main()

"""Closed contour/height support and bound pruning, independent of render pixels."""

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from parent_surface_inputs import ParentInputs, open_parent
from source_support_function import compile_support, sample_support, support_ranges


def shape(**changes):
    return {
        "ring": [[0, 0], [2, 0], [2, 2], [0, 2]],
        "holes": [],
        "roof_kind": 0,
        "rise_m": 0,
        "seat_m": 0,
        "foot_m": 0,
        "eaves_m": 2,
        "sole_m": 0,
    } | changes


def surface(shapes, *, origin=(0, 0, 0), frame=None):
    frame = np.eye(3) if frame is None else np.asarray(frame)
    return {
        "origin": list(origin),
        "east": frame[0],
        "north": frame[1],
        "up": frame[2],
        "shapes": shapes,
    }


def compile_surfaces(surfaces):
    frames = [
        [s[k] for k in ("origin", "east", "north", "up")]
        for s in surfaces
        if s is not None
    ]
    parent = SimpleNamespace(
        plans={"summaries": [{"supported": True, "frame": f} for f in frames]},
        iter_forms=lambda _: iter([(1, np.arange(len(surfaces)), surfaces)]),
    )
    return compile_support(parent, {1})


def values(compiled, points, **kwargs):
    support, basis, anchor, _ = compiled
    return sample_support(support, np.asarray(points) @ basis.T - anchor, **kwargs)


class SupportFunctionTest(unittest.TestCase):
    def test_closed_box_sign_and_known_face_distances(self):
        compiled = compile_surfaces([surface([shape()])])
        actual, _ = values(
            compiled, [[1, 1, 1], [1, 1, 3], [1, 1, -1], [3, 1, 1], [0, 1, 1]]
        )
        np.testing.assert_allclose(actual, [-1, 1, 1, 1, 0], atol=1e-13)

    def test_corner_magnitude_is_a_bound_not_euclidean_distance(self):
        compiled = compile_surfaces([surface([shape()])])
        actual, _ = values(compiled, [[3, 1, 3]])
        self.assertAlmostEqual(actual[0], 1, places=12)
        self.assertLess(actual[0], np.sqrt(2))

    def test_two_centimetre_gap_remains_open(self):
        compiled = compile_surfaces(
            [surface([shape()]), surface([shape()], origin=(2.02, 0, 0))]
        )
        actual, _ = values(compiled, [[1, 1, 1], [2.01, 1, 1], [3, 1, 1]])
        self.assertLess(actual[0], 0)
        self.assertGreater(actual[1], 0)
        self.assertLess(actual[2], 0)

    def test_courtyard_remains_outside_at_every_height(self):
        compiled = compile_surfaces(
            [
                surface(
                    [
                        shape(
                            ring=[[0, 0], [4, 0], [4, 4], [0, 4]],
                            holes=[[[1, 1], [3, 1], [3, 3], [1, 3]]],
                        )
                    ]
                )
            ]
        )
        actual, _ = values(compiled, [[2, 2, z] for z in [-1, 0, 1, 2, 3]])
        self.assertTrue((actual > 0).all())
        occupied, _ = values(compiled, [[0.5, 2, 1]])
        self.assertLess(occupied[0], 0)

    def test_gable_height_stays_coupled_to_its_contour(self):
        compiled = compile_surfaces(
            [
                surface(
                    [
                        shape(
                            ring=[[-1, -1], [1, -1], [1, 1], [-1, 1]],
                            roof_kind=1,
                            rise_m=1,
                            centre=[0, 0],
                            axis_u=[1, 0],
                            half_v_m=1,
                        )
                    ]
                )
            ]
        )
        actual, _ = values(
            compiled, [[0, 0, 2.9], [0, 0.5, 2.7], [0, 0.5, 2.4], [0, 1.1, 1]]
        )
        self.assertLessEqual(actual[0], 0)
        self.assertGreater(actual[1], 0)
        self.assertLess(actual[2], 0)
        self.assertGreater(actual[3], 0)

    def test_shared_csg_seam_is_an_interior_zero_not_a_surface_contract(self):
        compiled = compile_surfaces(
            [surface([shape()]), surface([shape()], origin=(2, 0, 0))]
        )
        actual, _ = values(compiled, [[2, 1, 1]])
        self.assertAlmostEqual(actual[0], 0)

    def test_affine_source_frame_and_world_lipschitz_bound(self):
        frame = np.array([[2, 0, 0], [0, 1, 0], [0.25, 0, 1.5]])
        compiled = compile_surfaces(
            [surface([shape()], origin=(10, -4, 2), frame=frame)]
        )
        rng = np.random.default_rng(71)
        local = rng.uniform(-1, 3, (512, 3))
        points = local @ frame + [10, -4, 2]
        actual, _ = values(compiled, points)
        expected = ((local >= 0) & (local <= 2)).all(1)
        np.testing.assert_array_equal(actual <= 0, expected)
        self.assertTrue(
            (
                np.abs(actual[1:] - actual[:-1])
                <= np.linalg.norm(points[1:] - points[:-1], axis=1) + 1e-12
            ).all()
        )

    def test_bound_pruning_matches_exhaustive_contour_evaluation(self):
        compiled = compile_surfaces(
            [surface([shape()]), surface([shape()], origin=(30, 0, 0))]
        )
        points = np.random.default_rng(6).uniform(-2, 5, (256, 3))
        actual, work = values(compiled, points)
        expected, exhaustive = values(compiled, points, prune=False)
        np.testing.assert_allclose(actual, expected, atol=1e-13, rtol=1e-13)
        self.assertLess(
            work["exact_contour_distance_queries"],
            exhaustive["exact_contour_distance_queries"],
        )

    def test_unsupported_sibling_keeps_the_entire_source_out(self):
        compiled = compile_surfaces(
            [surface([shape(), shape(roof_kind=6)]), surface([shape()])]
        )
        self.assertEqual(compiled[0].source_ids.tolist(), [1])
        self.assertEqual([row["source"] for row in compiled[3]], [0])

    def test_invalid_or_unbounded_point_packets_fail_before_geometry_queries(self):
        support = compile_surfaces([surface([shape()])])[0]
        for points in [np.array([[np.nan, 0, 0]]), np.zeros((2, 2))]:
            with self.assertRaises(ValueError):
                sample_support(support, points)
        with patch(
            "source_support_function.shapely.points",
            side_effect=AssertionError("no geometry queries"),
        ):
            support.domains = support.domains * 1024
            with self.assertRaises(ValueError):
                sample_support(support, np.zeros((16385, 3)))


class SupportRangeTest(unittest.TestCase):
    def test_adaptive_control_keeps_an_unsampled_thin_source_unresolved(self):
        from probe_source_support import adaptive_frontier

        thin = shape(ring=[[0.01, 0.01], [0.02, 0.01], [0.02, 0.02], [0.01, 0.02]])
        support = compile_surfaces([surface([thin])])[0]
        root = np.array([[0.0, 0.0, 0.0], [1.0, 1.0, 1.0]])
        with TemporaryDirectory(prefix="outshine-tests.source-support.") as directory:
            output = Path(directory)
            adaptive_frontier(support, root, output)
            with np.load(output / "support-frontier.npz", allow_pickle=False) as data:
                boxes, states = data["bounds"], data["states"]
                volume = np.prod(boxes[:, 1] - boxes[:, 0], axis=1).sum()
                self.assertAlmostEqual(volume, 1)
                point = np.array([0.015, 0.015, 0.51])
                covered = ((point >= boxes[:, 0]) & (point <= boxes[:, 1])).all(axis=1)
                self.assertEqual(covered.sum(), 1)
                self.assertEqual(states[covered][0], 0)

    def test_whole_boxes_inside_outside_and_crossing_the_surface(self):
        support, basis, anchor, _ = compile_surfaces([surface([shape()])])
        np.testing.assert_array_equal(basis, np.eye(3))
        np.testing.assert_array_equal(anchor, [0, 0, 0])
        bounds = np.array(
            [[[0.9] * 3, [1.1] * 3], [[3] * 3, [3.1] * 3], [[1.9] * 3, [2.1] * 3]]
        )
        ranges, _ = support_ranges(support, bounds)
        self.assertLess(ranges[0, 1], 0)
        self.assertGreater(ranges[1, 0], 0)
        self.assertLess(ranges[2, 0], 0)
        self.assertGreater(ranges[2, 1], 0)

    def test_an_empty_centre_does_not_erase_a_thin_feature_in_the_box(self):
        thin = shape(ring=[[0.01, 0.01], [0.02, 0.01], [0.02, 0.02], [0.01, 0.02]])
        support = compile_surfaces([surface([thin])])[0]
        value, _ = sample_support(support, [[0.5, 0.5, 0.5]])
        self.assertGreater(value[0], 0)
        ranges, _ = support_ranges(support, [[[0, 0, 0], [1, 1, 1]]])
        self.assertLess(ranges[0, 0], 0)
        self.assertGreater(ranges[0, 1], 0)

    def test_lipschitz_ranges_enclose_independent_interior_samples(self):
        support = compile_surfaces([surface([shape()])])[0]
        rng = np.random.default_rng(61)
        low = rng.uniform(-2, 4, (64, 3))
        high = low + rng.uniform(0, 2, (64, 3))
        ranges, _ = support_ranges(support, np.stack((low, high), axis=1))
        points = low[:, None] + rng.uniform(0, 1, (64, 16, 3)) * (high - low)[:, None]
        values, _ = sample_support(support, points.reshape(-1, 3))
        values = values.reshape(64, 16)
        self.assertTrue((values >= ranges[:, :1] - 1e-12).all())
        self.assertTrue((values <= ranges[:, 1:] + 1e-12).all())


class OriginalSupportTest(unittest.TestCase):
    def test_original_source_partition_without_shell_interpretation(self):
        path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view/original-inputs.tar.gz"
        )
        original_member = ParentInputs.member

        def member(parent, name):
            self.assertIn(name, {parent.metadata["index"], parent.forms[33]})
            return original_member(parent, name)

        with patch.object(ParentInputs, "member", member), open_parent(path) as parent:
            support, _, _, needs = compile_support(parent, {33})
            unknown = [row["source"] for row in needs]
            np.testing.assert_array_equal(
                np.sort(np.r_[support.source_ids, unknown]),
                np.flatnonzero(parent.cells == 33),
            )
            self.assertEqual(len(support.source_ids), 575)
            self.assertEqual(len(support.domains), 1472)
            self.assertEqual(len(unknown), 1)
            self.assertEqual(parent.form_reads, 1)

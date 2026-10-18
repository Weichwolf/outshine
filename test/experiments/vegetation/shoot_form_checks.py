"""Rigid lamina roll, original inventory/roots and independent curve/rotation controls."""

import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from compact_crown import CompactCrown
from outshine_ref.vegetation.curve_frame import arc_frame
from scipy.spatial.transform import Rotation
from shoot_form import ShootForm
from spatial_source_checks import fixture


def setup_form(**traits):
    source = fixture()
    base = CompactCrown.build(source)
    return source, base, ShootForm.build(source, base, **traits)


class ShootFormChecks(unittest.TestCase):
    def test_zero_controls_reproduce_every_original_bit(self):
        _, base, form = setup_form()
        mask = np.ones(len(base.data["hosts"]), bool)
        expected = base.evaluate(mask, max_leaves=100000, anchors=True)
        actual = form.evaluate(mask, max_leaves=100000, anchors=True)
        for key in actual:
            self.assertEqual(actual[key].tobytes(), expected[key].tobytes())

    def test_roll_keeps_stiel_axis_and_roots_and_matches_independent_rotation(self):
        source, base, form = setup_form(leaf_roll_spread=0.85, host_roll_spread=0.3)
        mask = np.ones(len(base.data["hosts"]), bool)
        original = base.evaluate(mask, max_leaves=100000, anchors=True)
        actual = form.evaluate(mask, max_leaves=100000, anchors=True)
        np.testing.assert_array_equal(actual["origins"], original["origins"])
        np.testing.assert_array_equal(actual["nodes"], original["nodes"])
        np.testing.assert_array_equal(
            actual["frames"][:, :, 1], original["frames"][:, :, 1]
        )
        for i, frame in enumerate(original["frames"]):
            rotation = Rotation.from_rotvec(frame[:, 1] * form.roll[i]).as_matrix()
            np.testing.assert_allclose(
                actual["frames"][i], rotation @ frame, atol=8e-16
            )
            petiole = source.crown.petiole_m
            root = actual["origins"][i] - petiole * actual["frames"][i, :, 1]
            np.testing.assert_allclose(root, actual["nodes"][i], atol=8e-16)

    def test_scale_anchors_match_independent_new_length_circular_arcs(self):
        source, base, form = setup_form(scale_spread=0.55)
        mask = np.ones(len(base.data["hosts"]), bool)
        result = form.evaluate(mask, max_leaves=100000, anchors=True)
        for i, node in enumerate(source.crown.hosts):
            lo, hi = source.crown.offsets[i : i + 2]
            count = int(hi - lo)
            length = source.crown.length_m * form.scale[i]
            internode = length * 0.9 / max(0.5, count - 0.5)
            t = 0.95 - np.arange(count) * internode / length
            centre = arc_frame(length, source.crown.bend_rad, 0, t)[0]
            origin, frame = source.crown.stem(int(node))
            np.testing.assert_allclose(
                result["nodes"][lo:hi], centre @ frame.T + origin, atol=2e-15
            )
        self.assertAlmostEqual(float(form.scale.mean()), 1.0, places=15)
        self.assertGreater(float(np.ptp(form.scale)), 0.2)

    def test_frames_and_triangle_area_survive_combined_form(self):
        source, base, form = setup_form(scale_spread=0.55, leaf_roll_spread=0.85)
        result = form.evaluate(
            np.ones(len(base.data["hosts"]), bool), max_leaves=100000
        )
        frames = result["frames"]
        np.testing.assert_allclose(np.linalg.det(frames), 1, atol=8e-16)
        np.testing.assert_allclose(
            frames.swapaxes(1, 2) @ frames,
            np.broadcast_to(np.eye(3), frames.shape),
            atol=1e-15,
        )
        np.testing.assert_array_equal(result["variants"], source.crown.variants)
        for i in range(0, len(frames), 7):
            mesh = source.blades[int(result["variants"][i])].mesh(8, 1)
            before = mesh["vertices"][mesh["faces"]]
            after = (mesh["vertices"] @ frames[i].T + result["origins"][i])[
                mesh["faces"]
            ]
            area = lambda x: (
                np.linalg.norm(np.cross(x[:, 1] - x[:, 0], x[:, 2] - x[:, 0]), axis=1)
                / 2
            )
            np.testing.assert_allclose(
                area(after), area(before), rtol=1e-11, atol=1e-17
            )

    def test_selected_results_equal_full_rows_and_capacity_refuses_first(self):
        _, base, form = setup_form(scale_spread=0.55, leaf_roll_spread=0.85)
        mask = np.arange(len(base.data["hosts"])) % 2 == 0
        full = form.evaluate(np.ones(len(mask), bool), max_leaves=100000)
        selected = form.evaluate(mask, max_leaves=100000)
        rows = np.repeat(mask, np.diff(base.data["offsets"]).astype(int))
        for key, value in selected.items():
            np.testing.assert_array_equal(value, full[key][rows])
        with patch("compact_crown_kernel.local_poses", side_effect=AssertionError):
            self.assertIsNone(form.evaluate(mask, max_leaves=0))
            empty = form.evaluate(np.zeros(len(mask), bool), max_leaves=0)
        self.assertEqual(empty["frames"].shape, (0, 3, 3))

    def test_finished_coefficients_replay_without_source_randomness_or_build(self):
        _, base, form = setup_form(scale_spread=0.55, leaf_roll_spread=0.85)
        with tempfile.TemporaryDirectory(
            prefix="outshine-shoot-form-", dir="/tmp"
        ) as d:
            path = Path(d) / "form.npz"
            form.save(path)
            with (
                patch.object(ShootForm, "build", side_effect=AssertionError),
                patch("numpy.random.Generator", side_effect=AssertionError),
                patch("crown_shoots.CrownShoots.group", side_effect=AssertionError),
            ):
                loaded = ShootForm.load(path, expected_source=base.identity)
                actual = loaded.evaluate(
                    np.ones(len(base.data["hosts"]), bool), max_leaves=100000
                )
            self.assertEqual(loaded.identity, form.identity)
            expected = form.evaluate(
                np.ones(len(base.data["hosts"]), bool), max_leaves=100000
            )
            for key in actual:
                self.assertEqual(actual[key].tobytes(), expected[key].tobytes())

    def test_form_identity_binds_recipe_and_parameters_and_old_source_is_preserved(
        self,
    ):
        source, base, form = setup_form(scale_spread=0.55)
        changed = ShootForm.build(source, base, scale_spread=0.56)
        self.assertNotEqual(form.identity, changed.identity)
        self.assertEqual(source.identity, base.identity)
        for value in (form.scale, form.roll, form.lower, form.upper):
            self.assertFalse(value.flags.writeable)


if __name__ == "__main__":
    unittest.main()

"""Actual capture integrity and exact alternatives, without GPU tolerance oracles."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from terrain_material_capture import load
from terrain_material_observations import evaluate_capture, mean

FIXTURE = Path(__file__).with_name("fixtures") / "terrain-material-fragments"


class NativeMaterialObservations(unittest.TestCase):
    def test_original_words_frame_scope_and_unnormalized_surface_are_preserved(self):
        for name in ("jura", "koerbersee"):
            capture = load(FIXTURE / (name + ".json"))
            self.assertEqual(len(capture.points), 64)
            self.assertEqual(capture.metadata["frame_index"], 59)
            self.assertEqual(
                capture.metadata["product_commit"],
                "f24ae481f301e70a9b3e24a3a55763048ab61953",
            )
            self.assertTrue(capture.metadata["limitations"])
            self.assertTrue(np.any(capture.points[:, 52] < 0))
            length = np.linalg.norm(capture.points[:, 16:19], axis=-1)
            self.assertTrue(np.any(abs(length - 1) > 1e-3))
            for array in (capture.points, capture.palette_words, capture.palette):
                with self.assertRaises(ValueError):
                    array.setflags(write=True)
            with self.assertRaises(TypeError):
                capture.metadata["frame_index"] = 0

    def test_all_actual_weighted_material_channels_are_identical_between_methods(self):
        for name in ("jura", "koerbersee"):
            capture = load(FIXTURE / (name + ".json"))
            direct, work = evaluate_capture(capture)
            lookup, prepared = evaluate_capture(capture, prepared=True)
            np.testing.assert_array_equal(direct, lookup)
            self.assertEqual(direct.shape, (64, 6))
            self.assertTrue(np.isfinite(direct).all())
            self.assertEqual(
                work["active_rock_octaves"], prepared["active_rock_octaves"]
            )
            self.assertEqual(
                work["rock_octaves_skipped_before_noise"],
                prepared["rock_octaves_skipped_before_noise"],
            )
            self.assertGreater(work["rock_octaves_skipped_before_noise"], 0)

    def test_negative_material_selection_uses_optical_fallback_not_unmapped_mean(self):
        capture = load(FIXTURE / "jura.json")
        self.assertEqual(int(capture.points[0, 52]), -1)
        rows = int(capture.palette_words[0])
        fallback = mean(capture.palette, rows, -1)
        unmapped = mean(
            capture.palette, rows, capture.metadata["unmapped_material_row_i32"]
        )
        self.assertFalse(np.array_equal(fallback, unmapped))
        result, _ = evaluate_capture(capture)
        np.testing.assert_array_equal(result[0], np.r_[fallback, 0, 0])

    def test_zero_mixture_support_avoids_work_without_changing_any_channel(self):
        for name in ("jura", "koerbersee"):
            capture = load(FIXTURE / (name + ".json"))
            baseline, before = evaluate_capture(capture)
            selected, after = evaluate_capture(capture, prune_support=True)
            cached, cached_work = evaluate_capture(
                capture, prepared=True, prune_support=True
            )
            self.assertEqual(baseline.tobytes(), selected.tobytes())
            self.assertEqual(baseline.tobytes(), cached.tobytes())
            self.assertLess(after["rock_calls"], before["rock_calls"])
            self.assertLess(
                after["first_substrate_calls"], before["first_substrate_calls"]
            )
            self.assertLess(
                after["analytic_hash_words_queried"],
                before["analytic_hash_words_queried"],
            )
            self.assertEqual(cached_work["analytic_hash_words_queried"], 0)

    def test_corrupt_palette_record_precision_hash_and_layout_fail(self):
        source = json.loads((FIXTURE / "jura.json").read_bytes())
        for mode in (
            "palette",
            "precision",
            "record_hash",
            "layout",
            "duplicate",
            "negative_weight",
        ):
            payload = json.loads(json.dumps(source))
            if mode == "palette":
                payload["palette_words_u32"][17] ^= 1
            elif mode == "precision":
                payload["points"][0][4] += 1e-8
            elif mode == "record_hash":
                payload["points"][0][54] ^= 1
            elif mode == "layout":
                payload["record_layout"][1]["name"] = "unbound_frame"
            elif mode == "duplicate":
                payload["points"][1][:2] = payload["points"][0][:2]
            else:
                payload["points"][0][28] = -1
            with tempfile.TemporaryDirectory(
                prefix="outshine-material-observations-"
            ) as folder:
                path = Path(folder) / "input.json"
                path.write_text(json.dumps(payload))
                with self.subTest(mode=mode), self.assertRaises(ValueError):
                    load(path)


if __name__ == "__main__":
    unittest.main()

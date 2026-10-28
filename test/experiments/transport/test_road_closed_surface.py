"""Independent slab geometry, original normal offsets and pre-build refusals."""

import json
import lzma
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from road_closed_checks import observed_body_error
from road_closed_surface import normal_departure, prepare_closed
from road_surface_model import Curve
from test_road_surface_lod import recipe


def straight(slope=0.0, bank=0.0, thickness=0.3, shoulder=0.0):
    source = recipe()
    source["form"] = "closed_shell"
    source["section_m"] = [2.0, shoulder, thickness]
    for pose in (*source["seams"], source["intervals"][0]["middle"]):
        pose[3], pose[7], pose[8] = slope * pose[0], slope, 0.0
        pose[9], pose[10] = bank, 0.0
    return source


class ClosedRoadGeometry(unittest.TestCase):
    def test_known_horizontal_box_has_exact_topology_volume_and_payload(self):
        body = prepare_closed(straight(), 0.25, max_payload_bytes=284)
        self.assertEqual(body["positions_enh_m"].shape, (8, 3))
        self.assertEqual(body["indices"].shape, (12, 3))
        self.assertEqual(body["payload_bytes"], 284)
        self.assertEqual(body["quality"]["closed_oriented_edges"], 18)
        self.assertAlmostEqual(body["quality"]["signed_volume_m3"], 12, places=5)
        np.testing.assert_array_equal(body["upper_source_vertex_ids"], [1, 2, 5, 6])
        self.assertLess(observed_body_error(straight(), body), 1e-7)

    def test_positive_shoulders_keep_all_original_cross_section_bands(self):
        source = straight(shoulder=0.5)
        body = prepare_closed(source, 0.25, max_payload_bytes=604)
        self.assertEqual(body["positions_enh_m"].shape, (16, 3))
        self.assertEqual(body["indices"].shape, (28, 3))
        self.assertEqual(body["payload_bytes"], 604)
        self.assertAlmostEqual(body["quality"]["signed_volume_m3"], 15, places=5)

    def test_complete_bound_includes_the_native_normal_offset_difference(self):
        source = straight(slope=0.5, bank=0.04)
        for method in ("adaptive", "uniform"):
            body = prepare_closed(source, 0.25, max_payload_bytes=1024, method=method)
            self.assertGreater(body["normal_offset_departure_bound_m"], 0.1)
            self.assertGreater(
                body["maximum_stored_bound_m"], body["upper_maximum_stored_bound_m"]
            )
            self.assertLessEqual(body["maximum_stored_bound_m"], 0.25)
            self.assertLessEqual(
                observed_body_error(source, body), body["maximum_stored_bound_m"] + 1e-7
            )

    def test_near_vertical_normal_bound_does_not_cancel_to_zero(self):
        source = straight(slope=1e-12)
        bound, _ = normal_departure(Curve(source))
        self.assertGreater(bound, 0)
        self.assertLess(abs(bound / (0.3e-12) - 1), 1e-12)

    def test_bounded_observation_batches_cover_the_same_whole_body(self):
        source = straight(slope=0.5, bank=0.04)
        body = prepare_closed(source, 0.25, max_payload_bytes=1024)
        a = observed_body_error(source, body, batch_faces=1)
        b = observed_body_error(source, body, batch_faces=128)
        self.assertAlmostEqual(a, b, places=12)
        with self.assertRaises(ValueError):
            observed_body_error(source, body, batch_faces=0)

    def test_steep_offset_and_payload_fail_before_render_subdivision(self):
        with patch(
            "road_closed_surface.prepare", side_effect=AssertionError("render work")
        ):
            with self.assertRaisesRegex(ValueError, "Normal-offset"):
                prepare_closed(straight(slope=2), 0.25, max_payload_bytes=1024)
            with self.assertRaisesRegex(ValueError, "payload capacity"):
                prepare_closed(straight(), 0.25, max_payload_bytes=283)

    def test_unrepresentable_thickness_and_folded_input_cannot_emit_a_body(self):
        source = straight(thickness=0.01)
        for pose in (*source["seams"], source["intervals"][0]["middle"]):
            pose[3] = 1e6
        with self.assertRaisesRegex(ValueError, "thickness"):
            prepare_closed(source, 0.25, max_payload_bytes=1024)
        folded = recipe(curvature=0.6)
        folded["form"] = "closed_shell"
        with self.assertRaisesRegex(ValueError, "Folded"):
            prepare_closed(folded, 0.25, max_payload_bytes=1024)

    def test_original_recipe_is_unchanged_and_fine_fields_are_never_read(self):
        source = straight()
        before = json.dumps(source, sort_keys=True)

        class MetadataOnly(dict):
            def __getitem__(self, key):
                if key not in ("form", "range_m", "section_m", "seams", "intervals"):
                    raise AssertionError("Fine geometry requested")
                return super().__getitem__(key)

        guarded = MetadataOnly(
            source, upper_positions_m=object(), upper_normals=object()
        )
        body = prepare_closed(guarded, 0.25, max_payload_bytes=1024)
        self.assertGreater(len(body["indices"]), 0)
        self.assertEqual(json.dumps(source, sort_keys=True), before)

    def test_both_methods_match_for_a_known_plane_and_invalid_requests_reject(self):
        outputs = [
            prepare_closed(straight(), 0.25, max_payload_bytes=1024, method=m)
            for m in ("adaptive", "uniform")
        ]
        for key in ("positions_enh_m", "indices", "face_roles"):
            self.assertEqual(outputs[0][key].tobytes(), outputs[1][key].tobytes())
        for options in (
            {"max_payload_bytes": True},
            {"max_payload_bytes": 0},
            {"max_payload_bytes": 1024, "max_stations": True},
            {"max_payload_bytes": 1024, "method": "unknown"},
        ):
            with self.assertRaises(ValueError):
                prepare_closed(straight(), 0.25, **options)
        source = straight()
        source["form"] = "surface"
        with self.assertRaisesRegex(ValueError, "closed-shell"):
            prepare_closed(source, 0.25, max_payload_bytes=1024)


class OriginalClosedRecipes(unittest.TestCase):
    def test_all_original_closed_recipes_have_explicit_same_budget_results(self):
        root = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/transport/road_curve_requests"
        )
        manifest = json.loads((root / "manifest.json").read_text())
        counts = {method: [0, 0] for method in ("adaptive", "uniform")}
        with lzma.open(root / manifest["archive"], "rt") as stream:
            for line in stream:
                row = json.loads(line)
                if row["form"] != "closed_shell":
                    continue
                for method, count in counts.items():
                    try:
                        body = prepare_closed(
                            row, 0.25, max_payload_bytes=8 * 1024 * 1024, method=method
                        )
                    except ValueError as error:
                        self.assertIn("Normal-offset", str(error))
                        count[1] += 1
                    else:
                        self.assertLessEqual(body["maximum_stored_bound_m"], 0.25)
                        self.assertGreater(body["quality"]["signed_volume_m3"], 0)
                        count[0] += 1
        for accepted, rejected in counts.values():
            self.assertEqual(accepted + rejected, 186)
            self.assertGreaterEqual(accepted, 181)


if __name__ == "__main__":
    unittest.main()

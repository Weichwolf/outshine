"""Actual byte-owned attributes and independent declared world-axis pose controls."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from check_building_capture_inputs import VERTEX, expanded_stream
from full_view_reference import load, views

ROOT = Path(__file__).resolve().parents[2] / "outshine/data/render/building-full-view"


class FullViewReference(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source, cls.document, cls.context, cls.receipt = load(ROOT / "cell.json.gz")

    def test_adapted_whole_mesh_has_exact_original_vertex_inputs(self):
        source = self.source
        vertices = [
            VERTEX.pack(*p, *uv, int(n), *c, *m)
            for p, uv, n, c, m in zip(
                source["positions"],
                source["uv"],
                source["normal"],
                source["colour"],
                source["metres"],
                strict=True,
            )
        ]
        raw = b"".join(vertices[i] for triangle in source["indices"] for i in triangle)
        geometry = self.document["results"][0]["geometry"]
        self.assertEqual(
            raw, expanded_stream(geometry, "walls") + expanded_stream(geometry, "roofs")
        )
        self.assertEqual(len(source["indices"]), 14169)
        self.assertEqual(len(np.unique(source["binding"][:, 1])), 576)
        self.assertTrue((source["binding"][:, 3] == -1).all())
        self.assertTrue(self.receipt["opaque_assumption"])

    def test_camera_sequence_matches_finish_and_quarter_turn_about_world_up(self):
        poses = views(self.document, self.context)
        self.assertEqual(len(poses), 120)
        frame = self.context["scene_frame"]
        basis = np.column_stack([frame[k] for k in ("x_ecef", "y_ecef", "z_ecef")])
        origin = np.array(frame["origin_ecef"]) - self.document["anchor_ecef"]
        for name in ("static", "moving"):
            finish = (
                np.array(self.context["motion"][name]["finish"]["world_from_camera"])
                .reshape(4, 4)
                .T
            )
            eye, camera, focal, width, height = poses[name + "-060"]
            np.testing.assert_allclose(
                eye, origin + basis @ finish[:3, 3], rtol=0, atol=1e-12
            )
            expected = np.stack(
                (basis @ finish[:3, 0], basis @ finish[:3, 1], -basis @ finish[:3, 2])
            )
            np.testing.assert_allclose(camera, expected, rtol=0, atol=1e-14)
            self.assertEqual(
                (focal, width, height), (self.document["view"]["focal_px"], 1280, 720)
            )
        np.testing.assert_allclose(
            poses["moving-060"][0] - poses["static-060"][0],
            6 * basis[:, 0],
            rtol=0,
            atol=1e-12,
        )
        start = (
            np.array(self.context["motion"]["static"]["start"]["world_from_camera"])
            .reshape(4, 4)
            .T
        )
        right = start[:3, 0]
        np.testing.assert_allclose(
            poses["static-015"][1][0],
            basis @ [-right[2], right[1], right[0]],
            rtol=0,
            atol=1e-14,
        )
        actual = self.document["anchor_ecef"] + origin + basis @ start[:3, 3]
        error = np.linalg.norm(actual - self.document["view"]["eye_ecef"])
        self.assertLess(
            error,
            2 * frame["camera_start_ecef_error_m"] + 2 * np.spacing(abs(actual)).max(),
        )

    def test_bad_manifest_and_unlisted_inputs_fail_before_geometry_consumption(self):
        with tempfile.TemporaryDirectory(prefix="outshine-tests.full-view-") as folder:
            root = Path(folder)
            manifest = json.loads((ROOT / "manifest.json").read_text())
            for name in manifest:
                (root / name).symlink_to(ROOT / name)
            manifest["cell.json.gz"]["sha256"] = "0" * 64
            (root / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "manifest mismatch"):
                load(root / "cell.json.gz")
            with self.assertRaisesRegex(ValueError, "belong"):
                load(root / "unlisted.json")


if __name__ == "__main__":
    unittest.main()

"""Flat source-to-mesh controls: no elevation dependency or geographic camera altitude."""

import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import mapbox_vector_tile
import numpy as np
from place_reference import PLACES, flat, read
from place_scene import prepare
from place_sources import run
from place_tiles import decode


class FlatPlaces(unittest.TestCase):
    def test_flat_cameras_need_no_ground_sample_and_retain_geographic_bearing(self):
        for name in PLACES:
            original, camera = read(name), flat(name)
            self.assertFalse(camera.samples_height)
            self.assertLessEqual(camera.view().eye[2], 80.0 + 1e-8)
            self.assertAlmostEqual(camera.view().eye[2], camera.declared_height_m)
            np.testing.assert_allclose(camera.basis, original.basis, atol=0)
            self.assertEqual(camera.scenario_sha256, original.scenario_sha256)

    def test_acquisition_requests_only_vectors_including_tokyo(self):
        def source(subject, _fetch):
            kind, tile, url = subject
            self.assertEqual(kind, "vector")
            return {"kind": kind, "tile": tile, "url": url}

        with tempfile.TemporaryDirectory(prefix="outshine-buildings-flat-") as folder:
            output = Path(folder) / "sources.json"
            with (
                patch("place_sources.acquire", side_effect=source),
                patch(
                    "place_sources.endpoint",
                    return_value=(
                        "https://example.test/{z}/{x}/{y}.pbf",
                        "control",
                        "a" * 64,
                    ),
                ),
            ):
                run(output, False, 1000.0)
            inputs = json.loads(output.read_text())
            self.assertEqual(len(inputs["places"]), 4)
            self.assertEqual(inputs["ground_mode"], "flat-no-dem")
            self.assertEqual(
                [p["kind"] for p in inputs["provider_definitions"]], ["vector"]
            )

    def test_actual_vector_decode_and_scene_keep_building_height_on_zero_plane(self):
        properties = {"render_height": 9.0, "render_min_height": 0.0}
        payload = mapbox_vector_tile.encode(
            {
                "name": "building",
                "features": [
                    {
                        "id": 7,
                        "properties": properties,
                        "geometry": "POLYGON((2000 2000,2040 2000,2040 2040,2000 2040,2000 2000))",
                    }
                ],
            },
            default_options={"y_coord_down": True},
        )
        with tempfile.TemporaryDirectory(prefix="outshine-buildings-flat-") as folder:
            path = Path(folder) / "source.pbf"
            path.write_bytes(payload)
            receipt = {
                "kind": "vector",
                "tile": [14, 8603, 5223],
                "url": "control",
                "path": str(path),
                "sha256": hashlib.sha256(payload).hexdigest(),
            }
            inputs = {
                "places": [
                    {
                        "camera": {"name": "Husum"},
                        "source_urls": ["control"],
                        "initial_source_radius_m": 3000.0,
                    }
                ],
                "receipts": [receipt],
            }
            with patch(
                "outshine_ref.places.Elevation.sample",
                side_effect=AssertionError("DEM used"),
            ):
                rows, _ = decode(flat("Husum"), receipt)
                mesh, scene = prepare("Husum", inputs)
            self.assertEqual(rows[0]["properties"], properties)
            self.assertEqual(rows[0]["placement_base_enu_m"], 0.0)
            self.assertIsNone(rows[0]["ground_asl_m"])
            self.assertEqual(scene["ground_mode"], "flat-no-dem")
            self.assertEqual(float(mesh["vertices"][:, 2].min()), 0.0)
            self.assertEqual(float(mesh["vertices"][:, 2].max()), 9.0)
            inputs["receipts"] = [receipt | {"kind": "elevation"}]
            with self.assertRaisesRegex(ValueError, "no DEM"):
                prepare("Husum", inputs)


if __name__ == "__main__":
    unittest.main()

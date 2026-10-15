"""Independent metric-frame and camera-height counterexamples for transport."""

import hashlib
import io
import json
import math
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

import numpy as np
from outshine_ref.places import read_camera
from PIL import Image
from transport_place_inputs import (
    GIRTH_M,
    ROOT,
    place_camera,
    plan_geography,
    plan_vertices,
)


class PlaceInputs(unittest.TestCase):
    def camera(self):
        camera = read_camera(ROOT / "src/assets/places/Wien.scenario")
        return replace(
            camera,
            latitude_deg=0.0,
            longitude_deg=0.0,
            origin_latitude_deg=0.0,
            origin_longitude_deg=0.0,
        )

    def test_quarter_equator_has_independent_ecef_answer(self):
        vertices = np.array([[0.0, 0.0, 123.0], [GIRTH_M / 4, 0.0, 0.0]])
        original = vertices.copy()
        result = plan_vertices(self.camera(), [0.0, 0.0], vertices)
        np.testing.assert_allclose(
            result, [[0.0, 0.0, 123.0], [6378137.0, 0.0, -6378137.0]], atol=2e-9, rtol=0
        )
        np.testing.assert_array_equal(vertices, original)

    def test_ellipsoid_latitude_is_not_spherical_mercator_length(self):
        north = GIRTH_M / (2 * math.pi) * math.asinh(1.0)
        result = plan_vertices(self.camera(), [0.0, 0.0], [[0.0, north, 0.0]])
        np.testing.assert_allclose(
            result, [[0.0, 4487348.408865919, -1860546.1211510682]], atol=3e-9, rtol=0
        )

    def test_dateline_wrap_and_historical_origin_round_trip(self):
        for origin in [
            [48.21, 16.37],
            [35.66, 139.7],
            [40.78, -73.965],
            [47.238, 9.599],
        ]:
            latitude, longitude = plan_geography(origin, np.zeros((1, 2)))
            np.testing.assert_allclose(
                [latitude[0], longitude[0]], origin, atol=6e-14, rtol=0
            )
        latitude, longitude = plan_geography([0.0, 180.0], [[GIRTH_M / 360, 0.0]])
        np.testing.assert_allclose(
            [latitude[0], longitude[0]], [0.0, -179.0], atol=1e-12
        )

    def test_coincident_road_soil_contacts_remain_bit_identical(self):
        vertices = np.array([[20.0, 30.0, 140.0], [20.0, 30.0, 140.0], [1.0, 2.0, 3.0]])
        result = plan_vertices(self.camera(), [48.21, 16.37], vertices)
        np.testing.assert_array_equal(result[0], result[1])

    def test_invalid_or_unlabelled_frame_is_rejected(self):
        for origin in [[90.0, 0.0], [0.0, 181.0], [0.0], [np.nan, 0.0]]:
            with self.assertRaises(ValueError):
                plan_geography(origin, [[0.0, 0.0]])
        with self.assertRaises(ValueError):
            plan_vertices(self.camera(), [0.0, 0.0], [[0.0, np.nan, 0.0]])

    def manifest(self, directory, name, receipts):
        camera = read_camera(ROOT / "src/assets/places" / (name + ".scenario"))
        path = directory / "elevation.json"
        path.write_text(
            json.dumps(
                {
                    "cases": {
                        name: {
                            "scenario_sha256": camera.scenario_sha256,
                            "receipts": receipts,
                        }
                    }
                }
            )
        )
        return path

    def test_relative_height_records_actual_parent_zoom(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.transport-places."
        ) as directory:
            directory = Path(directory)
            stream = io.BytesIO()
            Image.fromarray(np.full((2, 2, 3), [128, 17, 128], np.uint8)).save(
                stream, format="WEBP", lossless=True
            )
            data = stream.getvalue()
            path = directory / "parent.webp"
            path.write_bytes(data)
            receipt = {
                "kind": "elevation",
                "tile": [0, 0, 0],
                "path": str(path),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
            native, view, proof = place_camera(
                "Tokyo", self.manifest(directory, "Tokyo", [receipt])
            )
            self.assertEqual(proof["camera_dem_zoom"], 0)
            self.assertEqual(proof["camera_dem_height_m"], 17.5)
            np.testing.assert_allclose(view["eye"], [0.0, 0.0, 77.5], atol=2e-9, rtol=0)
            np.testing.assert_allclose(
                native.basis.T @ native.basis, np.eye(3), atol=1e-15
            )

    def test_relative_camera_needs_real_coverage_but_absolute_does_not(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.transport-places."
        ) as directory:
            directory = Path(directory)
            with self.assertRaises(ValueError):
                place_camera("Tokyo", self.manifest(directory, "Tokyo", []))
            _, view, proof = place_camera("Wien", self.manifest(directory, "Wien", []))
            self.assertIsNone(proof["camera_dem_height_m"])
            np.testing.assert_allclose(
                view["eye"], [0.0, 0.0, 250.0], atol=2e-9, rtol=0
            )

    def test_receipts_are_bound_to_camera_declaration(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.transport-places."
        ) as directory:
            path = self.manifest(Path(directory), "Wien", [])
            data = json.loads(path.read_text())
            data["cases"]["Wien"]["scenario_sha256"] = "changed"
            path.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                place_camera("Wien", path)


if __name__ == "__main__":
    unittest.main()

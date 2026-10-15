import hashlib
import importlib.util
import io
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.places import (
    Elevation,
    PlaceCamera,
    acquire,
    covering_tiles,
    decode_terrarium,
    decode_vector,
    read_camera,
    receipt_bytes,
    tile_geography,
)
from PIL import Image


def north_camera():
    return PlaceCamera(
        "control",
        0,
        0,
        0,
        0,
        0,
        False,
        0,
        0,
        90,
        (1600, 900),
        "2026-06-21T12:00:00Z",
        "",
        240000,
    )


def webp(values, *, mode="RGB", lossless=True):
    pixels = np.zeros((*np.shape(values), 4 if mode == "RGBA" else 3), dtype=np.uint8)
    encoded = np.asarray(values) + 32768
    pixels[..., 0] = np.floor(encoded / 256).astype(np.uint8)
    pixels[..., 1] = np.floor(encoded % 256).astype(np.uint8)
    pixels[..., 2] = np.rint((encoded % 1) * 256).astype(np.uint8)
    if mode == "RGBA":
        pixels[..., 3] = 128
    stream = io.BytesIO()
    Image.fromarray(pixels).save(stream, format="WEBP", lossless=lossless)
    return stream.getvalue()


def stored(root, values, tile=(0, 0, 0), name="dem"):
    data = webp(values)
    path = Path(root) / (name + ".webp")
    path.write_bytes(data)
    return {
        "kind": "elevation",
        "tile": list(tile),
        "path": str(path),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


class PlaceCameraTests(unittest.TestCase):
    def test_equatorial_height_and_curved_earth_section(self):
        camera = north_camera()
        angle = np.deg2rad(0.01)
        expected = (6378137 * np.sin(angle), 0, 6378137 * (np.cos(angle) - 1))
        np.testing.assert_allclose(camera.local(0, 0.01, 0), expected, atol=1e-9)
        np.testing.assert_allclose(camera.local(0, 0, 23), (0, 0, 23), atol=1e-9)

    def test_camera_handedness_and_independent_image_edges(self):
        camera = north_camera()
        np.testing.assert_allclose(-camera.basis[:, 2], (0, 1, 0), atol=1e-15)
        self.assertAlmostEqual(np.linalg.det(camera.basis), 1)
        point = ((0, 10, 0), (0, 10, 10), (160 / 9, 10, 0), (0, -1, 0))
        pixel, depth = camera.project(point)
        np.testing.assert_allclose(
            pixel[:3], ((800, 450), (800, 0), (1600, 450)), atol=3e-13
        )
        self.assertTrue(np.isnan(pixel[3]).all())
        self.assertLess(depth[3], 0)
        with self.assertRaises(ValueError):
            camera.project(point, profile=(1000, 1000))

    def test_relative_height_requires_actual_dem_and_nd_coordinates_broadcast(self):
        camera = replace(north_camera(), samples_height=True, declared_height_m=60)
        for absent in (None, np.nan, True, [12]):
            with self.assertRaises(ValueError):
                camera.eye(absent)
        np.testing.assert_allclose(camera.eye(35), (0, 0, 95), atol=1e-9)
        values = camera.local(np.zeros((2, 1)), np.zeros(3), np.arange(3))
        self.assertEqual(values.shape, (2, 3, 3))
        np.testing.assert_allclose(
            values[..., 2], np.tile(np.arange(3), (2, 1)), atol=1e-9
        )

    def test_scenario_retains_origin_time_lens_and_sight_without_a_place_list(self):
        data = b'<scenario><world lat="47" lon="10" sightM="240000"/><render widthPx="1280" heightPx="720"/><clock start="2026-09-07T10:40:00Z" live="no"/><views><view fovDeg="33.97"><at lat="47" lon="10" heightM="1772" samplesHeight="no" bearingDeg="240" pitchDeg="-0.5"/></view></views></scenario>'
        with tempfile.TemporaryDirectory(prefix="outshine-tests.places.") as root:
            path = Path(root) / "ArbitraryName.scenario"
            path.write_bytes(data)
            camera = read_camera(path)
            self.assertEqual(camera.name, "ArbitraryName")
            self.assertEqual(camera.scenario_sha256, hashlib.sha256(data).hexdigest())
            self.assertEqual(camera.vertical_fov_deg, 33.97)
            self.assertEqual(camera.sight_m, 240000)
            self.assertEqual(camera.time_utc, "2026-09-07T10:40:00Z")
            self.assertFalse(camera.samples_height)


class PlaceHeightTests(unittest.TestCase):
    def test_lossless_fractional_terrarium_and_owned_readonly_raster(self):
        expected = np.array(((0, 10.5), (20.25, 30)))
        decoded = decode_terrarium(webp(expected))
        np.testing.assert_array_equal(decoded, expected)
        with self.assertRaises(ValueError):
            decoded.setflags(write=True)

    def test_invalid_wire_container_alpha_lossy_and_earth_domain_refuse(self):
        valid = webp(((0, 10), (20, 30)))
        cases = (
            valid[:-1],
            valid + b"x",
            b"not webp",
            webp(((0, 1), (2, 3)), lossless=False),
            webp(((0, 1), (2, 3)), mode="RGBA"),
            webp(((10000, 0), (0, 0))),
        )
        for data in cases:
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                decode_terrarium(data)

    def test_bilinear_height_and_actual_finer_zoom_with_missing_child(self):
        with tempfile.TemporaryDirectory(prefix="outshine-tests.places.") as root:
            coarse = stored(root, ((0, 10), (20, 30)))
            fine = stored(root, np.full((2, 2), 42), (1, 1, 1), "fine")
            missing = {"kind": "elevation", "tile": [2, 2, 2], "missing": True}
            elevation = Elevation([coarse, fine, missing])
            height, zoom = elevation.sample_with_zoom((0, 0), (0, -90))
            np.testing.assert_allclose(height, (42, 12.5), atol=1e-14)
            np.testing.assert_array_equal(zoom, (1, 0))
            values = Elevation([coarse]).sample(
                np.array(((0,), (66.51326044311186,))), np.array((0, 90))
            )
            np.testing.assert_allclose(values, ((15, 17.5), (10, 12.5)), atol=2e-14)

    def test_absence_and_changed_source_never_become_zero(self):
        with self.assertRaises(ValueError):
            Elevation([]).sample(0, 0)
        with tempfile.TemporaryDirectory(prefix="outshine-tests.places.") as root:
            record = stored(root, ((0, 10), (20, 30)))
            Path(record["path"]).write_bytes(b"changed")
            with self.assertRaises(ValueError):
                Elevation([record]).sample(0, 0)


class PlaceSourceTests(unittest.TestCase):
    def test_retained_bytes_no_automatic_network_refresh_and_pinned_hash(self):
        subject = "vector", (1, 1, 1), "https://example.test/1/1/1.pbf"
        with tempfile.TemporaryDirectory(prefix="outshine-tests.places.") as root:
            with patch(
                "outshine_ref.places.sources.subprocess.check_output",
                return_value=b"source\n200",
            ) as network:
                first = acquire(subject, root, fetch=True)
                self.assertEqual(network.call_count, 1)
            with patch(
                "outshine_ref.places.sources.subprocess.check_output"
            ) as network:
                second = acquire(subject, root, fetch=True)
                network.assert_not_called()
            self.assertEqual(first["sha256"], second["sha256"])
            self.assertEqual(receipt_bytes(second), b"source")
            with self.assertRaises(ValueError):
                acquire(subject, root, max_bytes=2)

    def test_explicit_404_and_failure_budget_leave_no_payload(self):
        with tempfile.TemporaryDirectory(prefix="outshine-tests.places.") as root:
            with patch(
                "outshine_ref.places.sources.subprocess.check_output",
                return_value=b"absent\n404",
            ):
                result = acquire(
                    ("elevation", (0, 0, 0), "https://example.test/dem"),
                    root,
                    fetch=True,
                )
                self.assertTrue(result["missing"])
                with self.assertRaises(ValueError):
                    acquire(
                        ("vector", (0, 0, 0), "https://example.test/map"),
                        root,
                        fetch=True,
                    )
            with patch(
                "outshine_ref.places.sources.subprocess.check_output",
                return_value=b"long source\n200",
            ):
                with self.assertRaises(ValueError):
                    acquire(
                        ("vector", (0, 0, 0), "https://example.test/map"),
                        root,
                        fetch=True,
                        max_bytes=2,
                    )
            self.assertEqual(list(Path(root).rglob("*.pbf")), [])

    def test_geography_retains_buffered_extent_and_source_windows_wrap(self):
        latitude, longitude = tile_geography(
            ((0, 0), (-4096, 0), (4096, 4096)), 4096, (1, 1, 1)
        )
        np.testing.assert_allclose(latitude, (0, 0, -85.0511287798066), atol=1e-13)
        np.testing.assert_allclose(longitude, (0, -180, -180), atol=1e-13)
        tiles = covering_tiles(0, 179.99, 5000, 10)
        self.assertIn(0, {tile[1] for tile in tiles})
        self.assertIn(1023, {tile[1] for tile in tiles})
        with self.assertRaises(ValueError):
            covering_tiles(0, 0, 500000, 22, max_tiles=2)

    @unittest.skipUnless(
        importlib.util.find_spec("mapbox_vector_tile"), "places extra required"
    )
    def test_mvt_decode_keeps_every_layer_properties_and_source_ids(self):
        import mapbox_vector_tile

        data = mapbox_vector_tile.encode(
            [
                {
                    "name": name,
                    "features": [
                        {
                            "id": index + 7,
                            "properties": {"class": name, "custom": "kept"},
                            "geometry": {"type": "Point", "coordinates": [12, 34]},
                        }
                    ],
                }
                for index, name in enumerate(
                    ("building", "landcover", "water", "transportation", "poi")
                )
            ],
            default_options={"y_coord_down": True},
        )
        with tempfile.TemporaryDirectory(prefix="outshine-tests.places.") as root:
            path = Path(root) / "source.pbf"
            path.write_bytes(data)
            result = decode_vector(
                {
                    "kind": "vector",
                    "path": str(path),
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
            )
        self.assertEqual(
            set(result), {"building", "landcover", "water", "transportation", "poi"}
        )
        for index, name in enumerate(
            ("building", "landcover", "water", "transportation", "poi")
        ):
            feature = result[name]["features"][0]
            self.assertEqual(feature["id"], index + 7)
            self.assertEqual(feature["properties"], {"class": name, "custom": "kept"})
            self.assertEqual(feature["geometry"]["coordinates"], [12, 34])

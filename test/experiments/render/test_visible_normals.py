"""Conservation, current-light response and geometry-independent storage controls."""

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from native_capture_meshes import independent_depth
from opaque_sample_grid import samples
from packed_normals import pack, unpack
from visible_normal_bake import cook, histogram
from visible_normal_compare import geometry_controls
from visible_normal_material import shade, tables
from visible_normal_query import load_product, query

PROFILE = {"width": 64, "height": 64, "focal_px": 10.0}
BOX = np.array([[-0.5, -0.5, -0.1], [0.5, 0.5, 0.1]])
RECORDS = [{"index": 0, "kind": 0, "id": 123}]


def scene(split=False):
    if split:
        positions = np.array(
            [[x, y, 10] for y in (-2, 0, 2) for x in (-2, 0, 2)], np.float32
        )
        triangles = [[a, a + 3, a + 4] for a in (0, 1, 3, 4)] + [
            [a, a + 4, a + 1] for a in (0, 1, 3, 4)
        ]
    else:
        positions = np.array(
            [[-2, -2, 10], [2, -2, 10], [2, 2, 10], [-2, 2, 10]], np.float32
        )
        triangles = [[0, 3, 2], [0, 2, 1]]
    words = pack(np.tile([0.0, 0.0, -1.0], (len(positions), 1)))
    return {
        "positions": positions,
        "indices": np.array(triangles),
        "normal": words,
        "binding": np.zeros((len(triangles), 6), np.int32),
    }


def authored_measure(path, normals, weights):
    path.mkdir()
    arrays = {
        "offsets": np.array([0, len(weights)], np.uint32),
        "normal_words": pack(normals),
        "weights": np.array(weights, np.float32),
        "depth_mass": np.zeros(1, np.float32),
        "depth2_mass": np.zeros(1, np.float32),
        "basis": np.eye(3),
        "low": np.array([-0.5, -0.5]),
        "shape": np.array([1, 1], np.uint32),
        "pitch": np.array(1.0),
        "eyes": np.zeros((1, 3)),
        "corner_bits": np.zeros((1, 3), np.uint8),
        "z0": np.array(10.0),
    }
    np.savez_compressed(path / "measure.npz", **arrays)
    header = {
        "format": "filtered-visible-normal-measure-1",
        "provenance": {},
        "observer_bounds": [[0, 0, 0], [0, 0, 0]],
        "profile": PROFILE,
    }
    (path / "header.json").write_text(json.dumps(header))
    return load_product(path, [[0, 0, 0]], {})


class MeasureTests(unittest.TestCase):
    def test_matched_control_omits_unused_vertices_and_guaranteed_backfaces(self):
        original = scene()
        source = dict(original)
        source["positions"] = np.vstack(
            (original["positions"], [[999, 999, 999]])
        ).astype(np.float32)
        source["indices"] = np.vstack(
            (original["indices"], original["indices"][:, ::-1])
        )
        source["binding"] = np.zeros((4, 6), np.int32)
        with tempfile.TemporaryDirectory() as tmp:
            report = geometry_controls(source, RECORDS, BOX, Path(tmp))
            self.assertEqual(report["selection_work"]["kept_faces"], 2)
            with np.load(Path(tmp) / "backface-control.npz") as data:
                self.assertEqual(len(data["positions"]), 4)
                self.assertEqual(len(data["indices"]), 2)
            self.assertLess(
                report["backface_only"]["appearance_array_bytes"],
                report["full"]["appearance_array_bytes"],
            )

    def test_histogram_preserves_coverage_and_distinct_normals(self):
        offsets, words, weights = histogram(
            [0, 0, 1, 1, 1, 2], [7, 9, 7, 7, 9, 42], 4, 4
        )
        np.testing.assert_array_equal(offsets, [0, 2, 4, 5, 5])
        np.testing.assert_array_equal(words, [7, 9, 7, 9, 42])
        np.testing.assert_array_equal(weights, [0.25, 0.25, 0.5, 0.25, 0.25])

    def test_distribution_is_shaded_before_meaning_away_gloss(self):
        angle = np.deg2rad(20)
        normal = np.array(
            [[np.sin(angle), 0, -np.cos(angle)], [-np.sin(angle), 0, -np.cos(angle)]]
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "measure"
            product = authored_measure(path, normal, [0.25, 0.5])
            decoded = unpack(pack(normal))
            view = np.array([0.0, 0.0, -1.0])
            light = 2 * (view @ decoded[0]) * decoded[0] - view
            before = hashlib.sha256((path / "measure.npz").read_bytes()).hexdigest()
            result = query(product, [[0, 0, 0]], [[0, 0, 1]], light, 0.12, tables())
            expected = np.array([0.25, 0.5]) @ shade(
                decoded, np.tile(view, (2, 1)), light, 0.12, tables()
            )
            np.testing.assert_allclose(result["rgb"][0], expected, rtol=1e-12)
            self.assertEqual(result["coverage"][0], 0.75)
            self.assertAlmostEqual(result["mean_depth"][0], 10)
            self.assertGreater(np.linalg.norm(result["rgb"] - result["mean_rgb"]), 0.1)
            changed = query(
                product, [[0, 0, 0]], [[0, 0, 1]], [-1, 0, -1], 0.12, tables()
            )
            self.assertFalse(np.allclose(result["rgb"], changed["rgb"]))
            self.assertEqual(
                before, hashlib.sha256((path / "measure.npz").read_bytes()).hexdigest()
            )

    def test_empty_background_and_domain_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            product = authored_measure(
                Path(tmp) / "measure", np.array([[0, 0, -1]]), [0.5]
            )
            result = query(
                product,
                [[0, 0, 0], [0, 0, 0]],
                [[0, 0, -1], [1, 0, 1e-300]],
                [0, 0, -1],
                0.45,
                tables(),
            )
            np.testing.assert_array_equal(result["coverage"], 0)
            np.testing.assert_array_equal(result["rgb"], 0)
            self.assertTrue(np.isinf(result["mean_depth"]).all())
            with self.assertRaisesRegex(ValueError, "outside supported"):
                query(product, [[1, 0, 0]], [[0, 0, 1]], [0, 0, -1], 0.45, tables())

    def test_more_original_triangles_do_not_grow_field_or_work(self):
        with tempfile.TemporaryDirectory() as tmp:
            paths = [Path(tmp) / "coarse", Path(tmp) / "split"]
            headers = [
                cook(
                    scene(split),
                    BOX,
                    [0, 1, 0],
                    PROFILE,
                    RECORDS,
                    path,
                    {},
                    opaque_control=True,
                )
                for split, path in zip((False, True), paths, strict=True)
            ]
            self.assertEqual(
                headers[0]["stored_array_bytes"], headers[1]["stored_array_bytes"]
            )
            self.assertEqual(headers[0]["work"]["rays"], headers[1]["work"]["rays"])
            with (
                np.load(paths[0] / "measure.npz") as a,
                np.load(paths[1] / "measure.npz") as b,
            ):
                for key in ("offsets", "normal_words", "weights", "sources"):
                    np.testing.assert_array_equal(a[key], b[key])
                self.assertNotIn("positions", a.files)
                self.assertNotIn("indices", a.files)

    def test_raster_and_independent_ray_measure_agree(self):
        source = scene()
        source["positions"][2, 2] = 10.2
        with tempfile.TemporaryDirectory() as tmp:
            paths = [Path(tmp) / "raster", Path(tmp) / "ray"]
            for kind, path in zip(("raster", "ray"), paths, strict=True):
                cook(
                    source,
                    BOX,
                    [0, 1, 0],
                    PROFILE,
                    RECORDS,
                    path,
                    {},
                    opaque_control=True,
                    sampling=kind,
                )
            with (
                np.load(paths[0] / "measure.npz") as a,
                np.load(paths[1] / "measure.npz") as b,
            ):
                for key in ("offsets", "normal_words", "weights", "sources"):
                    np.testing.assert_array_equal(a[key], b[key])
                for key in ("depth_mass", "depth2_mass"):
                    np.testing.assert_allclose(a[key], b[key], atol=1e-6)

    def test_inverse_depth_raster_against_all_triangle_rays(self):
        source = scene()
        source["positions"][:, 2] += 0.2 * source["positions"][:, 0]
        start, step, size = np.array([-0.31, -0.29]), np.array([0.04, 0.03]), [17, 19]
        depth, _, _ = samples(source, np.zeros(3), np.eye(3), start, step, size)
        y, x = np.indices(size[::-1])
        q = start + (np.column_stack((x.ravel(), y.ravel())) + 0.5) * step
        directions = np.column_stack((q, np.ones(len(q))))
        exact = independent_depth(
            source["positions"][source["indices"]],
            np.zeros_like(directions),
            directions,
        )
        np.testing.assert_array_equal(np.isfinite(depth), np.isfinite(exact))
        hit = np.isfinite(exact)
        np.testing.assert_allclose(depth[hit], exact[hit], atol=1e-10)

    def test_explicit_opaque_control_and_payload_after_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            with self.assertRaisesRegex(ValueError, "opaque"):
                cook(scene(), BOX, [0, 1, 0], PROFILE, RECORDS, path / "no", {})
            (path / "header.json").write_text(
                json.dumps(
                    {
                        "format": "filtered-visible-normal-measure-1",
                        "provenance": {},
                        "observer_bounds": BOX.tolist(),
                    }
                )
            )
            with self.assertRaisesRegex(ValueError, "outside supported"):
                load_product(path, [[10, 0, 0]], {})


if __name__ == "__main__":
    unittest.main()

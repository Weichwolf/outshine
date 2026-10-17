"""Independent scalar placement, source identity, tail and Blender buffer controls."""

import unittest
from unittest.mock import patch

import numpy as np
from foliage_selection import directions
from outshine_ref.vegetation.axis_surface import axis_mesh
from scipy.spatial.transform import Rotation
from window_portrait_geometry import planes, select_views
from window_portrait_source import wood


def fixture():
    frames = Rotation.from_euler("xyz", ((0.2, 0.1, 0.3), (-0.4, 0.6, 1.0))).as_matrix()
    windows = {
        "origins": np.array(((2, 1, 3), (-1, 4, 2)), float),
        "frames": frames,
        "weights": np.array((1, 0.25)),
        "first_needle_ids": np.array((0, 64), dtype=np.uint64),
    }
    image = np.zeros((32, 32, 9))
    image[..., 3] = 0.5
    image[..., 8] = 0.5 * 0.04
    bank = [
        (image, (np.eye(3), np.array((-0.02, -0.1)), np.array((0.03, 0.1)), 0))
    ] * 64
    return windows, bank


class WindowPortraitTests(unittest.TestCase):
    def test_perspective_and_distant_queries_against_scalar_dot_products(self):
        windows, _ = fixture()
        for query in (
            {"eye": np.array((1, -3, 9))},
            {"direction": np.array((-1, 2, 3))},
        ):
            expected = []
            for origin, frame in zip(windows["origins"], windows["frames"]):
                ray = query["eye"] - origin if "eye" in query else query["direction"]
                scores = [float(ray @ frame @ d) for d in directions(64)]
                expected.append(max(range(64), key=scores.__getitem__))
            np.testing.assert_array_equal(select_views(windows, 64, **query), expected)

    def test_invalid_queries_fail(self):
        windows, _ = fixture()
        for query in (
            {},
            {"eye": (0, 0, 0), "direction": (0, 1, 0)},
            {"direction": (0, 0, 0)},
            {"eye": (np.nan, 0, 0)},
        ):
            with self.assertRaises(ValueError):
                select_views(windows, 64, **query)

    def test_transformed_vertices_uv_indices_and_tail_are_independent(self):
        windows, bank = fixture()
        selected = np.array((9, 63))
        data = planes(windows, bank, selected, np.array((1, 0)), 0)
        corners = (
            (-0.02, -0.1, 0.04),
            (0.03, -0.1, 0.04),
            (0.03, 0.1, 0.04),
            (-0.02, 0.1, 0.04),
        )
        for slot, source in enumerate((1, 0)):
            expected = [
                windows["origins"][source] + windows["frames"][source] @ corner
                for corner in corners
            ]
            np.testing.assert_allclose(
                data["vertices"][slot * 4 : slot * 4 + 4], expected, rtol=0, atol=1e-15
            )
            x, y = (selected[source] % 8) * 40 + 4, (selected[source] // 8) * 40 + 4
            np.testing.assert_allclose(
                data["uv"][slot * 4 : slot * 4 + 4],
                np.array(((x, y), (x + 32, y), (x + 32, y + 32), (x, y + 32))) / 320,
                rtol=0,
                atol=1e-15,
            )
        np.testing.assert_array_equal(
            data["faces"], ((0, 1, 2), (0, 2, 3), (4, 5, 6), (4, 6, 7))
        )
        np.testing.assert_array_equal(data["first_needle_ids"], (64, 0))
        np.testing.assert_array_equal(data["weights"], (0.25, 0.25, 1, 1))

    def test_budget_fails_before_catalog_access(self):
        windows, _ = fixture()
        with self.assertRaises(ValueError):
            planes(windows, None, None, (0, 1), 0, max_windows=1)

    def test_cached_wood_matches_scalar_transformed_tube(self):
        graph = {
            "nodes": [{}]
            + [
                {
                    "shape": {
                        "length": 0.3,
                        "bend": 0.1,
                        "radius": {
                            "kind": "custom",
                            "table": [0.01, 0.008, 0.006, 0.004],
                        },
                    }
                }
            ]
            * 4
        }
        frame = Rotation.from_euler("xyz", (0.3, -0.2, 0.6)).as_matrix()
        axes = {
            "path_offsets": np.array((0, 2)),
            "rotations": frame[None],
            "origins": np.array(((2, 3, 4),)),
            "uniform_scales": np.array((0.7,)),
        }
        result = wood(graph, axes)
        mesh = axis_mesh(0.3, 0.1, 0.01, 0.004, 4, 8)
        expected = np.array(
            [axes["origins"][0] + 0.7 * frame @ p for p in mesh["vertices"]]
        )
        np.testing.assert_allclose(result["vertices"], expected, rtol=0, atol=1e-15)
        np.testing.assert_allclose(
            result["normals"], mesh["normals"] @ frame.T, rtol=0, atol=1e-15
        )
        np.testing.assert_array_equal(result["faces"], mesh["faces"])

    def test_blender_bulk_positions_faces_uv_and_weight_read_back(self):
        from pathlib import Path
        from tempfile import TemporaryDirectory

        import bpy
        from window_portrait_blender import bind_mesh, textures, weighted_material

        windows, bank = fixture()
        data = planes(windows, bank, np.array((9, 63)), np.array((0, 1)), 0)
        bpy.ops.wm.read_factory_settings(use_empty=True)
        with TemporaryDirectory(prefix="outshine-vegetation-window-check-") as temp:
            directory = Path(temp) / "atlas"
            maps = textures(bank, directory)
            surface = weighted_material("window check", directory, maps, "camera")
            obj = bind_mesh("source", data, surface)
            positions = np.empty(len(data["vertices"]) * 3, np.float32)
            obj.data.vertices.foreach_get("co", positions)
            np.testing.assert_array_equal(
                positions.reshape(-1, 3), data["vertices"].astype(np.float32)
            )
            faces = np.empty(data["faces"].size, np.int32)
            obj.data.loops.foreach_get("vertex_index", faces)
            np.testing.assert_array_equal(faces.reshape(-1, 3), data["faces"])
            weights = np.empty(len(data["faces"]), np.float32)
            obj.data.attributes["window_tail_weight"].data.foreach_get("value", weights)
            np.testing.assert_array_equal(weights, data["weights"])
            uv = np.empty(data["faces"].size * 2, np.float32)
            obj.data.uv_layers[0].data.foreach_get("uv", uv)
            np.testing.assert_array_equal(
                uv.reshape(-1, 2),
                data["uv"][data["faces"]].reshape(-1, 2).astype(np.float32),
            )
            with (
                patch(
                    "window_portrait_blender.np.asarray",
                    side_effect=AssertionError("allocation"),
                ),
                self.assertRaises(ValueError),
            ):
                bind_mesh("bad", data | {"faces": np.array(((0, 1, 99),))}, surface)


if __name__ == "__main__":
    unittest.main()

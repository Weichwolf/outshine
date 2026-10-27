"""Template ownership, bounded independent replay and original geometry envelopes."""

import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
from test_window_detail_demand import cases
from window_extent_plan import compile_bounds
from window_geometry_plan import compile_geometry
from window_geometry_templates import WindowGeometry, load, save


class WindowGeometryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plans = cases()
        cls.assets = tuple(compile_geometry(p) for p in cls.plans)

    def test_all_original_windows_keep_identity_and_fit_the_original_bounds(self):
        for prepared, asset in zip(self.plans, self.assets, strict=True):
            metadata = compile_bounds(prepared)
            self.assertEqual(asset.identifiers, metadata.identifiers)
            self.assertEqual(asset.source_id, prepared.mass.identifier)
            for i, identifier in enumerate(asset.identifiers):
                raw = asset.emit(identifier)
                self.assertTrue(np.all(raw["vertices"] >= metadata.boxes[i, 0]))
                self.assertTrue(np.all(raw["vertices"] <= metadata.boxes[i, 1]))
                self.assertTrue(all(not v.flags.writeable for v in raw.values()))

    def test_compile_builds_only_unique_shapes_without_original_window_or_floor_expansion(
        self,
    ):
        prepared = self.plans[0]
        from window_geometry_plan import details

        with (
            patch("window_geometry_plan.details", wraps=details) as calls,
            patch(
                "window_detail_demand.window_closed",
                side_effect=AssertionError("per-window rebuild"),
            ),
        ):
            result = compile_geometry(prepared)
        self.assertEqual(
            calls.call_count,
            len({(w.width_m, w.height_m) for w in prepared.facade.windows}),
        )
        self.assertEqual(calls.call_count, len(result.templates))

    def test_canonical_closed_parts_have_positive_volume_and_two_sided_edges(self):
        for asset in self.assets:
            for raw in asset.templates:
                for part in np.unique(raw["parts"]):
                    points = raw["vertices"][raw["faces"][raw["parts"] == part]]
                    origin = points[0, 0]
                    local = points - origin
                    self.assertGreater(
                        np.einsum(
                            "ij,ij->", local[:, 0], np.cross(local[:, 1], local[:, 2])
                        )
                        / 6,
                        0,
                    )
                    _, ids = np.unique(
                        points.reshape(-1, 3), axis=0, return_inverse=True
                    )
                    edges = ids.reshape(-1, 3)[:, [[0, 1], [1, 2], [2, 0]]].reshape(
                        -1, 2
                    )
                    _, groups, counts = np.unique(
                        np.sort(edges, axis=1),
                        axis=0,
                        return_inverse=True,
                        return_counts=True,
                    )
                    np.testing.assert_array_equal(counts, 2)
                    np.testing.assert_array_equal(
                        np.bincount(groups, weights=edges[:, 0] < edges[:, 1]), 1
                    )

    def test_saved_reader_emits_the_same_arrays_and_rejects_foreign_source(self):
        asset = self.assets[0]
        with tempfile.TemporaryDirectory(
            prefix="outshine-window-geometry-"
        ) as directory:
            path = Path(directory) / "asset"
            save(asset, path)
            with self.assertRaises(ValueError):
                load(path, "foreign", asset.recipe)
            with self.assertRaises(ValueError):
                load(path, asset.source_id, "foreign")
            restored = load(path, asset.source_id, asset.recipe)
            self.assertEqual(restored.identifiers, asset.identifiers)
            for identifier in asset.identifiers[::31]:
                a, b = asset.emit(identifier), restored.emit(identifier)
                for key in a:
                    self.assertEqual(a[key].dtype, b[key].dtype)
                    self.assertEqual(a[key].tobytes(), b[key].tobytes())

    def test_storage_owns_immutable_arrays_and_forward_affine_frame_is_honored(self):
        source = self.assets[0]
        placements = source.placements.copy()
        value = replace(source, placements=placements)
        before = value.emit(value.identifiers[0])["vertices"].copy()
        placements[:] = 0
        np.testing.assert_array_equal(
            before, value.emit(value.identifiers[0])["vertices"]
        )
        with self.assertRaises(ValueError):
            value.placements.flags.writeable = True
        frame = np.array([[1e6, -2e6], [1.2, 0.1], [0.3, 0.9]])
        changed = replace(value, frame=frame)
        identity = replace(value, frame=np.array([[0.0, 0.0], [1.0, 0.0], [0.0, 1.0]]))
        local = identity.emit(identity.identifiers[0])["vertices"]
        expected = local.copy()
        expected[:, :2] = frame[0] + local[:, :1] * frame[1] + local[:, 1:2] * frame[2]
        np.testing.assert_array_equal(
            expected, changed.emit(changed.identifiers[0])["vertices"]
        )

    def test_invalid_identity_shape_indices_and_coordinates_reject(self):
        asset = self.assets[0]
        invalid = (
            {"identifiers": (asset.identifiers[0],) * len(asset.identifiers)},
            {
                "indices": np.full(
                    len(asset.identifiers), len(asset.templates), dtype=np.uint32
                )
            },
            {"placements": np.full(asset.placements.shape, np.nan)},
            {"placements": np.zeros_like(asset.placements)},
            {"frame": np.zeros((2, 2))},
            {"recipe": "unknown"},
        )
        for changes in invalid:
            with self.assertRaises(ValueError):
                replace(asset, **changes)
        with self.assertRaises(KeyError):
            asset.emit("foreign")
        empty = WindowGeometry(
            "empty",
            "cascade",
            (),
            np.array([[0.0, 0.0], [1.0, 0.0], [0.0, 1.0]]),
            np.empty((0, 5)),
            np.empty(0, dtype=np.uint32),
            (),
        )
        self.assertEqual(empty.array_bytes, 48)


if __name__ == "__main__":
    unittest.main()

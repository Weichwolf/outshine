"""Root profile metadata must preserve the actual historical recipe and source order."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
from camera_height_inputs import digest
from stair_cycle_inputs import load
from stair_local_prepare import root_rows
from stair_module_inputs import FIXTURE, source_context


class ResidentProfiles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import json

        cls.reference = FIXTURE.with_name(
            json.loads(FIXTURE.read_text())["reference_fixture"]
        )
        cls.loaded = load(cls.reference)
        cls.rows = cls.loaded[1]
        cls.sources = source_context(cls.reference)[1]
        cls.state = {
            "report": {
                "fixture_sha256": digest(cls.reference),
                "candidate_source_ids": [r["source_id"] for r in cls.rows],
                "after": {"body_sha256": "verified-body-binding"},
            }
        }

    def test_actual_axes_widths_and_altered_axis_are_preserved(self):
        with patch("stair_cycle_inputs.load", return_value=self.loaded):
            rows = root_rows(self.reference, self.state)
        self.assertEqual(len(rows), 32)
        equal = [
            np.array_equal(r["xy"], self.sources[r["source_id"]]["xy_m"]) for r in rows
        ]
        self.assertEqual(sum(equal), 31)
        target = next(r for r in rows if r["source_id"].endswith("32651280:3"))
        self.assertEqual(target["width_m"], 2.0)
        self.assertEqual(
            target["resident_profile_provenance"]["body_sha256"],
            "verified-body-binding",
        )
        for before, after in zip(self.rows, rows, strict=True):
            np.testing.assert_array_equal(before["xy"], after["xy"])
            self.assertEqual(before["width_m"], after["width_m"])
            self.assertNotIn("original", after)

    def test_wrong_recipe_is_refused_before_loading(self):
        state = copy.deepcopy(self.state)
        state["report"]["fixture_sha256"] = "wrong"
        with patch("stair_cycle_inputs.load") as loader:
            with self.assertRaisesRegex(ValueError, "verified root recipe"):
                root_rows(self.reference, state)
            loader.assert_not_called()

    def test_missing_or_reordered_sources_are_refused(self):
        for keys in (
            self.state["report"]["candidate_source_ids"][:-1],
            list(reversed(self.state["report"]["candidate_source_ids"])),
        ):
            state = copy.deepcopy(self.state)
            state["report"]["candidate_source_ids"] = keys
            with (
                patch("stair_cycle_inputs.load", return_value=self.loaded),
                self.assertRaisesRegex(ValueError, "ordered root source"),
            ):
                root_rows(self.reference, state)


class ActualResidentCap(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import hashlib
        import json
        from pathlib import Path

        path = (
            Path(__file__).with_name("fixtures")
            / "stair-resident-cycleway-profile-vienna.npz"
        )
        cls.meta = json.loads(path.with_suffix(".json").read_text())
        if hashlib.sha256(path.read_bytes()).hexdigest() != cls.meta["data_sha256"]:
            raise ValueError("actual original resident cap fixture changed")
        with np.load(path, allow_pickle=False) as data:
            cls.vertices, cls.faces = data["vertices"], data["faces"]
        if (
            hashlib.sha256(cls.vertices[cls.faces].tobytes()).hexdigest()
            != cls.meta["actual_cap_XYZ_sha256"]
        ):
            raise ValueError("actual original owner facet coordinates changed")

    def context(self):
        key = self.meta["source_id"]
        sources = {key: {"xy_m": self.meta["xy_m"], "mvt_xy": self.meta["mvt_xy"]}}
        state = {
            "cached_module_rows": {
                key: {
                    "xy": np.array(self.meta["xy_m"]),
                    "width_m": self.meta["width_m"],
                }
            },
            "road": {
                "body_vertices": self.vertices.copy(),
                "top_faces": self.faces.copy(),
                "face_sources": np.ones(len(self.faces), np.uint64),
            },
            "road_ground": np.ones(len(self.faces), bool),
            "report": {"candidate_source_ids": [key]},
        }
        return state, copy.deepcopy(sources), key

    def test_actual_full_varying_profile_is_preserved(self):
        from stair_module_loops import interior_profile

        state, sources, key = self.context()
        before = state["road"]["body_vertices"].copy()
        result = interior_profile(state, sources, key, 8)
        self.assertEqual(result["mvt_xy"], [4011, 2802])
        self.assertAlmostEqual(
            np.linalg.norm(result["profile"][-1, :2] - result["profile"][0, :2]), 2.0
        )
        self.assertGreater(np.ptp(result["profile"][:, 2]), 0.007)
        np.testing.assert_array_equal(state["road"]["body_vertices"], before)

    def test_changed_axis_frame_width_or_owner_are_refused(self):
        from stair_module_loops import interior_profile

        for kind in ("axis", "frame", "width", "owner", "ground"):
            state, sources, key = self.context()
            if kind == "axis":
                state["cached_module_rows"][key]["xy"][8, 0] += 0.001
            elif kind == "frame":
                sources[key]["xy_m"][8][0] += 0.001
            elif kind == "width":
                state["cached_module_rows"][key]["width_m"] = 3.0
            elif kind == "owner":
                state["road"]["face_sources"][:] = 0
            else:
                state["road_ground"][:] = False
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                interior_profile(state, sources, key, 8)

    def test_profile_with_missing_actual_face_interval_is_refused(self):
        from stair_module_loops import interior_profile

        state, sources, key = self.context()
        result = interior_profile(state, sources, key, 8)
        centre = result["profile"][1, :2]
        import shapely

        faces = state["road"]["top_faces"]
        polygons = shapely.polygons(state["road"]["body_vertices"][faces, :2])
        state["road"]["face_sources"][
            shapely.dwithin(polygons, shapely.Point(centre), 0.01)
        ] = 0
        with self.assertRaises(ValueError):
            interior_profile(state, sources, key, 8)

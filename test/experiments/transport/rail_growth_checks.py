"""Whole disjoint additions retain raised bodies and fail before mixed admission."""

import json
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import LocalFrame, Mesh
from rail_consumers_buildings import bound_products
from rail_occupancy_checks import products
from rail_occupancy_growth import disjoint, extend, xy_bounds
from rail_occupancy_reference import whole_raised
from stair_local_checks import asset


class RaisedGrowthChecks(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="outshine-raised-growth-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.path = self.root / "module"
        self.path.mkdir()
        self.report = {"valid_local_module": True}
        (self.path / "comparison.json").write_text(json.dumps(self.report))
        (self.path / "verification.json").write_text("{}")
        frame = LocalFrame("growth-check")
        root = {
            "frame": frame.identifier,
            "origin_lat_lon": [0, 0],
            "support_sha256": "old-supports",
        }
        (self.root / "resident.json").write_text(json.dumps(root))
        state = (
            SimpleNamespace(patches=[SimpleNamespace(frame=frame)]),
            (asset(0, x=60),),
            (),
            (),
            dict(
                root,
                root_resident_sha256=digest(self.root / "resident.json"),
                parent_reports=[],
                source_ids=["0:0"],
            ),
        )
        original = products()
        self.base = replace(
            original,
            state=state,
            state_inputs=(self.root, ()),
            originals={**original.originals, "1:0": {"properties": {}}},
        )
        self.changed = Mesh([[100, 0, 0], [102, 0, 0], [100, 2, 0]], [[0, 1, 2]])

    def candidate(self, road=None, changed=None):
        return (
            SimpleNamespace(
                patches=[
                    self.base.state[0].patches[0],
                    SimpleNamespace(mesh=self.changed if changed is None else changed),
                ]
            ),
            (*self.base.state[1], asset(1, x=100) if road is None else road),
            self.base.state[2],
            self.base.state[3],
            dict(self.base.state[4], source_ids=["0:0", "1:0"]),
        )

    def test_disjoint_append_reuses_old_buffers_and_exact_retained_raised_objects(self):
        candidate = self.candidate()
        with patch(
            "rail_occupancy_growth.append_verified", return_value=candidate
        ) as append:
            grown, proof = extend(self.base, [self.path])
        append.assert_called_once_with(self.base.state, self.path)
        self.assertIs(grown.state, candidate)
        for name in ("deck", "supports", "contacts", "top_faces", "rows"):
            self.assertIs(getattr(grown, name), getattr(self.base, name))
        self.assertEqual(grown.state_inputs, (self.root, (self.path,)))
        self.assertEqual(proof[0]["source_ids"], ["1:0"])
        self.assertEqual(self.base.state[4]["source_ids"], ["0:0"])

    def test_unchanged_legacy_base_needs_no_new_contract(self):
        old = products()
        result, proof = extend(old)
        self.assertIs(result, old)
        self.assertEqual(proof, [])
        with self.assertRaisesRegex(ValueError, "verified raised base lineage"):
            extend(old, [self.path])

    def test_capacity_duplicate_and_frame_reject_before_append(self):
        with patch(
            "rail_occupancy_growth.append_verified",
            side_effect=AssertionError("must reject first"),
        ):
            for paths in (
                (self.path, self.path),
                tuple(self.root / str(i) for i in range(33)),
            ):
                with self.assertRaisesRegex(ValueError, "bounded unique"):
                    extend(self.base, paths)
            for key, value in (
                ("frame", "other"),
                ("origin_lat_lon", [1, 0]),
                ("support_sha256", "other"),
            ):
                state = (*self.base.state[:4], dict(self.base.state[4], **{key: value}))
                with self.assertRaisesRegex(ValueError, "frame, origin and supports"):
                    extend(replace(self.base, state=state), [self.path])

    def test_body_or_any_changed_region_overlap_is_not_an_underpass(self):
        for candidate in (
            self.candidate(road=asset(1, x=8)),
            self.candidate(
                changed=Mesh([[8, 0, 0], [9, 0, 0], [8, 1, 0]], [[0, 1, 2]])
            ),
        ):
            with (
                patch("rail_occupancy_growth.append_verified", return_value=candidate),
                self.assertRaisesRegex(ValueError, "overlapping raised region"),
            ):
                extend(self.base, [self.path])
        self.assertEqual(len(self.base.state[1]), 1)

    def test_float32_touching_boxes_are_not_declared_separated(self):
        a = np.array([[1.00000001, 0, 0], [1.00000001, 1, 0]])
        b = np.array([[1.00000002, 0, 0], [1.00000002, 1, 0]])
        self.assertGreater(b[:, 0].min(), a[:, 0].max())
        self.assertFalse(disjoint(xy_bounds(b), xy_bounds(a)[None]))
        for cast in (False, True):
            represented, _ = whole_raised(self.base, cast=cast)
            meshes = (self.base.deck, *self.base.supports)
            box = np.vstack([xy_bounds(m.vertices, m.triangles) for m in meshes])
            overall = np.r_[box[:, :2].min(0), box[:, 2:].max(0)]
            covered = shapely.union_all(shapely.box(*box.T))
            for row in represented:
                xy = np.array(row.geometry.bounds)
                self.assertTrue(
                    np.all(xy[:2] >= overall[:2]) and np.all(xy[2:] <= overall[2:])
                )
                self.assertTrue(covered.covers(row.geometry))

    def test_reader_rejects_unbounded_or_rebound_extension_records(self):
        for records in (None, [{}], [{"path": str(self.path)}] * 33):
            with (
                patch(
                    "rail_consumers_buildings.load",
                    side_effect=AssertionError("must reject before base load"),
                ),
                self.assertRaisesRegex(ValueError, "bounded explicit"),
            ):
                bound_products({"transport_extensions": records})
        report = {
            "elevated_supports": str(self.root),
            "elevated_report_sha256": "hash",
            "elevated_receipt_sha256": "hash",
            "transport_extensions": [{"path": str(self.path)}],
        }
        with (
            patch("rail_consumers_buildings.digest", return_value="hash"),
            patch("rail_consumers_buildings.load", return_value=self.base),
            patch(
                "rail_consumers_buildings.extend",
                return_value=(self.base, [{"different": True}]),
            ),
            self.assertRaisesRegex(ValueError, "extension bindings differ"),
        ):
            bound_products(report)

    def test_raised_or_unknown_ground_roles_and_replaced_old_products_fail(self):
        for properties in ({"brunnel": "bridge"}, {"indoor": True}, {"layer": 1}):
            originals = {**self.base.originals, "1:0": {"properties": properties}}
            with (
                patch(
                    "rail_occupancy_growth.append_verified",
                    return_value=self.candidate(),
                ),
                self.assertRaisesRegex(ValueError, "known ground roles"),
            ):
                extend(replace(self.base, originals=originals), [self.path])
        duplicated = replace(asset(1, x=100), source_ids=("original-raised-rail",))
        with (
            patch(
                "rail_occupancy_growth.append_verified",
                return_value=self.candidate(road=duplicated),
            ),
            self.assertRaisesRegex(ValueError, "known ground roles"),
        ):
            extend(self.base, [self.path])
        candidate = self.candidate()
        changed = (candidate[0], (asset(0, x=60), candidate[1][-1]), *candidate[2:])
        with (
            patch("rail_occupancy_growth.append_verified", return_value=changed),
            self.assertRaisesRegex(ValueError, "retain every old product"),
        ):
            extend(self.base, [self.path])


if __name__ == "__main__":
    unittest.main()

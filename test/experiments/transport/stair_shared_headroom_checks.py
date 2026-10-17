"""Logical car volume on unchanged footway facets; boundaries and provenance."""

import copy
import dataclasses
import hashlib
import json
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from shared_bridge_geometry import corridor
from stair_occupancy_handoff import OccupancyQuery
from stair_occupancy_inputs import volume
from stair_occupancy_reuse import prefix
from stair_shared_headroom import binding
from stair_shared_headroom_verify import verify_shared
from stair_shared_headroom_volumes import supplement, upper_band


def scene():
    top = np.array([[0, -2, 10], [2, -2, 10.1], [2, 2, 10.1], [0, 2, 10]])
    faces = np.array([[0, 1, 2], [0, 2, 3]])
    old = SimpleNamespace(
        identity="old",
        source_ids=("foot",),
        data={
            "body_vertices": np.vstack((top, top - [0, 0, 0.3])),
            "top_faces": faces,
            "owners": np.ones((2, 1), bool),
            "ground": np.ones(2, bool),
        },
        shapes=shapely.polygons(top[faces, :2]),
    )
    row = {
        "source_id": "car",
        "xy": [[0, 0], [4, 0]],
        "width_m": 2,
        "profile_kind": "unclassified",
        "traffic_profile": {"clearance_m": 4.5},
    }
    new = SimpleNamespace(identity="new", source_ids=("car",), rows=(row,))
    contact = {
        "source_id": "car",
        "cached_source_id": "foot",
        "asset_sha256": "old",
        "asset_owner_column": 0,
        "asset_top_face_indices": [0, 1],
        "top_xyz_sha256": hashlib.sha256(top[faces].tobytes()).hexdigest(),
        "access": {"kind": "minor_footway_node", "motor_cycle_contact_arcs": 0},
        "mask_wkt": shapely.union_all(old.shapes).intersection(corridor(row)).wkt,
    }
    rows = tuple(
        volume(f"old:face:{i}", top[f] - [0, 0, 0.3], top[f], 2.5)[0]
        for i, f in enumerate(faces)
    )
    sources = {"foot": {"properties": {"subclass": "footway"}}}
    return new, old, contact, rows, (("foot",), ("foot",)), sources


class SharedHeadroomChecks(unittest.TestCase):
    def test_only_missing_upper_band_preserves_both_origins_and_physical_objects(self):
        new, old, contact, rows, owners, sources = scene()
        chosen = binding(new, old, contact)
        extra, origins, records, work = supplement(rows, owners, (chosen,), sources)
        self.assertEqual(origins, (("car", "foot"),) * 2)
        self.assertEqual(work["additional_volumes"], 2)
        for base, added, record in zip(rows, extra, records, strict=True):
            self.assertIs(added.lower_plane_m, base.upper_plane_m)
            self.assertTrue(added.geometry.covered_by(base.geometry))
            self.assertEqual(record["physical_source_ids"], ["foot"])
            self.assertEqual(record["required_clearance_m"], 4.5)
            self.assertEqual(added.upper_plane_m[1:], base.upper_plane_m[1:])

    def test_extra_and_full_duplicate_bands_have_identical_positive_conflicts(self):
        new, old, contact, rows, owners, sources = scene()
        chosen = (binding(new, old, contact),)
        extra, *_ = supplement(rows, owners, chosen, sources)
        full, *_ = supplement(rows, owners, chosen, sources, full=True)
        queries = [OccupancyQuery((*rows, *r)) for r in (extra, full)]
        for xy in [(0.5, 0), (1, 1.5), (2.5, 0)]:
            for height in [9, 10, 12.5, 13.0, 14.4, 14.6, 15]:
                p = BuildingFootprint(
                    "probe",
                    shapely.box(xy[0] - 0.1, xy[1] - 0.1, xy[0] + 0.1, xy[1] + 0.1),
                    HeightInterval(height, height + 0.1),
                )
                self.assertTrue(
                    queries[0].conflict(p)[0].equals(queries[1].conflict(p)[0])
                )

    def test_actual_saved_footway_regression_requires_car_clearance(self):
        root = Path(__file__).with_name("fixtures")
        meta = json.loads((root / "stair-shared-paving-caps-vienna.json").read_text())
        case = json.loads((root / "stair-shared-headroom-vienna.json").read_text())
        path = root / "stair-shared-paving-caps-vienna.npz"
        self.assertEqual(
            hashlib.sha256(path.read_bytes()).hexdigest(), meta["data_sha256"]
        )
        cap = meta["caps"][0]
        self.assertEqual(cap["asset_sha256"], case["asset_sha256"])
        index = cap["asset_top_face_indices"].index(case["face"])
        with np.load(path, allow_pickle=False) as data:
            top = data["cap0_vertices"][data["cap0_faces"][index]]
        base, _ = volume("actual", top - [0, 0, 0.3], top, 2.5)
        shape = base.geometry.intersection(
            shapely.from_wkt(meta["original_source_mask_wkt"])
        )
        extra = upper_band(base, shape, 2, "new-logical-car")
        p = BuildingFootprint(
            "actual-counterexample",
            shapely.from_wkb(case["polygon_wkb"]),
            HeightInterval(*case["heights_m"]),
        )
        self.assertTrue(OccupancyQuery((base,)).conflict(p)[0].is_empty)
        self.assertTrue(OccupancyQuery((base, extra)).conflict(p)[0].equals(p.geometry))

    def test_false_asset_owner_facet_xyz_mask_and_motor_claim_are_refused(self):
        new, old, contact, *_ = scene()
        for field, value in [
            ("asset_sha256", "other"),
            ("asset_owner_column", 1),
            ("asset_top_face_indices", [0]),
            ("top_xyz_sha256", "bad"),
            ("mask_wkt", "POLYGON ((0 0,1 0,1 1,0 1,0 0))"),
            ("access", {"kind": "minor_footway_node", "motor_cycle_contact_arcs": 1}),
        ]:
            changed = dict(contact, **{field: value})
            with self.subTest(field=field), self.assertRaises(ValueError):
                binding(new, old, changed)
        new.rows[0]["traffic_profile"]["clearance_m"] = 2.5
        with self.assertRaises(ValueError):
            binding(new, old, contact)

    def test_missing_changed_and_duplicate_extra_rows_never_verify(self):
        new, old, contact, rows, owners, sources = scene()
        chosen = binding(new, old, contact)
        extra, origins, records, _ = supplement(rows, owners, (chosen,), sources)
        combined = (*rows, *extra)
        metadata = [{"source_ids": list(k)} for k in (*owners, *origins)]
        report = {"borrowed_headroom": {"records": records}}
        with patch("stair_shared_headroom_verify.bindings", return_value=(chosen,)):
            count, _ = verify_shared(combined, metadata, (), (), sources, 2, report)
            self.assertEqual(count, 2)
            for bad_rows, bad_data in [
                (rows, metadata[:2]),
                (combined, metadata[:2] + [{"source_ids": ["foot"]}] * 2),
            ]:
                with self.assertRaises(ValueError):
                    verify_shared(bad_rows, bad_data, (), (), sources, 2, report)
            for changed in (
                dataclasses.replace(
                    extra[0],
                    upper_plane_m=(
                        extra[0].upper_plane_m[0] - 0.01,
                        *extra[0].upper_plane_m[1:],
                    ),
                ),
                dataclasses.replace(
                    extra[0],
                    lower_plane_m=(
                        extra[0].lower_plane_m[0] + 0.01,
                        *extra[0].lower_plane_m[1:],
                    ),
                ),
                dataclasses.replace(extra[0], geometry=extra[0].geometry.buffer(-0.01)),
            ):
                with self.assertRaises(ValueError):
                    verify_shared(
                        (*rows, changed, extra[1]), metadata, (), (), sources, 2, report
                    )
        with self.assertRaises(ValueError):
            supplement(rows, owners, (chosen, chosen), sources)

    def test_no_extension_for_already_sufficient_physical_car_headroom(self):
        new, old, contact, rows, owners, _ = scene()
        chosen = binding(new, old, contact)
        sources = {"foot": {"properties": {"class": "minor"}}}
        rows = tuple(
            volume(f"old:face:{i}", top - [0, 0, 0.3], top, 4.5)[0]
            for i, top in enumerate(old.data["body_vertices"][old.data["top_faces"]])
        )
        extra, _, _, _ = supplement(rows, owners, (chosen,), sources)
        self.assertEqual(extra, ())
        chosen["indices"] = np.zeros(32769, int)
        with self.assertRaisesRegex(ValueError, "budget"):
            supplement(rows, owners, (chosen,), sources)

    def test_prefix_reuse_rejects_changed_lineage_and_existing_logical_extras(self):
        report = {
            "frame": "a",
            "root_resident_sha256": "sha",
            "source_ids": ["one"],
            "parent_reports": ["p"],
        }
        receipt = {"valid_saved_occupancy": True}
        manifest = {
            **report,
            "source_ids": ["one", "two"],
            "parent_reports": ["p", "q"],
        }
        prefix(report, receipt, manifest)
        for field, value in [
            ("frame", "b"),
            ("root_resident_sha256", "bad"),
            ("source_ids", ["other"]),
            ("parent_reports", ["bad"]),
            ("borrowed_headroom", {"records": [1]}),
        ]:
            changed = copy.deepcopy(report)
            changed[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                prefix(changed, receipt, manifest)


if __name__ == "__main__":
    unittest.main()

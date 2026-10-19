"""Projected batches must preserve every original source exactly once."""

import copy
import unittest

from native_cell_mesh_validation import validate_row


def fixture():
    calls = [
        {
            "accepted": True,
            "members": [
                {"source": i, "detail": 1, "openings": False} for i in range(2)
            ],
        }
    ]
    row = {
        "accepted": True,
        "unsupported": 0,
        "details": [1, 1],
        "footprints": 2,
        "mesh_calls": [],
        "projection_calls": calls,
        "lumped": 2,
        "blocks": 1,
        "wall_vertices": 4,
        "roof_vertices": 0,
        "built_bytes": 120,
        "fallback_triangles": 2,
    }
    batch = {
        "capture": {
            name: {"count": count, "bytes": size}
            for name, count, size in (
                ("walls", 4, 96),
                ("roofs", 0, 0),
                ("wall_indices", 6, 24),
                ("roof_indices", 0, 0),
                ("colours", 0, 0),
                ("metres", 0, 0),
            )
        },
        "calls": [{"emitter": "EmitProjected"}],
    }
    return row, batch


class CellMeshValidationTests(unittest.TestCase):
    def test_collective_members_are_retained_without_per_face_ids(self):
        row, batch = fixture()
        validate_row(row, batch, 2)
        self.assertEqual(
            batch["calls"][0]["native_call"]["members"],
            row["projection_calls"][0]["members"],
        )
        self.assertNotIn("source", batch["calls"][0]["native_call"])

    def test_repeated_missing_or_wrong_detail_sources_reject(self):
        for members in ([0, 0], [0], [0, 2]):
            row, batch = fixture()
            row["projection_calls"][0]["members"] = [
                {"source": i, "detail": 1, "openings": False} for i in members
            ]
            with self.assertRaises(RuntimeError):
                validate_row(row, batch, 2)
        row, batch = fixture()
        row["projection_calls"][0]["members"][0]["openings"] = True
        with self.assertRaises(RuntimeError):
            validate_row(row, batch, 2)

    def test_unobserved_calls_and_bad_final_counts_reject(self):
        for change in ("bytes", "emitter", "block"):
            row, batch = copy.deepcopy(fixture())
            if change == "bytes":
                row["built_bytes"] += 4
            elif change == "emitter":
                batch["calls"][0]["emitter"] = "Mesh"
            else:
                row["blocks"] += 1
            with self.assertRaises(RuntimeError):
                validate_row(row, batch, 2)


if __name__ == "__main__":
    unittest.main()

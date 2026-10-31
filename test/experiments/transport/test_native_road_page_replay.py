"""Exact native atoms, contact closure, corrupt data and movement residency controls."""

import copy
import json
import sqlite3
import tempfile
import unittest
import zlib
from contextlib import closing
from pathlib import Path

import native_road_page_layout as layout
import native_road_page_replay as replay
import native_road_parts as corpus
import zstandard


class NativeRoadPageReplay(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        manifest, cls.metadata, cls.parts, cls.payload, cls.materials = corpus.load()
        cls.scope = manifest["archive_sha256"]
        cls.boxes = layout.bounds(cls.parts, cls.payload)

    def small_database(self, target):
        return layout.prepare(
            target,
            [[0, 1], [2, 3]],
            self.parts[:4],
            self.boxes[:4],
            self.payload,
            self.materials,
            self.scope,
        )

    def test_pending_is_deduplicated_but_is_not_ready(self):
        wanted, resident, pending = (
            {"new", "ready", "missing"},
            {"ready"},
            {"new", "stale"},
        )
        requests, cancel = replay.delta(wanted, resident, pending)
        self.assertEqual(requests, {"missing"})
        self.assertEqual(cancel, {"stale"})
        self.assertFalse(wanted <= resident)
        self.assertEqual(replay.delta(wanted, wanted), (set(), set()))

    def test_bounded_groups_keep_whole_oversized_atoms_and_one_owner(self):
        sizes = [p["payload_bytes"] for p in self.parts]
        groups = layout.partition(self.boxes, sizes, 256.0, 65536)
        self.assertEqual(
            sorted(i for g in groups for i in g), list(range(len(self.parts)))
        )
        self.assertTrue(
            all(sum(sizes[i] for i in g) <= 65536 or len(g) == 1 for g in groups)
        )
        with (
            tempfile.TemporaryDirectory() as temp,
            self.assertRaisesRegex(ValueError, "one package owner"),
        ):
            layout.prepare(
                Path(temp) / "bad",
                [[0, 1], [1, 2, 3]],
                self.parts[:4],
                self.boxes[:4],
                self.payload,
                self.materials,
                self.scope,
            )

    def test_stillness_same_page_return_and_shared_material_need_no_reread(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "pages"
            info = self.small_database(target)
            trace = {
                "frames": [
                    {"name": str(i), "parts": [p]}
                    for i, p in enumerate((0, 0, 1, 2, 0))
                ]
            }
            result = replay.replay(target, trace, 1024**2)
            self.assertEqual([f["requests"] for f in result["frames"]], [2, 0, 0, 1, 0])
            self.assertEqual(result["totals"]["requests"], 3)
            self.assertEqual(result["totals"]["read_bytes"], info["stored_bytes"])
            self.assertEqual(result["totals"]["decoded_bytes"], info["native_bytes"])

    def test_small_target_evicts_old_data_without_dropping_required_content(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "pages"
            self.small_database(target)
            trace = {
                "frames": [
                    {"name": str(i), "parts": [p]} for i, p in enumerate((0, 2, 0))
                ]
            }
            result = replay.replay(target, trace, 1)
            self.assertEqual([f["requests"] for f in result["frames"]], [2, 1, 1])
            self.assertEqual(result["totals"]["evictions"], 2)
            self.assertEqual(result["totals"]["over_budget_frames"], 3)
            self.assertTrue(
                all(
                    f["resident_bytes"] >= f["required_bytes"] for f in result["frames"]
                )
            )

    def test_unchanged_demand_never_selects_unneeded_blob(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "pages"
            self.small_database(target)
            with closing(sqlite3.connect(target / "assets.sqlite")) as database:
                excluded = database.execute(
                    "SELECT package FROM assets WHERE id=3"
                ).fetchone()[0]
                statements = []
                database.set_trace_callback(statements.append)
                result = replay.replay_pages(
                    database,
                    target,
                    {
                        "frames": [
                            {"name": "first", "parts": [0]},
                            {"name": "hold", "parts": [0]},
                        ]
                    },
                    1024**2,
                )
            reads = [
                s for s in statements if s.startswith("SELECT bytes FROM native_packages")
            ]
            self.assertEqual(len(reads), 2)
            self.assertTrue(all(excluded not in statement for statement in reads))
            self.assertEqual(result["frames"][1]["read_bytes"], 0)

    def test_empty_emission_keeps_metadata_without_payload_requests(self):
        empty = next(i for i, p in enumerate(self.parts) if p["payload_bytes"] == 0)
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "empty"
            layout.prepare(
                target,
                [[0]],
                [self.parts[empty]],
                self.boxes[[empty]],
                self.payload,
                self.materials,
                self.scope,
            )
            result = replay.replay(
                target, {"frames": [{"name": "metadata-only", "parts": [0]}]}, 1024
            )
            self.assertEqual(result["totals"]["requests"], 0)
            self.assertEqual(result["resident_payload_peak_bytes"], 0)

    def test_raw_blob_has_no_zstd_decode_allocation(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "pages"
            self.small_database(target)
            with (
                closing(sqlite3.connect(target / "assets.sqlite")) as database,
                database,
            ):
                key = database.execute(
                    "SELECT package FROM assets WHERE id=1"
                ).fetchone()[0]
                stored, codec = database.execute(
                    "SELECT bytes,codec FROM packages WHERE key=?", (key,)
                ).fetchone()
                raw = (
                    zstandard.ZstdDecompressor().decompress(stored) if codec else stored
                )
                database.execute(
                    "UPDATE packages SET bytes=?,crc=?,codec=0 WHERE key=?",
                    (raw, zlib.crc32(raw), key),
                )
            result = replay.replay(
                target, {"frames": [{"name": "raw", "parts": [0]}]}, 1024**2
            )
            self.assertEqual(
                result["totals"]["loaded_native_bytes"]
                - result["totals"]["decoded_bytes"],
                len(raw),
            )

    def test_valid_crc_cannot_hide_changed_original_payload(self):
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "pages"
            self.small_database(target)
            with (
                closing(sqlite3.connect(target / "assets.sqlite")) as database,
                database,
            ):
                key = database.execute(
                    "SELECT package FROM assets WHERE id=1"
                ).fetchone()[0]
                stored, _, codec, size = database.execute(
                    "SELECT bytes,crc,codec,native_bytes FROM packages WHERE key=?",
                    (key,),
                ).fetchone()
                raw = bytearray(
                    zstandard.ZstdDecompressor().decompress(stored) if codec else stored
                )
                raw[0] ^= 1
                stored = zstandard.ZstdCompressor(level=1).compress(raw)
                database.execute(
                    "UPDATE packages SET bytes=?,crc=?,codec=1,native_bytes=? WHERE key=?",
                    (stored, zlib.crc32(stored), size, key),
                )
            with self.assertRaisesRegex(ValueError, "original native payload"):
                replay.replay(
                    target, {"frames": [{"name": "corrupt", "parts": [0]}]}, 1024**2
                )

    def test_trace_preserves_contacts_and_detects_independently_changed_cut(self):
        trace = layout.demands(self.parts, self.metadata, self.boxes, samples=3)
        junction = next(
            i
            for frame in trace["frames"]
            for i in frame["parts"]
            if self.parts[i]["kind"] == "junction"
        )
        changed = list(self.parts)
        changed[junction] = copy.deepcopy(self.parts[junction])
        changed[junction]["legs"][0][4] += 0.01
        with self.assertRaisesRegex(ValueError, "leg cut"):
            layout.demands(changed, self.metadata, self.boxes, samples=3)
        self.assertEqual(trace["frames"][0]["parts"], trace["frames"][-1]["parts"])
        self.assertEqual(json.loads(json.dumps(trace)), trace)


if __name__ == "__main__":
    unittest.main()

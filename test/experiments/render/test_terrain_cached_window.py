"""Offline extraction must preserve source words and refuse incomplete neighborhoods."""

import hashlib
import json
import sqlite3
import struct
import tarfile
import tempfile
import unittest
import zlib
from contextlib import closing
from pathlib import Path

import numpy as np
from terrain_cached_page import page_fractions, select_page, select_window
from terrain_cached_window import export

FIXTURE = Path(__file__).resolve().parents[2] / "outshine/data/terrain/native-cell-page"
HEIGHTS = (FIXTURE / "238.f32").read_bytes()
SOURCE = json.loads((FIXTURE / "input.json").read_text())
WINDOW = (20, 572088, 572089, 363482, 363482)


def record(x=572088, y=363482, *, zoom=20, heights=HEIGHTS, side=33):
    return (
        struct.pack("<iIIiIBiI", zoom, x, y, side, 33, 1, 17, len(heights) // 4)
        + heights
    )


def region(records):
    # Version-5 native prefix only; no claim about the deliberately absent tail.
    ground = b"".join(
        (
            struct.pack("<Qdd", 0x0005314E5247, *SOURCE["anchor_lon_lat"]),
            bytes(44 + 268),
            struct.pack("<III", 0, 0, len(records)),
            *records,
        )
    )
    return struct.pack("<QQ", 0x35524E474F, len(ground)) + ground


def cache(path, data):
    with closing(sqlite3.connect(path)) as db, db:
        db.executescript(
            "CREATE TABLE packages(key,bytes,native_bytes,codec,crc);"
            "CREATE TABLE assets(key,kind,package,offset,bytes);"
        )
        db.execute(
            "INSERT INTO packages VALUES (?,?,?,?,?)",
            ("package", data, len(data), 0, zlib.crc32(data)),
        )
        db.execute(
            "INSERT INTO assets VALUES (?,?,?,?,?)",
            ("asset", "ground-region", "package", 0, len(data)),
        )


class TerrainCachedWindow(unittest.TestCase):
    def test_source_words_order_and_original_indices_survive_selection(self):
        data = region([record(x=572089), record(), record(x=572090)])
        selected, context = select_window(data, *WINDOW)
        self.assertEqual([row["page"] for row, _ in selected], ["1", "0"])
        self.assertEqual([row["x"] for row, _ in selected], [572088, 572089])
        self.assertTrue(all(heights == HEIGHTS for _, heights in selected))
        self.assertEqual(context["pages_in_prefix"], 3)
        row, heights, _ = select_page(data)
        self.assertEqual(row, {**SOURCE["row"], "page": "1"})
        self.assertEqual(heights, HEIGHTS)

    def test_missing_page_never_uses_ancestor_or_partial_result(self):
        data = region([record(), record(zoom=19, x=286044, y=181741)])
        with self.assertRaisesRegex(ValueError, "missing"):
            select_window(data, *WINDOW)

    def test_duplicate_and_unsupported_selected_layout_are_refused(self):
        for records, message in (
            ([record(), record(), record(x=572089)], "multiple"),
            ([record(), record(x=572089, side=32)], "unsupported"),
        ):
            with (
                self.subTest(message=message),
                self.assertRaisesRegex(ValueError, message),
            ):
                select_window(region(records), *WINDOW)

    def test_selected_nonfinite_and_late_truncation_are_refused(self):
        bad = struct.pack("<f", np.inf) + HEIGHTS[4:]
        with self.assertRaisesRegex(ValueError, "finite"):
            select_window(region([record(), record(x=572089, heights=bad)]), *WINDOW)
        with self.assertRaisesRegex(ValueError, "truncated"):
            select_window(region([record(), record(x=572089), record()[:-1]]), *WINDOW)

    def test_invalid_and_oversized_windows_are_refused_before_input_read(self):
        for window in (
            (24, 0, 0, 0, 0),
            (20, 2, 1, 0, 0),
            (20, 0, 0, -1, 0),
            (20, 0, 0, 0, 2**20),
            (20, 0, 16, 0, 0),
        ):
            with (
                self.subTest(window=window),
                self.assertRaisesRegex(ValueError, "window|sixteen"),
            ):
                select_window(b"", *window)

    def test_native_posting_fractions_keep_integer_spacing(self):
        row = {**SOURCE["row"], "postings": 100, "virtual": 0, "source_zoom": -1}
        fractions = page_fractions(row)
        self.assertEqual(fractions[1], 3 / 99)
        self.assertEqual(fractions[16], 49 / 99)
        self.assertEqual(fractions[-1], 1)
        np.testing.assert_array_equal(page_fractions(SOURCE["row"]), np.arange(33) / 32)

    def test_export_archive_is_bound_to_input_and_cache_is_unchanged(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.terrain-window."
        ) as tmp:
            folder = Path(tmp)
            database, output = folder / "assets.sqlite", folder / "output"
            cache(database, region([record(), record(x=572089)]))
            before = database.read_bytes()
            result = export(database, "asset", WINDOW, output)
            self.assertEqual(database.read_bytes(), before)
            self.assertEqual(result["height_bytes"], 9800)
            manifest = json.loads((output / "input.json").read_text())
            with tarfile.open(result["archive"], "r:xz") as packed:
                self.assertEqual(len(packed.getmembers()), 5)
                for member in packed.getmembers():
                    self.assertTrue(member.isfile())
                    self.assertEqual(
                        packed.extractfile(member).read(),
                        (output / member.name).read_bytes(),
                    )
            for item in manifest["rows"]:
                heights = (output / item["height_file"]).read_bytes()
                self.assertEqual(heights, HEIGHTS)
                self.assertEqual(
                    hashlib.sha256(heights).hexdigest(), item["height_sha256"]
                )
                fractions = (output / item["fractions_file"]).read_bytes()
                self.assertEqual(
                    hashlib.sha256(fractions).hexdigest(), item["fractions_sha256"]
                )
            with self.assertRaisesRegex(ValueError, "new explicit"):
                export(database, "asset", WINDOW, output)
            self.assertEqual(database.read_bytes(), before)

    def test_missing_input_produces_no_output_directory(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.terrain-window."
        ) as tmp:
            folder = Path(tmp)
            database, output = folder / "assets.sqlite", folder / "output"
            cache(database, region([record()]))
            with self.assertRaisesRegex(ValueError, "missing"):
                export(database, "asset", WINDOW, output)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()

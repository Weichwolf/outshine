"""Native SQLite layout adapters preserve payloads and physical blob locations."""

import sqlite3
import tempfile
import unittest
from contextlib import closing
from pathlib import Path

from native_package_storage import open_packages, prepare_packages


class NativePackages(unittest.TestCase):
    def test_inline_and_streamed_share_the_same_read_contract(self):
        for streamed in (False, True):
            with self.subTest(streamed=streamed), tempfile.TemporaryDirectory() as temp:
                path = Path(temp) / "assets.sqlite"
                with closing(sqlite3.connect(path)) as db:
                    db.execute(
                        "CREATE TABLE packages(key TEXT PRIMARY KEY,bytes BLOB,crc INTEGER,"
                        "codec INTEGER,native_bytes INTEGER)"
                    )
                    db.execute("INSERT INTO packages VALUES('old',?,5,0,3)", (b"old",))
                    if streamed:
                        db.execute("CREATE TABLE package_bodies(key TEXT PRIMARY KEY,bytes BLOB)")
                        db.execute("INSERT INTO packages VALUES('new',X'',7,0,8)")
                        db.execute("INSERT INTO package_bodies VALUES('new',?)", (b"new body",))
                    db.execute(f"PRAGMA user_version={5 if streamed else 4}")
                    db.commit()
                before = path.read_bytes()
                with closing(open_packages(path.as_uri() + "?mode=ro", uri=True)) as db:
                    prepare_packages(db)
                    rows = db.execute(
                        "SELECT key,bytes,stored_bytes,rowid,body_table FROM native_packages ORDER BY key"
                    ).fetchall()
                    self.assertEqual(len(rows), 2 if streamed else 1)
                    for key, payload, size, rowid, table in rows:
                        self.assertEqual(payload, b"old" if key == "old" else b"new body")
                        self.assertEqual(size, len(payload))
                        self.assertEqual(table, "packages" if key == "old" else "package_bodies")
                        with db.blobopen(table, "bytes", rowid, readonly=True) as blob:
                            self.assertEqual(blob.read(), payload)
                self.assertEqual(path.read_bytes(), before)

    def test_early_legacy_columns_remain_available(self):
        with closing(sqlite3.connect(":memory:")) as db:
            db.execute("CREATE TABLE packages(key TEXT PRIMARY KEY,bytes BLOB,crc INTEGER)")
            db.execute("INSERT INTO packages VALUES('raw',X'010203',19)")
            prepare_packages(db)
            self.assertEqual(
                db.execute("SELECT key,bytes,crc FROM native_packages").fetchone(),
                ("raw", b"\x01\x02\x03", 19),
            )


if __name__ == "__main__":
    unittest.main()

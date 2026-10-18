"""Exercise native cache maintenance on independent SQLite and file fixtures."""

import hashlib
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
CLIENT = ROOT / 'build/outshine-client'
MIB = 1024 * 1024
SCHEMA = '''
PRAGMA user_version=3;
CREATE TABLE assets(id INTEGER PRIMARY KEY,key TEXT UNIQUE,kind TEXT,
minx REAL,miny REAL,minz REAL,maxx REAL,maxy REAL,maxz REAL,package TEXT,
offset INTEGER,bytes INTEGER,level INTEGER,parent TEXT,request_key TEXT);
CREATE VIRTUAL TABLE bounds USING rtree(id,minx,maxx,miny,maxy,minz,maxz);
CREATE TABLE packages(key TEXT PRIMARY KEY,bytes BLOB,crc INTEGER,codec INTEGER,native_bytes INTEGER);
CREATE TABLE cache_usage_v1(key TEXT PRIMARY KEY,stored_bytes INTEGER,
created_unix_s INTEGER,last_used_unix_s INTEGER);
'''


def key(value):
    return hashlib.sha256(value.encode()).hexdigest()


def assets(path, products):
    path.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(path) as db:
        db.executescript(SCHEMA)
        for name, size, members, used in products:
            payload = bytes([len(name)]) * size
            identity = key(name)
            db.execute('INSERT INTO packages VALUES(?,?,?,?,?)',
                       (identity, payload, zlib.crc32(payload), 0, size))
            if used is not None:
                db.execute('INSERT INTO cache_usage_v1 VALUES(?,?,?,?)',
                           (identity, size, 1, used))
            for i in range(members):
                row = db.execute('INSERT INTO assets(key,kind,minx,miny,minz,maxx,maxy,maxz,'
                                 'package,offset,bytes,level,parent,request_key) '
                                 'VALUES(?,?,0,0,0,1,1,1,?,0,?,0,?,?)',
                                 (key(f'{name}-{i}'), 'fixture', identity, size, '', key(f'request-{name}-{i}')))
                db.execute('INSERT INTO bounds VALUES(?,0,1,0,1,0,1)', (row.lastrowid,))


def scalar(path, query):
    with sqlite3.connect(f'{path.as_uri()}?mode=ro', uri=True) as db:
        return db.execute(query).fetchone()[0]


class CacheCommand(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='outshine-cache-command-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.world = self.root / 'assets/assets.sqlite'
        self.prototypes = self.root / 'assets/prototypes.sqlite'
        self.env = dict(os.environ, SDL_VIDEODRIVER='outshine-intentionally-unavailable')

    def run_cache(self, *arguments, status=0):
        result = subprocess.run([str(CLIENT), 'cache', 'trim', '--root', str(self.root), *arguments],
                                env=self.env, capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, status, result.stdout + result.stderr)
        self.assertNotIn('SDL did not start', result.stdout + result.stderr)
        rows = {}
        for line in result.stdout.splitlines():
            fields = line.split('\t')
            if fields[0] == 'CACHE':
                rows[fields[1]] = dict(item.split('=', 1) for item in fields[2:])
        return result, rows

    def fixture(self):
        assets(self.world, [('old', MIB // 2, 2, 10), ('new', MIB // 2, 1, 20),
                            ('orphan', MIB // 4, 0, None)])
        assets(self.prototypes, [('prototype', MIB // 2, 1, 30)])

    def test_argument_validation_precedes_platform_and_storage(self):
        for arguments in [(), ('--assets-mib',), ('--sources-mib', '-1'),
                          ('--assets-mib', '1.5'), ('--assets-mib', 'nan'),
                          ('--assets-mib', '18446744073709551615'),
                          ('--assets-mib', '1', '--assets-mib', '2'), ('--unknown',)]:
            with self.subTest(arguments=arguments):
                self.run_cache(*arguments, status=2)
        self.assertEqual(list(self.root.iterdir()), [])
        help_result, _ = self.run_cache('--help')
        self.assertIn('--sources-mib', help_result.stdout)

    def test_preview_preserves_payloads_and_database(self):
        self.fixture()
        before = {p: p.read_bytes() for p in (self.world, self.prototypes)}
        _, rows = self.run_cache('--assets-mib', '1')
        self.assertEqual(int(rows['assets']['stored_bytes']), MIB * 7 // 4)
        self.assertEqual(int(rows['assets']['retained_bytes']), MIB)
        self.assertEqual(rows['assets']['packages_selected'], '2')
        self.assertEqual(rows['assets']['entries_selected'], '2')
        self.assertEqual(rows['assets']['unknown_use'], '1')
        self.assertEqual(rows['assets']['mode'], 'preview')
        for path, data in before.items():
            self.assertEqual(path.read_bytes(), data)

    def test_budget_is_shared_across_databases_and_packages_are_atomic(self):
        self.fixture()
        self.run_cache('--assets-mib', '1', '--apply')
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM packages'), 1)
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM assets'), 1)
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM bounds'), 1)
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM cache_usage_v1'), 1)
        self.assertEqual(scalar(self.world, "SELECT count(*) FROM assets WHERE request_key<>''"), 1)
        self.assertEqual(scalar(self.prototypes, 'SELECT count(*) FROM packages'), 1)
        _, rows = self.run_cache('--assets-mib', '1', '--apply')
        self.assertEqual(rows['assets']['packages_selected'], '0')

    def test_zero_budget_compacts_real_files(self):
        self.fixture()
        before = sum(p.stat().st_size for p in (self.world, self.prototypes))
        _, rows = self.run_cache('--assets-mib', '0', '--apply', '--compact')
        self.assertEqual(rows['assets']['retained_bytes'], '0')
        after = sum(p.stat().st_size for p in (self.world, self.prototypes))
        self.assertLess(after, before)
        for path in (self.world, self.prototypes):
            self.assertEqual(scalar(path, 'SELECT count(*) FROM assets'), 0)
            self.assertEqual(scalar(path, 'SELECT count(*) FROM bounds'), 0)
            self.assertEqual(scalar(path, 'PRAGMA integrity_check'), 'ok')

    def test_sources_use_usage_metadata_and_keep_foreign_files(self):
        directory = self.root / 'sources'
        directory.mkdir()
        for name, stamp in [('a', 3000), ('b', 1000), ('c', 5000)]:
            path = directory / key(name)
            path.write_bytes(b'x' * (MIB // 2))
            os.utime(path, (stamp, stamp))
        index = directory / '.usage.sqlite'
        with sqlite3.connect(index) as db:
            db.execute('CREATE TABLE cache_usage_v1(key TEXT PRIMARY KEY,stored_bytes INTEGER,'
                       'created_unix_s INTEGER,last_used_unix_s INTEGER)')
            db.executemany('INSERT INTO cache_usage_v1 VALUES(?,?,?,?)',
                           [(key('a'), MIB // 2, 1, 10), (key('b'), MIB // 2, 1, 20),
                            (key('gone'), 3, 1, 1)])
        foreign = directory / 'notes.txt'
        foreign.write_text('keep')
        target = self.root / 'outside'
        target.write_text('keep outside')
        (directory / key('link')).symlink_to(target)
        absence = directory / '.outshine-absence-v1'
        absence.mkdir()
        marker = absence / key('absent')
        marker.write_text('outshine-absence-v2\n')
        os.utime(marker, (1, 1))
        self.run_cache('--sources-mib', '1')
        self.assertTrue((directory / key('a')).exists())
        self.run_cache('--sources-mib', '1', '--apply', '--compact')
        self.assertFalse((directory / key('a')).exists())
        self.assertTrue((directory / key('b')).exists())
        self.assertFalse(marker.exists())
        self.assertEqual(scalar(index, 'SELECT count(*) FROM cache_usage_v1'), 1)
        self.run_cache('--sources-mib', '0', '--apply')
        self.assertTrue(foreign.exists())
        self.assertTrue((directory / key('link')).is_symlink())
        self.assertEqual(target.read_text(), 'keep outside')
        self.assertEqual(scalar(index, 'SELECT count(*) FROM cache_usage_v1'), 0)

    def test_unknown_schema_fails_before_any_asset_changes(self):
        self.fixture()
        with sqlite3.connect(self.prototypes) as db:
            db.execute('PRAGMA user_version=100')
        before = self.world.read_bytes()
        result, _ = self.run_cache('--assets-mib', '0', '--apply', status=1)
        self.assertIn('unsupported', result.stderr)
        self.assertEqual(before, self.world.read_bytes())

    def test_busy_writer_and_symlink_do_not_delete_data(self):
        self.fixture()
        db = sqlite3.connect(self.prototypes)
        try:
            db.execute('BEGIN IMMEDIATE')
            result, _ = self.run_cache('--assets-mib', '0', '--apply', status=1)
            self.assertIn('locked', result.stderr)
        finally:
            db.rollback()
            db.close()
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM packages'), 3)
        outside = self.root / 'outside.sqlite'
        self.prototypes.rename(outside)
        self.prototypes.symlink_to(outside)
        self.run_cache('--assets-mib', '0', '--apply', status=1)
        self.assertEqual(scalar(outside, 'SELECT count(*) FROM packages'), 1)

    def test_unknown_source_index_fails_before_file_removal(self):
        directory = self.root / 'sources'
        directory.mkdir()
        path = directory / key('data')
        path.write_bytes(b'keep')
        with sqlite3.connect(directory / '.usage.sqlite') as db:
            db.execute('CREATE TABLE unrelated(value TEXT)')
        self.run_cache('--sources-mib', '0', '--apply', status=1)
        self.assertTrue(path.exists())

    def test_missing_caches_are_an_empty_preview_without_creation(self):
        _, rows = self.run_cache('--assets-mib', '0', '--sources-mib', '0')
        self.assertEqual(rows['assets']['stored_bytes'], '0')
        self.assertEqual(rows['sources']['stored_bytes'], '0')
        self.assertEqual(list(self.root.iterdir()), [])

    def test_legacy_packages_and_zero_length_entries_can_be_cleared(self):
        assets(self.world, [('legacy', MIB // 2, 1, None), ('empty', 0, 1, None)])
        with sqlite3.connect(self.world) as db:
            db.execute('DROP TABLE cache_usage_v1')
        _, rows = self.run_cache('--assets-mib', '0', '--apply')
        self.assertEqual(rows['assets']['unknown_use'], '2')
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM packages'), 0)
        directory = self.root / 'sources'
        directory.mkdir()
        (directory / key('empty')).touch()
        self.run_cache('--sources-mib', '0', '--apply')
        self.assertFalse((directory / key('empty')).exists())

    def test_failed_package_delete_rolls_back_members_and_bounds(self):
        self.fixture()
        with sqlite3.connect(self.prototypes) as db:
            db.execute("CREATE TRIGGER reject_trim BEFORE DELETE ON packages "
                       "BEGIN SELECT RAISE(ABORT,'injected package deletion failure');END")
        self.run_cache('--assets-mib', '0', '--apply', status=1)
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM packages'), 3)
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM assets'), 3)
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM bounds'), 3)
        self.assertEqual(scalar(self.prototypes, 'SELECT count(*) FROM assets'), 1)

    def test_active_wal_reader_prevents_a_false_compaction_success(self):
        assets(self.world, [('old', MIB, 1, 1)])
        db = sqlite3.connect(self.world)
        try:
            db.execute('PRAGMA journal_mode=WAL')
            db.execute('BEGIN')
            db.execute('SELECT count(*) FROM assets').fetchone()
            result, _ = self.run_cache('--assets-mib', '0', '--apply', '--compact', status=1)
            self.assertIn('cache trimmed', result.stderr)
        finally:
            db.rollback()
            db.close()
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM packages'), 0)
        self.run_cache('--assets-mib', '0', '--apply', '--compact')

    def test_invalid_usage_metadata_never_deletes_payloads(self):
        self.fixture()
        with sqlite3.connect(self.world) as db:
            db.execute("UPDATE cache_usage_v1 SET last_used_unix_s='invalid'")
        self.run_cache('--assets-mib', '0', '--apply', status=1)
        self.assertEqual(scalar(self.world, 'SELECT count(*) FROM packages'), 3)


if __name__ == '__main__':
    unittest.main()

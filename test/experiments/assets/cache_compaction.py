"""Compare SQLite reclamation, including a pinned WAL reader; no shared cache writes."""

import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import tempfile
import time


PACKAGE_BYTES = 512 * 1024
PACKAGE_COUNT = 24
BATCH_PAGES = 256


def scalar(db, sql):
    return db.execute(sql).fetchone()[0]


def snapshot(db, path):
    files = [Path(str(path) + suffix) for suffix in ('', '-wal', '-shm')]
    return {
        'pages': scalar(db, 'PRAGMA page_count'),
        'free_pages': scalar(db, 'PRAGMA freelist_count'),
        'file_bytes': sum(p.stat().st_size for p in files if p.exists()),
        'allocated_bytes': sum(p.stat().st_blocks * 512 for p in files if p.exists()),
        'wal_bytes': files[1].stat().st_size if files[1].exists() else 0,
    }


def digest(db):
    result = hashlib.sha256()
    for identity, payload in db.execute('SELECT id,bytes FROM packages ORDER BY id'):
        result.update(identity.to_bytes(4, 'little'))
        result.update(payload)
    return result.hexdigest()


def reclaim(db, path):
    checkpoint = db.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()
    if checkpoint[0]:
        return {'deferred': True, 'steps': [], 'checkpoint': checkpoint}
    steps = []
    free = scalar(db, 'PRAGMA freelist_count')
    for _ in range(free + 1):
        if not free:
            return {'deferred': False, 'steps': steps}
        before = snapshot(db, path)
        start = time.perf_counter_ns()
        db.execute(f'PRAGMA incremental_vacuum({BATCH_PAGES})').fetchall()
        checkpoint = db.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()
        elapsed = time.perf_counter_ns() - start
        after = snapshot(db, path)
        assert not checkpoint[0], checkpoint
        assert 0 < free - after['free_pages'] <= BATCH_PAGES, (before, after)
        assert after['file_bytes'] < before['file_bytes'], (before, after)
        steps.append({'before': before, 'after': after, 'ns': elapsed})
        free = after['free_pages']
    raise AssertionError('reclamation did not converge')


def exercise(directory, mode):
    path = directory / f'{mode}.sqlite'
    db = sqlite3.connect(path, timeout=0)
    reader = None
    try:
        db.execute(f'PRAGMA auto_vacuum={mode}')
        db.execute('PRAGMA journal_mode=WAL')
        db.execute('PRAGMA synchronous=NORMAL')
        db.execute('PRAGMA wal_autocheckpoint=0')
        db.execute('CREATE TABLE packages(id INTEGER PRIMARY KEY,bytes BLOB NOT NULL)')
        for i in range(PACKAGE_COUNT):
            data = hashlib.shake_256(str(i).encode()).digest(PACKAGE_BYTES)
            db.execute('INSERT INTO packages VALUES(?,?)', (i, data))
        db.commit()
        assert not db.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()[0]
        before = snapshot(db, path)
        original = digest(db)
        reader = sqlite3.connect(path, timeout=0)
        reader.execute('BEGIN')
        assert digest(reader) == original
        start = time.perf_counter_ns()
        db.execute('DELETE FROM packages WHERE id % 3 != 0')
        db.commit()
        delete_ns = time.perf_counter_ns() - start
        deleted = snapshot(db, path)
        preserved = digest(db)
        if mode == 'INCREMENTAL':
            blocked = reclaim(db, path)
            assert blocked['deferred'] and not blocked['steps']
            assert snapshot(db, path) == deleted
        else:
            checkpoint = db.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()
            assert checkpoint[0]
            blocked = {'deferred': True, 'checkpoint': checkpoint}
        assert digest(reader) == original
        reader.rollback()
        reader.close()
        reader = None
        start = time.perf_counter_ns()
        if mode == 'INCREMENTAL':
            reclaimed = reclaim(db, path)
        else:
            assert not db.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()[0]
            after_checkpoint = snapshot(db, path)
            if mode == 'NONE':
                assert after_checkpoint['pages'] == before['pages']
                assert after_checkpoint['free_pages'] > 0
                db.execute('VACUUM')
                assert not db.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()[0]
            reclaimed = {'deferred': False, 'after_checkpoint': after_checkpoint}
        reclaim_ns = time.perf_counter_ns() - start
        assert digest(db) == preserved
        assert scalar(db, 'PRAGMA integrity_check') == 'ok'
        after = snapshot(db, path)
        assert after['free_pages'] == 0
        assert after['file_bytes'] < before['file_bytes'] // 2
        return {'mode': mode, 'before': before, 'deleted': deleted, 'blocked': blocked,
                'reclaim': reclaimed, 'after': after, 'delete_ns': delete_ns,
                'reclaim_ns': reclaim_ns, 'retained_sha256': preserved}
    finally:
        if reader is not None:
            reader.close()
        db.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='outshine-master-cache-compaction-') as temporary:
        results = [exercise(Path(temporary), mode) for mode in ('NONE', 'FULL', 'INCREMENTAL')]
    assert len({case['retained_sha256'] for case in results}) == 1
    report = {'sqlite': sqlite3.sqlite_version, 'package_bytes': PACKAGE_BYTES,
              'packages': PACKAGE_COUNT, 'batch_pages': BATCH_PAGES, 'cases': results}
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    for case in results:
        print(f"{case['mode']}: {case['before']['file_bytes']} -> "
              f"{case['after']['file_bytes']} bytes; "
              f"{len(case['reclaim'].get('steps', []))} bounded steps; pinned reader deferred")


if __name__ == '__main__':
    main()

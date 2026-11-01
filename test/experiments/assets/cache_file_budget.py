"""Exercise real SQLite growth under one file budget; no shared-cache writes."""

import argparse
import ctypes as C
import hashlib
import json
import multiprocessing as mp
import os
from pathlib import Path
import sqlite3
import tempfile
import time

from sqlite_budget_vfs import BudgetVfs, FULL, OK, SIZE_HINT, CHUNK_SIZE


LIMIT = 12 * 1024**2
RESERVE = 1024**2
PAYLOAD = 128 * 1024


def data(identity):
    return hashlib.shake_256(str(identity).encode()).digest(PAYLOAD)


def kind(error):
    code = getattr(error, 'sqlite_errorcode', None)
    if code is not None:
        return code & 255
    if str(error) == 'database or disk is full':
        return FULL
    if 'locked' in str(error):
        return 5
    raise error


def schema(db):
    db.execute('PRAGMA auto_vacuum=INCREMENTAL')
    db.execute('PRAGMA journal_mode=WAL')
    db.execute('PRAGMA synchronous=NORMAL')
    db.execute('PRAGMA wal_autocheckpoint=0')
    db.execute('CREATE TABLE IF NOT EXISTS packages(id INTEGER PRIMARY KEY, '
               'last_used INTEGER NOT NULL, bytes BLOB NOT NULL)')
    db.execute('CREATE INDEX IF NOT EXISTS package_use ON packages(last_used,id)')
    db.commit()


def publish(db, identity, used=None, payload=None):
    try:
        db.execute('INSERT INTO packages VALUES(?,?,?)',
                   (identity, identity if used is None else used,
                    data(identity) if payload is None else payload))
        db.commit()
        return True
    except sqlite3.Error as error:
        db.rollback()
        if kind(error) not in (FULL, 5):
            raise
        return False


def digest(db):
    result = hashlib.sha256()
    for identity, used, body in db.execute('SELECT id,last_used,bytes FROM packages ORDER BY id'):
        result.update(f'{identity}:{used}:'.encode())
        result.update(body)
    return result.hexdigest()


def verify(db):
    assert db.execute('PRAGMA integrity_check').fetchone() == ('ok',)
    assert not db.execute('PRAGMA foreign_key_check').fetchall()


def reclaim(db, budget):
    with budget.maintenance():
        checkpoint = db.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()
        if checkpoint[0]:
            return False, 0
        free = db.execute('PRAGMA freelist_count').fetchone()[0]
        steps = 0
        while free:
            assert steps < 4096, 'bounded reclamation did not converge'
            db.execute('PRAGMA incremental_vacuum(32)').fetchall()
            checkpoint = db.execute('PRAGMA wal_checkpoint(TRUNCATE)').fetchone()
            assert not checkpoint[0], checkpoint
            following = db.execute('PRAGMA freelist_count').fetchone()[0]
            assert 0 < free - following <= 32, (free, following)
            free = following
            steps += 1
        return True, steps


def pinned_and_reclaim(root):
    budget = BudgetVfs(root, LIMIT, RESERVE)
    db = budget.connect('assets.sqlite')
    reader = None
    try:
        schema(db)
        for identity in range(8):
            assert publish(db, identity, used=1000000 if identity == 0 else identity)
        assert reclaim(db, budget)[0]
        original = digest(db)
        reader = budget.connect('assets.sqlite')
        reader.execute('BEGIN')
        assert digest(reader) == original
        accepted = 8
        for identity in range(8, 128):
            if not publish(db, identity):
                break
            accepted += 1
        else:
            raise AssertionError('pinned WAL must meet backpressure before 128 packages')
        assert digest(reader) == original, 'pinned input snapshot changed'
        committed = digest(db)
        assert db.execute('SELECT count(*) FROM packages').fetchone()[0] == accepted
        assert not publish(db, 9999, payload=b'X' * LIMIT)
        assert digest(db) == committed, 'failed oversized write changed committed products'
        before = sum(budget.snapshot().values())
        assert not reclaim(db, budget)[0], 'pinned reader cannot be silently dropped'
        assert digest(reader) == original
        reader.rollback()
        reader.close()
        reader = None
        assert reclaim(db, budget)[0], 'reserved checkpoint space must allow recovery'
        checkpointed = sum(budget.snapshot().values())
        assert digest(db) == committed
        oldest = [row[0] for row in db.execute(
            'SELECT id FROM packages ORDER BY last_used,id LIMIT 3')]
        assert oldest == [1, 2, 3], oldest
        with budget.maintenance():
            db.executemany('DELETE FROM packages WHERE id=?', [(key,) for key in oldest])
            db.commit()
        ok, steps = reclaim(db, budget)
        assert ok and steps
        assert db.execute('SELECT count(*) FROM packages WHERE id=0').fetchone()[0] == 1
        assert publish(db, 10000), 'publication must resume after reader release and eviction'
        assert reclaim(db, budget)[0]
        verify(db)
        return {'accepted_before_pressure': accepted, 'pinned_file_bytes': before,
                'checkpointed_file_bytes': checkpointed, 'reclaim_steps': steps,
                'stats': budget.stats}
    finally:
        if reader is not None:
            reader.close()
        db.close()
        budget.close()


def writer(root, name, start, ready, output):
    budget = BudgetVfs(root, LIMIT, RESERVE)
    db = budget.connect(name)
    try:
        db.execute('PRAGMA wal_autocheckpoint=0')
        ready.put(name)
        if not start.wait(10):
            raise TimeoutError('writer start signal')
        accepted = 0
        for identity in range(128):
            if not publish(db, identity):
                break
            accepted += 1
        verify(db)
    finally:
        db.close()
        budget.close()
    output.put({'store': name, 'accepted': accepted, 'stats': budget.stats})


def parallel(root):
    budget = BudgetVfs(root, LIMIT, RESERVE)
    try:
        for name in ('assets.sqlite', 'prototypes.sqlite'):
            db = budget.connect(name)
            try:
                schema(db)
            finally:
                db.close()
    finally:
        budget.close()
    context = mp.get_context('spawn')
    start, ready, output = context.Event(), context.Queue(), context.Queue()
    workers = [context.Process(target=writer, args=(root, name, start, ready, output))
               for name in ('assets.sqlite', 'prototypes.sqlite')]
    try:
        for worker in workers:
            worker.start()
        assert len({ready.get(timeout=15) for _ in workers}) == len(workers)
        start.set()
        results = [output.get(timeout=30) for _ in workers]
        for worker in workers:
            worker.join(15)
            assert worker.exitcode == 0, worker.exitcode
        budget = BudgetVfs(root, LIMIT, RESERVE)
        try:
            for result in results:
                db = budget.connect(result['store'])
                try:
                    verify(db)
                    assert db.execute('SELECT count(*) FROM packages').fetchone()[0] == result['accepted']
                finally:
                    db.close()
            assert sum(budget.snapshot().values()) <= LIMIT
        finally:
            budget.close()
        return results
    finally:
        start.set()
        for worker in workers:
            if worker.is_alive():
                worker.terminate()
                worker.join(5)
        ready.close()
        output.close()


def crash_writer(root):
    budget = BudgetVfs(root, LIMIT, RESERVE)
    db = budget.connect('assets.sqlite')
    schema(db)
    for identity in range(4):
        assert publish(db, identity)
    db.execute('PRAGMA cache_size=16')
    db.execute('INSERT INTO packages VALUES(99,99,?)', (data(99) * 8,))
    os._exit(0)


def recovery(root):
    context = mp.get_context('spawn')
    process = context.Process(target=crash_writer, args=(root,))
    process.start()
    try:
        process.join(15)
        assert process.exitcode == 0, process.exitcode
        budget = BudgetVfs(root, LIMIT, RESERVE)
        db = budget.connect('assets.sqlite')
        try:
            verify(db)
            assert [row[0] for row in db.execute('SELECT id FROM packages ORDER BY id')] == list(range(4))
            assert reclaim(db, budget)[0]
            assert publish(db, 5), 'restart must make forward progress after an abandoned write'
            verify(db)
            return {'recovered_products': 4, 'stats': budget.stats}
        finally:
            db.close()
            budget.close()
    finally:
        if process.is_alive():
            process.terminate()
            process.join(5)



class HardLimitOnly(BudgetVfs):
    def grow(self, path, end, call, reserve_checkpoint=False):
        return super().grow(path, end, call, reserve_checkpoint=False)


def without_checkpoint_reserve(root):
    budget = HardLimitOnly(root, LIMIT, RESERVE)
    db = budget.connect('assets.sqlite')
    reader = None
    try:
        schema(db)
        for identity in range(8):
            assert publish(db, identity)
        assert reclaim(db, budget)[0]
        reader = budget.connect('assets.sqlite')
        reader.execute('BEGIN')
        initial = digest(reader)
        accepted = 8
        for identity in range(8, 128):
            if not publish(db, identity):
                break
            accepted += 1
        else:
            raise AssertionError('hard-only cache must reach capacity')
        assert digest(reader) == initial
        committed = digest(db)
        reader.close()
        reader = None
        failed = False
        try:
            reclaim(db, budget)
        except sqlite3.Error as error:
            assert kind(error) == FULL, error
            failed = True
        assert failed, 'a hard limit alone must expose the checkpoint headroom failure'
        assert digest(db) == committed
        verify(db)
        return {'accepted': accepted, 'checkpoint_blocked_after_reader_release': True,
                'stats': budget.stats}
    finally:
        if reader is not None:
            reader.close()
        db.close()
        budget.close()


def entrypoints(root):
    budget = BudgetVfs(root, LIMIT, RESERVE)
    db = budget.connect('assets.sqlite')
    try:
        schema(db)
        assert publish(db, 1)
        committed = digest(db)
        main = next(file for file, entry in budget.files.items()
                    if entry['path'].name == 'assets.sqlite')
        before = budget.snapshot()
        hint = C.c_int64(2 * LIMIT)
        chunk = C.c_int(2 * LIMIT)
        assert budget.control(main, SIZE_HINT, C.byref(hint)) == OK
        assert budget.control(main, CHUNK_SIZE, C.byref(chunk)) == OK
        assert budget.snapshot() == before
        assert budget.truncate(main, LIMIT + 4096) == FULL
        byte = C.create_string_buffer(b'X')
        assert budget.write(main, byte, 1, LIMIT) == FULL
        pointer = C.c_void_p()
        assert budget.shm_map(main, 4096, 32768, 1, C.byref(pointer)) == FULL
        assert budget.snapshot() == before
        assert digest(db) == committed
        verify(db)
        return {'write_truncate_shm_denied': 3, 'preallocation_hints_ignored': 2,
                'stats': budget.stats}
    finally:
        db.close()
        budget.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    began = time.perf_counter_ns()
    with tempfile.TemporaryDirectory(prefix='outshine-master-1920-budget-') as tmp:
        root = Path(tmp)
        source = root / 'source.keep'
        source.write_bytes(data(-1))
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        report = {'sqlite': sqlite3.sqlite_version, 'limit_bytes': LIMIT,
                  'maintenance_reserve_bytes': RESERVE,
                  'pinned': pinned_and_reclaim(root / 'pinned'),
                  'hard_only_counterexample': without_checkpoint_reserve(root / 'hard-only'),
                  'entrypoints': entrypoints(root / 'entrypoints'),
                  'parallel': parallel(root / 'parallel'),
                  'recovery': recovery(root / 'recovery')}
        assert hashlib.sha256(source.read_bytes()).hexdigest() == source_hash
    report['elapsed_ns'] = time.perf_counter_ns() - began
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'output': str(args.output), 'elapsed_ms': report['elapsed_ns'] / 1e6,
                      'pinned': report['pinned']['accepted_before_pressure'],
                      'parallel': [row['accepted'] for row in report['parallel']]}))


if __name__ == '__main__':
    main()

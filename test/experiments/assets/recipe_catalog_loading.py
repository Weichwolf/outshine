import argparse
import json
import os
from pathlib import Path
import resource
import sqlite3
import subprocess
import sys
import time


def recipe(key):
    return {'id': key, 'family': key % 2000, 'region': key % 256,
            'dependencies': [0, 1] if key >= 2 else [],
            'operators': [['box', [1, 1, 1]], ['surface', 'concrete']]}


def build(folder, count):
    folder.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(folder / 'recipes.sqlite')
    db.execute('create table recipes (id integer primary key, family integer, region integer, payload text)')
    start = time.perf_counter()
    with open(folder / 'recipes.json', 'w') as stream:
        stream.write('[')
        for first in range(0, count, 1000):
            batch = []
            for key in range(first, min(count, first + 1000)):
                text = json.dumps(recipe(key), separators=(',', ':'))
                stream.write((',' if key else '') + text)
                batch.append((key, key % 2000, key % 256, text))
            db.executemany('insert into recipes values (?,?,?,?)', batch)
        stream.write(']')
    db.execute('create index selection on recipes (family, region)')
    db.commit()
    db.close()
    return {'count': count, 'build_s': time.perf_counter() - start,
            'bytes': {p.name: p.stat().st_size for p in folder.iterdir()}}


def consume(folder, count, mode):
    baseline = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    start = time.perf_counter()
    if mode == 'sqlite':
        db = sqlite3.connect((folder / 'recipes.sqlite').resolve().as_uri() + '?mode=ro&immutable=1', uri=True)
        def load(key):
            row = db.execute('select payload from recipes where id=?', (key,)).fetchone()
            if row is None:
                raise ValueError('missing recipe')
            return json.loads(row[0])
        decoded = 0
    else:
        with open(folder / 'recipes.json') as stream:
            rows = json.load(stream)
        def load(key):
            return rows[key]
        decoded = len(rows)
    seeds = [2 + at * (count - 3) // 63 for at in range(64)]
    pending, resolved = seeds[:], {}
    while pending:
        key = pending.pop()
        if key in resolved:
            continue
        if len(resolved) >= 128:
            raise ValueError('closure budget exceeded')
        row = load(key)
        assert row['id'] == key
        resolved[key] = row
        pending.extend(row['dependencies'])
    if mode == 'sqlite':
        decoded = len(resolved)
        db.close()
    elapsed = time.perf_counter() - start
    assert len(resolved) == 66
    scale = 1 if sys.platform == 'darwin' else 1024
    import hashlib
    digest = hashlib.sha256(json.dumps(sorted(resolved.items()), sort_keys=True).encode()).hexdigest()
    return {'mode': mode, 'count': count, 'elapsed_ms': elapsed * 1000,
            'decoded_entries': decoded, 'closure_entries': len(resolved), 'digest': digest,
            'peak_rss_bytes': resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * scale,
            'baseline_peak_rss_bytes': baseline * scale}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('output', type=Path)
    parser.add_argument('--worker', choices=['sqlite', 'json'])
    parser.add_argument('--count', type=int)
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(consume(args.output, args.count, args.worker)))
        return
    report = {'scope': 'synthetic indexed lookup vs full JSON decode; not grammar evaluation',
              'cache_state': 'fresh processes; OS cache uncontrolled/warm; no cold SSD claim',
              'python': sys.version, 'runs': []}
    for count in [10000, 100000, 1000000]:
        folder = args.output / str(count)
        entry = build(folder, count)
        entry['consumers'] = []
        for repeat in range(3):
            for mode in ['sqlite', 'json']:
                raw = subprocess.check_output([sys.executable, __file__, str(folder), '--worker', mode, '--count', str(count)])
                result = json.loads(raw)
                result['repeat'] = repeat
                entry['consumers'].append(result)
        assert len({r['digest'] for r in entry['consumers']}) == 1
        report['runs'].append(entry)
        (args.output / 'report.json').write_text(json.dumps(report, indent=2))
        print(json.dumps({'count': count, 'build_s': entry['build_s'], 'bytes': entry['bytes']}), flush=True)
        for path in folder.iterdir():
            path.unlink()
        folder.rmdir()


if __name__ == '__main__':
    main()

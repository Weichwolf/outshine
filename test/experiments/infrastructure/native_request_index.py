"""Compare request bindings using real cached metadata and synthetic request identities."""
from pathlib import Path
import argparse
import hashlib
import json
import sqlite3
import statistics
import time


def measure(connection, sql, requests):
    samples = []
    for _ in range(7):
        started = time.perf_counter_ns()
        for request, expected in requests:
            row = connection.execute(sql, (request,)).fetchone()
            assert row == (expected,)
        samples.append((time.perf_counter_ns() - started) / len(requests) / 1000)
    return dict(median_us=statistics.median(samples), samples_us=samples)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=Path.home() /
                        "Library/Application Support/outshine/outshine/assets/assets.sqlite")
    args = parser.parse_args()
    with sqlite3.connect(f"file:{args.cache}?mode=ro", uri=True) as source:
        rows = source.execute("SELECT id,key,kind FROM assets ORDER BY id").fetchall()
    chosen = [row for row in rows if row[2] == "building-basis"]
    assert chosen
    requests = [(hashlib.sha256(f"synthetic-request-{row[0]}".encode()).hexdigest(), row[1])
                for row in chosen]
    with sqlite3.connect(":memory:") as connection:
        connection.executescript(
            "CREATE TABLE assets(id INTEGER PRIMARY KEY,key TEXT NOT NULL UNIQUE,"
            "kind TEXT NOT NULL,request TEXT NOT NULL DEFAULT '');"
            "CREATE UNIQUE INDEX requests ON assets(request) WHERE request<>'';"
            "CREATE TABLE bindings(request TEXT PRIMARY KEY,asset INTEGER NOT NULL);")
        connection.executemany("INSERT INTO assets(id,key,kind) VALUES(?,?,?)", rows)
        connection.executemany("UPDATE assets SET request=? WHERE key=?", requests)
        connection.executemany("INSERT INTO bindings VALUES(?,?)",
                               [(request, row[0]) for (request, _), row in zip(requests, chosen)])
        queries = {
            "record_column": "SELECT key FROM assets WHERE request=? AND request<>''",
            "binding_table": "SELECT a.key FROM bindings r JOIN assets a ON a.id=r.asset "
                             "WHERE r.request=?",
        }
        result = dict(cached_records=len(rows), requests=len(requests),
                      identity="synthetic; this measures lookup, not generator identity or runtime",
                      payload_bytes_loaded=0, alternatives={})
        for name, query in queries.items():
            measured = measure(connection, query, requests)
            measured["plan"] = [row[3] for row in connection.execute(
                "EXPLAIN QUERY PLAN " + query, (requests[0][0],))]
            result["alternatives"][name] = measured
        print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

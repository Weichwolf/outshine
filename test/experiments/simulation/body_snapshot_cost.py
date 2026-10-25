"""Compare immutable pose payload copies; Python timings are not native budgets."""

import argparse
import json
import time

import numpy as np


def publish(source, previous, dirty, page_rows):
    if not len(dirty):
        return previous, 0, 0
    if page_rows == 0:
        return source.copy(), source.nbytes, 1
    touched = np.unique(dirty // page_rows)
    pages = list(previous)
    copied = 0
    for page in touched:
        block = source[page * page_rows : (page + 1) * page_rows].copy()
        pages[page] = block
        copied += block.nbytes
    return tuple(pages), copied, len(touched)


def case(rows, changed, scattered, page_rows, frames):
    source = np.arange(rows * 24, dtype=np.uint64).reshape(rows, 24)
    initial = source.copy()
    dirty = (
        np.linspace(0, rows - 1, changed, dtype=np.int64)
        if scattered
        else np.arange(changed)
    )
    previous = (
        initial
        if not page_rows
        else tuple(initial[a : a + page_rows].copy() for a in range(0, rows, page_rows))
    )
    held = previous
    copied = blocks = 0
    start = time.perf_counter_ns()
    for _ in range(frames):
        source[dirty, 0] += 1
        previous, size, count = publish(source, previous, dirty, page_rows)
        copied += size
        blocks += count
    elapsed = time.perf_counter_ns() - start
    actual = previous if not page_rows else np.concatenate(previous)
    old = held if not page_rows else np.concatenate(held)
    np.testing.assert_array_equal(actual, source)
    np.testing.assert_array_equal(old, initial)
    assert changed or previous is held
    return {
        "rows": rows,
        "changed": changed,
        "scattered": scattered,
        "page_rows": page_rows,
        "frames": frames,
        "payload_bytes_per_publish": copied // frames,
        "payload_blocks_per_publish": blocks / frames,
        "directory_slots_per_publish": (len(previous) if changed and page_rows else 0),
        "python_us_per_publish": elapsed / frames / 1000,
    }


def retention_adversary():
    rows_per_page, pages = 64, 16
    source = np.zeros((rows_per_page * pages, 24), dtype=np.uint64)
    independent = tuple(
        source[a : a + rows_per_page].copy()
        for a in range(0, len(source), rows_per_page)
    )
    coalesced = independent
    for first_page in range(pages):
        first = first_page * rows_per_page
        source[first:, 0] += 1
        run = source[first:].copy()
        coalesced = coalesced[:first_page] + tuple(
            run[a : a + rows_per_page] for a in range(0, len(run), rows_per_page)
        )
        independent = independent[:first_page] + tuple(
            source[a : a + rows_per_page].copy()
            for a in range(first, len(source), rows_per_page)
        )

    def retained_bytes(chunks):
        owners = {}
        for chunk in chunks:
            owner = chunk
            while isinstance(owner.base, np.ndarray):
                owner = owner.base
            owners[id(owner)] = owner.nbytes
        return sum(owners.values())

    np.testing.assert_array_equal(np.concatenate(independent), source)
    np.testing.assert_array_equal(np.concatenate(coalesced), source)
    exact = source.nbytes
    independent_bytes = retained_bytes(independent)
    coalesced_bytes = retained_bytes(coalesced)
    assert independent_bytes == exact
    assert coalesced_bytes == exact * (pages + 1) // 2
    return {
        "logical_payload_bytes": exact,
        "independent_page_bytes": independent_bytes,
        "coalesced_run_bytes": coalesced_bytes,
        "pages": pages,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--frames", type=int, default=120)
    args = parser.parse_args()
    results = []
    for rows in (64, 1024, 16384):
        for changed in (0, 1, rows // 64, rows):
            for scattered in (False, True):
                for page_rows in (0, 64):
                    results.append(
                        case(rows, changed, scattered, page_rows, args.frames)
                    )
    with open(args.output, "w", encoding="utf-8") as stream:
        json.dump(
            {
                "record_bytes": 192,
                "cases": results,
                "retention_adversary": retention_adversary(),
            },
            stream,
            indent=2,
        )
    print(f"{len(results)} exact payload/held-view checks passed")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Compare ordered packet plans on an actual private native Vulkan draw trace (#612)."""
import argparse
import hashlib
import json
from pathlib import Path


def signature(rows):
    digest = hashlib.sha256()
    for row in rows:
        digest.update(json.dumps(row, sort_keys=True, separators=(',', ':')).encode())
        digest.update(b'\n')
    return digest.hexdigest()


def contiguous_joins(rows):
    result = []
    for row in rows:
        if result:
            previous = result[-1]
            same = previous['key'] == row['key'] and not previous['indirect'] and not row['indirect']
            if same and all(previous[key] == row[key] for key in ('first', 'count', 'base_vertex')):
                if previous['first_instance'] + previous['instances'] == row['first_instance']:
                    previous['instances'] += row['instances']
                    continue
            if same and all(previous[key] == row[key] for key in ('instances', 'first_instance', 'base_vertex')):
                if previous['first'] + previous['count'] == row['first']:
                    previous['count'] += row['count']
                    continue
        result.append(dict(row))
    return result


def compare(trace, limit):
    rows = trace['draws']
    chunks = [rows[first:first + limit] for first in range(0, len(rows), limit)]
    streamed = [row for chunk in chunks for row in chunk]
    assert signature(streamed) == signature(rows)
    adjacent = contiguous_joins(rows)
    return dict(draws=len(rows), trace_order_sha256=signature(rows), packet_header_bytes=trace['packet_bytes'],
                complete_nonempty_descriptor_vectors=sum(bool(row['set_slots']) for row in rows),
                complete_nonempty_vertex_vectors=sum(bool(row['vertex_slots']) for row in rows),
                current=dict(accepted_draws=min(limit, len(rows)), complete=len(rows) <= limit,
                             packet_header_bound_bytes=limit * trace['packet_bytes']),
                large_reference=dict(packets=len(rows), logical_packet_header_bytes=len(rows) * trace['packet_bytes']),
                contiguous_join_upper_bound=dict(packets=len(adjacent), fits=len(adjacent) <= limit,
                    note='Only geometric/state opportunities; shader primitive/base-instance observations still need checking.'),
                bounded_stream=dict(draws=len(streamed), packets_peak=max(map(len, chunks), default=0),
                    logical_packet_header_peak_bytes=max(map(len, chunks), default=0) * trace['packet_bytes'],
                    physical_passes=len(chunks), attachment_boundaries=max(0, len(chunks) - 1),
                    ordered_draws_unchanged=signature(streamed) == signature(rows)),
                scope='Logical planning counters only. Vector capacity, allocator/driver memory, '
                      'actual GPU time and physical attachment traffic require native measurement.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--trace', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--packet-limit', type=int, default=16384)
    args = parser.parse_args()
    if args.packet_limit < 1:
        parser.error('packet limit must be positive')
    output = args.output.resolve()
    if not output.is_relative_to(Path('/tmp').resolve()):
        parser.error('private outputs must be under /tmp')
    trace = json.loads(args.trace.read_text())
    result = compare(trace, args.packet_limit)
    result['trace_sha256'] = hashlib.sha256(args.trace.read_bytes()).hexdigest()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

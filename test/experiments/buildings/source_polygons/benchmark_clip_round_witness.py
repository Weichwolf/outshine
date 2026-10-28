from concurrent.futures import ProcessPoolExecutor
from collections import Counter
import argparse
import hashlib
import json
import os
from pathlib import Path
import time

from clip_round_witness import certifies, convex, search, token

from canonical_polygons import components, load_exports, resolve, whole
from check_reference import fixture_paths


def run(packet):
    key, complete, observations, unsupported = packet
    if unsupported:
        return {'key': key, 'status': unsupported, 'attempts': 0}
    witness, attempts = search(complete, observations)
    if witness is None:
        return {'key': key, 'status': 'bounded-search-unresolved', 'attempts': attempts}
    assert certifies(witness, observations)
    return {'key': key, 'status': 'conditional-global-witness', 'attempts': attempts,
            'witness': [[str(v) for v in p] for p in witness]}


def timed(packets, parallel=False):
    start = time.monotonic()
    if parallel:
        with ProcessPoolExecutor(max_workers=len(os.sched_getaffinity(0))) as pool:
            results = list(pool.map(run, packets))
    else:
        results = [run(p) for p in packets]
    return results, time.monotonic() - start


def main(output, load_note):
    observations, _ = load_exports(fixture_paths())
    packets = []
    for group in components(observations):
        candidate = resolve(group)
        if candidate['status'] != 'ready' or all(whole(o) for o in group):
            continue
        full = next(o for o in group if whole(o))
        ring = list(token([(int(x), int(y)) for x, y in full.polygon.exterior.coords]))
        expected = [(list(token([(int(x), int(y)) for x, y in o.polygon.exterior.coords])), o.window)
                    for o in sorted(group, key=lambda o: (o.tile, o.ordinal))]
        unsupported = ('unmodelled-holes' if any(o.polygon.interiors for o in group) else
                       'unmodelled-nonconvex-rings' if not convex(ring) or any(
                           not convex(r) for r, _ in expected) else None)
        packets.append((candidate['geometry_digest'], ring, expected, unsupported))
    packets.sort()
    first, first_seconds = timed(packets)
    repeat, repeat_seconds = timed(packets)
    parallel, parallel_seconds = timed(packets, parallel=True)
    assert first == repeat == parallel
    adversarial_packet = None
    old, new = (35816072, 23367744), (35816034, 23367744)
    for key, ring, expected, unsupported in packets:
        if any(old in r for r, _ in expected):
            bad = [([new if p == old else p for p in r], window) for r, window in expected]
            adversarial_packet = (key, ring, bad, unsupported)
            break
    assert adversarial_packet is not None
    negative = run(adversarial_packet)
    assert negative['status'] == 'bounded-search-unresolved'
    report = {'scope': 'All original ready components with cut observations audited. '
                      'Only conditional convex pointwise clip/round witnesses; unsupported '
                      'cases and failed bounded searches remain unresolved. No historical '
                      'provider calibration, topology-repair or identity admission.',
              'original_observations': len(observations), 'cut_components': len(packets),
              'workers': len(os.sched_getaffinity(0)),
              'counts': dict(Counter(r['status'] for r in first)),
              'full_output_equal_serial_repeat_and_pool': True,
              'serial_seconds': [first_seconds, repeat_seconds], 'pool_seconds': parallel_seconds,
              'timing_load': load_note,
              'timing_limits': 'Development trend only; no isolated cost winner or physical memory claim.',
              'declared_adversarial_search': negative,
              'input_sha256': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in fixture_paths()},
              'results': first}
    output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('results', 'input_sha256')}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Audit conditional witnesses over all fixed cut components.')
    parser.add_argument('--output', type=Path, required=True, help='Report path outside the fixed inputs.')
    parser.add_argument('--load-note', default='Not recorded; no isolated performance claim.')
    args = parser.parse_args()
    main(args.output, args.load_note)

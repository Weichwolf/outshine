import hashlib
import json
import sys
import time
from pathlib import Path

import numpy as np

path = Path(sys.argv[1])
rawhash = hashlib.sha256(path.read_bytes()).hexdigest()
with np.load(path, allow_pickle=False) as data:
    a = {k: data[k] for k in data.files}
n = len(a["source_ids"])
lookup = a["float64_words"]
assert a["local_frames"].dtype == a["coordinates"].dtype == np.uint16
base = (
    sum(v.nbytes for v in a.values())
    - lookup.nbytes
    - a["local_frames"].nbytes
    - a["coordinates"].nbytes
)
result = {}
for group_size in (1, 8, 32, 128, 1555):
    cpu = time.process_time()
    dictionary_bytes = index_bytes = 0
    query = []
    entries = 0
    for s in range(0, n, group_size):
        e = min(s + group_size, n)
        lo, hi = map(int, a["source_patch_offsets"][[s, e]])
        p, q = map(int, a["patch_polygon_offsets"][[lo, hi]])
        r, t = map(int, a["polygon_ring_offsets"][[p, q]])
        v, w = map(int, a["ring_vertex_offsets"][[r, t]])
        old = np.r_[a["local_frames"][lo:hi].ravel(), a["coordinates"][v:w].ravel()]
        unique, inv = np.unique(old, return_inverse=True)
        width = 2 if len(unique) <= 65536 else 4
        assert np.array_equal(lookup[unique][inv].view("u8"), lookup[old].view("u8"))
        d = len(unique) * 8
        indexes = len(old) * width
        dictionary_bytes += d
        index_bytes += indexes
        entries += 1
        query.extend([d + indexes] * (e - s))
    encoded = base + dictionary_bytes + index_bytes + entries * 16
    result[str(group_size)] = {
        "groups": entries,
        "domain_array_bytes_plus_group_index": encoded,
        "dictionary_bytes": dictionary_bytes,
        "control_index_bytes": index_bytes,
        "group_controls_and_dictionary_bytes_per_single_source_p50_p95_max": np.percentile(
            query, [50, 95, 100]
        ).tolist(),
        "single_run_cpu_s": time.process_time() - cpu,
    }
print(
    json.dumps(
        {
            "input_sha256": rawhash,
            "sources": n,
            "grouping": "contiguous actual producer order; not demonstrated spatial grouping",
            "scope": "lossless word reconstruction; group dictionary plus all group control indices per single-source demand; excludes other metadata, recipes, fallbacks, real IO, allocations, cache and visibility",
            "variants": result,
        },
        indent=2,
    )
)

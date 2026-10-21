from pathlib import Path
import struct, math, json, time, hashlib, argparse
parser = argparse.ArgumentParser(description='Compare whole-batch and conservative cluster index work on a native shadow snapshot.')
parser.add_argument('--casters', type=Path, required=True)
parser.add_argument('--patch-log', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--initial-patches', type=int, default=0)
args = parser.parse_args()
raw = args.casters.read_bytes()
offset = 0
nb, nj, ns, nr = struct.unpack_from('<4Q', raw)
offset = 32
batches = []
for _ in range(nb):
    fields = struct.unpack_from('<8I4f', raw, offset)
    offset += 48
    batches.append(fields)
jobs = list(struct.iter_unpack('<4I', raw[offset:offset + nj * 4]))
offset += nj * 4
spheres = list(struct.iter_unpack('<12f', raw[offset:offset + ns * 4]))
offset += ns * 4
rows = list(struct.iter_unpack('<16f', raw[offset:offset + nr * 64]))
offset += nr * 64
assert offset == len(raw)
log = args.patch_log.read_text()
patches = []
for line in log.splitlines():
    if line.startswith('CASTER_PATCH '):
        fields = line.split()[1:]
        patches.append((tuple(map(int, fields[:6])), tuple(map(float, fields[6:]))))
assert 'SUBJECT_POSE' not in log, 'A changing vertex stream needs snapshots for each revision.'
assert patches and 0 <= args.initial_patches < len(patches)
EPS = 2 ** (-23)
MIN = 2 ** (-126)
GUARD = 8 * EPS
normal = lambda v: math.isfinite(v) and (v == 0 or abs(v) >= MIN)

def f32(v):
    try:
        return struct.unpack('<f', struct.pack('<f', v))[0]
    except OverflowError:
        return math.copysign(math.inf, v)

def pack(c, r):
    cc = tuple(map(f32, c))
    if not all(map(normal, cc)):
        return None
    rr = f32(r + math.dist(c, cc))
    if not math.isfinite(rr) or rr < 0:
        return None
    rr = struct.unpack('<f', struct.pack('<I', struct.unpack('<I', struct.pack('<f', rr))[0] + 1))[0]
    return (*cc, rr) if normal(rr) else None
models = {}

def world(s, slot):
    if not all(map(normal, s[:4])) or s[3] < 0:
        return None
    if slot not in models:
        row = rows[slot]
        m = [row[axis::4] for axis in range(3)]
        if row[15] != 1 or any((row[i] != 0 for i in [3, 7, 11])) or (not all(map(normal, row))):
            models[slot] = None
        else:
            scale = math.sqrt(max((sum((abs(sum((m[k][a] * m[k][b] for k in range(3)))) for b in range(3))) for a in range(3))))
            models[slot] = (m, scale)
    if models[slot] is None:
        return None
    m, scale = models[slot]
    centre = [m[a][3] + sum((m[a][c] * s[c] for c in range(3))) for a in range(3)]
    error = [GUARD * (abs(m[a][3]) + 1 + sum((abs(m[a][c]) * (abs(s[c]) + s[3]) for c in range(3)))) for a in range(3)]
    return pack(centre, s[3] * scale * (1 + GUARD) + math.hypot(*error))
prepared = []
began = time.monotonic()
prepared_count = 0
for i, b in enumerate(batches):
    first, count, model, instances, jfirst, jcount, motion, bound, *_ = b
    covered = first
    prev = first
    held = []
    valid = instances == 1 and jcount > 0 and (model < nr) and (jfirst + jcount <= len(jobs))
    if valid:
        for sphere, owner, start, size in jobs[jfirst:jfirst + jcount]:
            end = start + size
            if owner != i or start < prev or start > covered or (end > first + count) or (size == 0) or size % 3 or (start - first) % 3:
                valid = False
                break
            held.append((start, size, world(spheres[sphere], model) if sphere < len(spheres) else None))
            covered = max(covered, end)
            prev = start
    prepared.append(held if valid and covered == first + count else None)
    prepared_count += len(held) if prepared[-1] is not None else 0
prep = time.monotonic() - began

def planes(matrix):
    out = []
    for side in range(4):
        sign = 1 if side % 2 == 0 else -1
        aa = matrix[3::4]
        bb = matrix[side // 2::4]
        eq = tuple((a + sign * b for a, b in zip(aa, bb)))
        err = tuple((abs(a) + abs(b) for a, b in zip(aa, bb)))
        out.append((eq, err, math.hypot(*eq[:3])))
    return out

def outside(s, frustum):
    if s is None:
        return False
    for eq, err, scale in frustum:
        maximum = eq[3] + s[3] * scale + sum((eq[a] * s[a] for a in range(3)))
        uncertainty = err[3] + 1 + sum((err[a] * (abs(s[a]) + s[3]) for a in range(3)))
        if maximum < -32 * EPS * uncertainty:
            return True
    return False
counts = []
began = time.monotonic()
checks = 0
for n, (meta, matrix) in enumerate(patches):
    frustum = planes(matrix)
    baseline = selected = 0
    selected_draws = 0
    batch_checks = 0
    for b, held in zip(batches, prepared):
        first, count, model, instances, jfirst, jcount, motion, bound, *sphere = b
        if model >= meta[4] and model < meta[5]:
            continue
        if outside(sphere if bound else None, frustum):
            continue
        instances = min(instances, meta[4] - model) if model < meta[5] else instances
        if not instances:
            continue
        baseline += count * instances
        batch_checks += 1
        if held is None:
            selected += count * instances
            selected_draws += 1
            continue
        begin = end = 0
        for start, size, ball in held:
            checks += 1
            if outside(ball, frustum):
                continue
            if end and start > end:
                selected += end - begin
                selected_draws += 1
                end = 0
            if not end:
                begin = start
            end = max(end, start + size)
        if end:
            selected += end - begin
            selected_draws += 1
    counts.append(dict(patch=n, baseline=baseline, selected=selected, draws=selected_draws, coarse_batch_checks=batch_checks))
result = dict(input_sha256=hashlib.sha256(raw).hexdigest(), batches=nb, jobs=nj // 4, spheres=ns // 12, placements=nr, eligible_batches=sum((x is not None for x in prepared)), prepared_world_bounds=prepared_count, prepare_seconds=prep, selection_seconds=time.monotonic() - began, cluster_tests=checks, measured_baseline=sum((x['baseline'] for x in counts[args.initial_patches:])), measured_selected=sum((x['selected'] for x in counts[args.initial_patches:])), counts=counts, scope='Python work/reference comparison on actual native exported input; not native time/quality admission. No vertex poses in exported window; prepared bounds reuse vs repeated transforms measured as work counts.')
args.output.write_text(json.dumps(result, indent=2))
print(json.dumps({k: v for k, v in result.items() if k != 'counts'}, indent=2), flush=True)

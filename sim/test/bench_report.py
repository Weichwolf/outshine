#!/usr/bin/env python3
"""Aggregate bench.js runs into a baseline, and say what a later run must beat to count.

Stdlib only, like pngstat.py -- a check that needs a package install to keep the build honest is
its own joke.

What this computes and why:

* PERCENTILES, not the mean. The refactor this baseline exists for is about hitches. A stall in
  1 % of frames moves p99 and leaves the mean where it was; the mean is exactly the statistic that
  cannot see the thing we are looking for. p99 is printed WITH the number of frames behind it,
  because a p99 resting on four samples is a rumour, not a percentile.
* The NOISE FLOOR -- the spread of the same statistic across identical repeat runs. This is the
  real product here. Without it "p95 improved by 4 %" is not a result, because nobody knows whether
  4 % is anything at all. A later step counts only if it moves a metric by more than this floor.
* A PLAUSIBILITY CROSS-CHECK between two independent measurements of the same CPU work: the sum of
  the per-frame callback timings (ours, performance.now inside the rAF wrapper) against CDP's
  ScriptDuration (chromium's own accounting). They measure overlapping-but-not-identical things --
  ScriptDuration also contains WebSocket handlers and module init -- so they are required to agree
  in ORDER, not exactly. If our sum ever EXCEEDS ScriptDuration, or falls far under it, one of the
  two is not what we think it is and the run is flagged rather than quietly averaged in.

frame_interval is reported as a DIAGNOSTIC and must not be read as frame time -- see bench.js's
header: headless chromium paces an idle page at ~131 ms while the frame itself costs 0.5 ms, and
adding CPU load makes the rate go UP. It is here to show the pacing, not the performance.

  bench_report.py out.json run1.json run2.json ...   -> writes the baseline, prints the summary
"""
import sys, json, statistics


def pct(xs, p):
    """Nearest-rank percentile on the sorted sample. No interpolation: the rank is the honest
    answer to 'a real frame this slow exists'."""
    if not xs:
        return float('nan')
    s = sorted(xs)
    k = max(0, min(len(s) - 1, int(round(p / 100.0 * len(s) + 0.5)) - 1))
    return s[k]


def stats(xs):
    return {'n': len(xs), 'mean': statistics.fmean(xs) if xs else float('nan'),
            'p50': pct(xs, 50), 'p95': pct(xs, 95), 'p99': pct(xs, 99),
            'max': max(xs) if xs else float('nan')}


def spread(vals):
    """Relative run-to-run spread of one statistic: (max-min)/mean. The noise floor."""
    vals = [v for v in vals if v == v]
    if len(vals) < 2:
        return float('nan')
    m = statistics.fmean(vals)
    return (max(vals) - min(vals)) / m if m else float('nan')


def sem_rel(vals):
    """Relative standard error of the MEAN across runs -- the number a later comparison actually
    needs. The run-to-run scatter here is not just measurement jitter: every browser session starts
    with the aircraft somewhere else in its orbit, under different sun and weather, so each run
    draws a different scene. That scatter does not shrink by measuring longer, only by averaging
    more runs -- which is exactly what the SEM describes and the raw spread does not.

    A later step should be believed only if the means differ by more than ~2x the pooled SEM."""
    vals = [v for v in vals if v == v]
    if len(vals) < 2:
        return float('nan')
    m = statistics.fmean(vals)
    return (statistics.stdev(vals) / len(vals) ** 0.5) / m if m else float('nan')


def main():
    out_path, runs = sys.argv[1], sys.argv[2:]
    data = [json.load(open(p)) for p in runs]

    by_ground = {}
    for d in data:
        by_ground.setdefault(d['ground'], []).append(d)

    baseline = {'runs': len(data), 'grounds': {}}
    warnings = []

    # One artifact per baseline, or it is not a baseline. This is not hypothetical: a run here was
    # taken against :8080 while another agent rebuilt that stack from their working tree, so the
    # samples straddled two different cc.wasm builds and the average described neither.
    shas = {d.get('wasm_sha256') for d in data}
    if len(shas) > 1:
        warnings.append(f"samples span {len(shas)} DIFFERENT cc.wasm builds {sorted(s[:12] for s in shas if s)} "
                        f"— this is not one baseline; pin a commit with bench_stack.sh")
    busy = [d for d in data if d.get('idle_pct_after', 100) < 40]
    if busy:
        warnings.append(f"{len(busy)}/{len(data)} runs finished on a box under load "
                        f"(idle {[d.get('idle_pct_after') for d in busy]}%) — wall time inside the "
                        f"frame callback counts time descheduled, so those runs measure contention")
    restarted = [d for d in data if d.get('aircraft_restarted')]
    if restarted:
        warnings.append(f"{len(restarted)}/{len(data)} runs had fb-aircraft restart mid-window — "
                        f"the scene (pose, altitude, flight phase) jumped underneath them. Those "
                        f"runs are a different scene, not a noisier sample of the same one")

    for ground, ds in sorted(by_ground.items()):
        per_run = []
        for d in ds:
            cb_sum_s = sum(d['frame_cb_ms']) / 1000.0
            script = d['cdp']['ScriptDuration']
            r = {
                'frame_cb': stats(d['frame_cb_ms']),
                'interval_diag': stats(d['interval_ms']),
                'fps': len(d['frame_cb_ms']) / d['measure_wall_s'],
                'cdp': d['cdp'],
                'counters': d['counters'],
                'streamed_s': d['streamed_s'],
                'ground_pct': d.get('ground_pct'),
                'measure_wall_s': d['measure_wall_s'],
                # CPU per frame from chromium's own accounting -- the metric least dependent on our
                # instrumentation, and the one an SoA/SIMD change has to move.
                'script_ms_per_frame': script * 1000.0 / max(1, len(d['frame_cb_ms'])),
                'script_share': script / d['measure_wall_s'],
                'task_share': d['cdp']['TaskDuration'] / d['measure_wall_s'],
                'cb_sum_s': cb_sum_s,
                # The regression net. Per-frame GL call counts: p50 is the steady-state work,
                # max catches the legitimate tile-boundary bake. See INVARIANTS below.
                'gl': {k: {'p50': pct(v, 50), 'p95': pct(v, 95), 'max': max(v) if v else 0,
                           'total': sum(v)}
                       for k, v in (d.get('gl_per_frame') or {}).items()},
            }
            # The cross-check. Our per-frame sum lives inside ScriptDuration, so it must not exceed
            # it, and should be a substantial fraction of it.
            r['cb_over_script'] = cb_sum_s / script if script else float('nan')
            if script and not (0.2 <= r['cb_over_script'] <= 1.05):
                warnings.append(
                    f"{ground}: frame_cb sum {cb_sum_s*1000:.0f}ms vs ScriptDuration "
                    f"{script*1000:.0f}ms (ratio {r['cb_over_script']:.2f}) — the two CPU metrics "
                    f"disagree; one of them is not measuring what it claims")
            per_run.append(r)

        noise, thresh = {}, {}
        series = {f'frame_cb.{k}': [r['frame_cb'][k] for r in per_run]
                  for k in ('p50', 'p95', 'p99', 'max')}
        series['script_ms_per_frame'] = [r['script_ms_per_frame'] for r in per_run]
        series['script_share'] = [r['script_share'] for r in per_run]
        series['task_share'] = [r['task_share'] for r in per_run]
        for k, v in series.items():
            noise[k] = spread(v)
            thresh[k] = 2 * sem_rel(v)   # the "believe it only above this" line

        baseline['grounds'][ground] = {'per_run': per_run, 'noise': noise,
                                       'threshold_2sem': thresh}

    baseline['warnings'] = warnings
    json.dump(baseline, open(out_path, 'w'), indent=1)

    for ground, g in baseline['grounds'].items():
        rs = g['per_run']
        n = statistics.fmean([r['frame_cb']['n'] for r in rs])
        print(f"\n=== ground={ground}  ({len(rs)} runs x {rs[0]['measure_wall_s']:.0f}s, "
              f"{n:.0f} frames/run)")
        print("  frame_cb   " + "  ".join(
            f"{k}={statistics.fmean([r['frame_cb'][k] for r in rs]):6.2f}ms"
            for k in ('p50', 'p95', 'p99', 'max')))
        print(f"             (p99 rests on ~{max(1, round(n*0.01))} frames per run)")
        print(f"  cpu/frame  script={statistics.fmean([r['script_ms_per_frame'] for r in rs]):.2f}ms"
              f"   busy={statistics.fmean([r['script_share'] for r in rs])*100:.1f}% of one core"
              f"   (task {statistics.fmean([r['task_share'] for r in rs])*100:.1f}%)")
        print(f"  x-check    frame_cb sum / ScriptDuration = "
              f"{statistics.fmean([r['cb_over_script'] for r in rs]):.2f}  (must be <=1.05)")
        print(f"  DIAG only  frame_interval p50="
              f"{statistics.fmean([r['interval_diag']['p50'] for r in rs]):.1f}ms "
              f"({statistics.fmean([r['fps'] for r in rs]):.1f} fps) — headless pacing, NOT a "
              f"perf metric, see bench.js header")
        # --- the counters, and the invariant that makes them a net rather than a curiosity ---
        gl_keys = [k for k in rs[0].get('gl', {}) if any(r['gl'][k]['total'] for r in rs)]
        if gl_keys:
            print("  GL per frame (counts, not times — no noise floor to beat):")
            for k in sorted(gl_keys):
                p50 = statistics.fmean([r['gl'][k]['p50'] for r in rs])
                mx = max(r['gl'][k]['max'] for r in rs)
                tot = statistics.fmean([r['gl'][k]['total'] for r in rs])
                print(f"             {k:22s} p50={p50:7.1f}  max={mx:6.0f}  total/run={tot:8.0f}")
        # --- THE invariants. Two DIFFERENT uploads live in texImage2D and must not be conflated ---
        #
        # An earlier version asserted "texImage2D median == 0 per frame" and would have shipped a
        # gate that is permanently red in EVS, because it counted two unrelated things:
        #
        #   tile albedo  w3_bake -> glTexImage2D + glGenerateMipmap   (world3d.h:322)
        #   video frame  fb_codec_upload -> texImage2D, NO mipmap     (cc.c:77)
        #
        # ground=photo is EVS: the simulated 5.8 GHz link really does encode, decode and upload a
        # NEW picture every frame -- that is the design, not repeated work. ground=osm is SVS and
        # bypasses the codec entirely (a terrain database does not cross a radio link), so it is
        # exactly 0. An alarm that always fires gets switched off, so the split is the gate.
        #
        # generateMipmap is the honest separator: only the tile path calls it. It is also exactly
        # the counter that would have shown the bug this effort came from -- 256 per frame, not 0.
        mip = 'generateMipmap'
        bake = [r['gl'][mip]['p50'] for r in rs if mip in r.get('gl', {})]
        if bake and statistics.fmean(bake) > 0:
            warnings.append(
                f"{ground}: {mip} median is {statistics.fmean(bake):.1f} PER FRAME with the world "
                f"settled — tile albedo is being re-baked every frame instead of reused. This is "
                f"the 1-fps thrash signature; expected median is 0 (a real tile-boundary bake "
                f"lands in max, never in the median)")
        # Video uploads = texture uploads that are NOT tile bakes. Hard expectation per mode, so a
        # codec accidentally re-enabled in SVS shows up as a number rather than as a vibe.
        tex = [r['gl'].get('texImage2D', {}).get('p50', 0) for r in rs]
        vid = statistics.fmean(tex) - (statistics.fmean(bake) if bake else 0)
        want = 1 if ground == 'photo' else 0
        print(f"  codec      video uploads/frame = {vid:.2f}  (EVS expects 1, SVS expects 0)")
        if abs(vid - want) > 0.05:
            warnings.append(
                f"{ground}: {vid:.2f} video uploads per frame, expected {want}. "
                + ("SVS must bypass the codec — a terrain database does not cross the radio link"
                   if want == 0 else "EVS should upload exactly one decoded frame per frame"))
        c = rs[0]['counters']
        if c:
            print(f"  streamer   drawn={c['drawn']}/{c['budget']} pending={c['pending']} "
                  f"wanted_split={c['wanted_split']} over_budget={c['over_budget']}")
            print(f"             levels={c['levels']} baked={c['baked']} cached={c['cached']} "
                  f"evicted={c['evicted']}")
        print(f"  stream_s   {statistics.fmean([r['streamed_s'] for r in rs]):.1f}s")
        gp = [r['ground_pct'] for r in rs if r.get('ground_pct') is not None]
        if gp:
            print(f"  ground_pct mean={statistics.fmean(gp):.1f}%  ({min(gp):.0f}..{max(gp):.0f})")
        print("  NOISE: spread = raw run-to-run range; THRESHOLD = 2x SEM of the mean, i.e. what a")
        print("         later step's mean-of-N must differ by before it may be called anything:")
        for k, v in sorted(g['noise'].items(), key=lambda kv: -(kv[1] if kv[1] == kv[1] else 0)):
            print(f"             {k:22s} spread {v*100:6.1f}%   threshold "
                  f"{g['threshold_2sem'][k]*100:5.1f}%")

    if warnings:
        print("\n!! PLAUSIBILITY WARNINGS")
        for w in warnings:
            print("   " + w)

    print(f"\nwrote {out_path}")


if __name__ == '__main__':
    main()

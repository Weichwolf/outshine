#!/usr/bin/env python3
"""Diff two tools/fb_regress.sh snapshots.

Separates the three things a conservation gate has to tell apart:
  APPENDED  a telemetry file gained trailing columns; every pre-existing column is byte-identical
  MOVED     a pre-existing telemetry column changed value
  EVENTS    the normalised events.log differs
Anything else (a file only on one side, a changed exit code) is reported as such.
"""
import csv, os, subprocess, sys

a, b = sys.argv[1], sys.argv[2]


def missions(root):
    return sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d)))


appended, moved, events, other = {}, {}, [], []
for m in missions(a):
    pa, pb = os.path.join(a, m), os.path.join(b, m)
    if not os.path.isdir(pb):
        other.append(f"{m}: missing in {b}")
        continue
    fa = {f for f in os.listdir(pa) if f.endswith(".csv")}
    fb = {f for f in os.listdir(pb) if f.endswith(".csv")}
    for f in sorted(fa ^ fb):
        other.append(f"{m}/{f}: only on one side")
    for f in sorted(fa & fb):
        ra = list(csv.reader(open(os.path.join(pa, f))))
        rb = list(csv.reader(open(os.path.join(pb, f))))
        if not ra or not rb:
            continue
        ha, hb = ra[0], rb[0]
        if len(ra) != len(rb):
            other.append(f"{m}/{f}: row count {len(ra)} -> {len(rb)}")
            continue
        n = len(ha)
        if hb[:n] != ha:
            other.append(f"{m}/{f}: header changed in place")
            continue
        if len(hb) > n:
            appended.setdefault(tuple(hb[n:]), []).append(f"{m}/{f}")
        bad = set()
        for i in range(1, len(ra)):
            for j in range(n):
                if ra[i][j] != rb[i][j]:
                    bad.add(ha[j])
        if bad:
            moved.setdefault(m, set()).update(bad)
    ea, eb = os.path.join(pa, "events.norm"), os.path.join(pb, "events.norm")
    if os.path.exists(ea) and os.path.exists(eb):
        if open(ea).read() != open(eb).read():
            events.append(m)

xa, xb = os.path.join(a, "exit.txt"), os.path.join(b, "exit.txt")
if os.path.exists(xa) and os.path.exists(xb):
    d = subprocess.run(["diff", xa, xb], capture_output=True, text=True).stdout
    if d:
        other.append("EXIT CODES DIFFER:\n" + d)

print(f"== APPENDED COLUMNS (old columns byte-identical) ==")
for cols, files in appended.items():
    print(f"  {list(cols)}  in {len(files)} file(s)")
print(f"== MISSIONS WITH A MOVED TELEMETRY VALUE: {len(moved)} ==")
for m in sorted(moved):
    print(f"  {m}: {sorted(moved[m])}")
print(f"== MISSIONS WITH A CHANGED events.log: {len(events)} ==")
for m in events:
    print(f"  {m}")
print(f"== OTHER: {len(other)} ==")
for o in other:
    print(f"  {o}")

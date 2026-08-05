#!/usr/bin/env python3
"""fb-test: the one runner, and the tests are data (doc/testing.md §2).

A harness MEASURES and prints one line per measurement:

    [measure] <harness> <key>=<value> ... value=<number> unit=<string>

A declaration under sim/test/<subject>/ says what that number is expected to be:

    { "subject": "modules/air",
      "claim":   "F-15C Eagle (f15c): Vmax at 12000 m",
      "measure": {"harness": "air-envelope", "args": {"row": "f15c", "anchor": "A1"}},
      "expect":  {"value": 2.5, "unit": "M", "band": 0.05},
      "source":  "[DOC modules/air/catalogue.md §f15c]; band [DOC .../flight-model-recipe.md §7.1]",
      "tier":    "A" }

A declaration matches the measurement line whose fields all agree with its `args`. Nothing else about
the harness is known here — which is the point: a harness that computes its own verdict can quietly
stop gating, and one that did exactly that hid seven anchors outside their bands for as long as they
existed (doc/testing.md §0).

  tier A   gates. Outside its band or unmeasured = failure.
  tier B   recorded and reported, never gating. `expect.value = null` is a declared HOLE: the sources
           publish no figure, the number is measured anyway, and the hole is visible in the table
           instead of absent from it.

Stdlib only. Exit 0 = every tier-A declaration is inside its band, 1 = at least one is not.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import time

SIM_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEST_DIR = os.path.join(SIM_DIR, "test")
SRC_DIR = os.path.join(SIM_DIR, "src")

# name -> (binary under sim/, the make target that builds it). A harness reaches this table when its
# expectations have become declarations; the ones still carrying their own verdict are named in
# doc/testing.md ## Gaps and are NOT silently absent.
HARNESSES = {
    "air-envelope": ("build/fb-test-air-envelope", "test-air"),
}

RE_MEASURE = re.compile(r"^\[measure\]\s+(\S+)\s+(.*)$")
REQUIRED = ("subject", "claim", "measure", "expect", "source", "tier")


def load(root):
    """Every declaration under root, with the file it came from. Structural defects are collected
    rather than raised: a malformed declaration is a finding, not a crash."""
    decls, errors = [], []
    for dp, dns, fns in os.walk(root):
        dns[:] = sorted(d for d in dns if not d.startswith("."))
        for fn in sorted(fns):
            if not fn.endswith(".json"):
                continue
            path = os.path.join(dp, fn)
            rel = os.path.relpath(path, root).replace(os.sep, "/")
            subject = os.path.dirname(rel)
            try:
                body = json.load(open(path, encoding="utf-8"))
            except ValueError as e:
                errors.append(f"{rel}: not JSON ({e})")
                continue
            if not isinstance(body, list):
                errors.append(f"{rel}: the file is a LIST of declarations")
                continue
            for i, d in enumerate(body):
                where = f"{rel}[{i}]"
                miss = [k for k in REQUIRED if k not in d]
                if miss:
                    errors.append(f"{where}: missing {', '.join(miss)}")
                    continue
                # THE MIRRORED TREE, applied to content: a declaration's subject IS its directory, so
                # "what tests this?" is answered by path (make verify-trees).
                if d["subject"] != subject:
                    errors.append(f"{where}: subject '{d['subject']}' but it lies in '{subject}/'")
                if d["tier"] not in ("A", "B"):
                    errors.append(f"{where}: tier '{d['tier']}' is neither A nor B")
                if not d["source"]:
                    errors.append(f"{where}: no source — a band without a source is a defect")
                e = d["expect"]
                if (e.get("value") is None) != (e.get("band") is None):
                    errors.append(f"{where}: value and band must both be present or both be null")
                if e.get("value") is not None and d["tier"] != "A":
                    errors.append(f"{where}: a published figure that does not gate — tier must be A")
                d["_where"] = where
                decls.append(d)
    return decls, errors


def run_harness(name, capture=None):
    """(measurements, seconds). A measurement is a dict of the line's fields plus value/unit."""
    if capture:
        out = open(capture, encoding="utf-8").read()
        secs = 0.0
    else:
        binary, target = HARNESSES[name]
        path = os.path.join(SIM_DIR, binary)
        if not os.path.exists(path):
            print(f"fb-test: {binary} is missing — `make -C sim {target}`", file=sys.stderr)
            return None, 0.0
        t0 = time.time()
        p = subprocess.run([path], cwd=SIM_DIR, capture_output=True, text=True)
        secs = time.time() - t0
        if p.returncode != 0:
            print(f"fb-test: {binary} exited {p.returncode}", file=sys.stderr)
            return None, secs
        out = p.stdout
    rows = []
    for line in out.splitlines():
        m = RE_MEASURE.match(line)
        if not m or m.group(1) != name:
            continue
        fields = dict(kv.split("=", 1) for kv in m.group(2).split() if "=" in kv)
        if "value" not in fields:
            continue
        fields["value"] = float(fields["value"])
        rows.append(fields)
    return rows, secs


def find(rows, args):
    hits = [r for r in rows if all(str(r.get(k)) == str(v) for k, v in args.items())]
    return hits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--harness", action="append", help="run only these harnesses (repeatable)")
    ap.add_argument("--from", dest="capture", help="read a saved measurement stream instead of "
                    "running the harness (one harness only)")
    ap.add_argument("--verbose", action="store_true", help="print every declaration, not only the "
                    "failures")
    a = ap.parse_args()

    decls, errors = load(TEST_DIR)
    wanted = set(a.harness) if a.harness else None

    streams, secs = {}, {}
    for name in sorted({d["measure"]["harness"] for d in decls}):
        if wanted and name not in wanted:
            continue
        if name not in HARNESSES:
            errors.append(f"harness '{name}' is declared against but not in fb_test.py's table")
            continue
        streams[name], secs[name] = run_harness(name, a.capture)

    n = {"band": 0, "outside": 0, "unmeasured": 0, "recorded": 0, "hole": 0, "skipped": 0}
    lines = []
    for d in decls:
        name = d["measure"]["harness"]
        if name not in streams:
            n["skipped"] += 1
            continue
        rows = streams[name]
        e, tier = d["expect"], d["tier"]
        hits = find(rows, d["measure"]["args"]) if rows is not None else []
        if len(hits) > 1:
            errors.append(f"{d['_where']}: {len(hits)} measurements match its args")
        if not hits:
            n["unmeasured" if e.get("value") is not None else "hole"] += 1
            lines.append(("NOT MEASURED", tier, d, None, None))
            continue
        got = hits[0]["value"]
        if e.get("value") is None:
            n["recorded"] += 1
            lines.append(("recorded", tier, d, got, None))
            continue
        dev = (got - e["value"]) / e["value"] if e["value"] else 0.0
        inband = abs(dev) <= e["band"]
        n["band" if inband else "outside"] += 1
        lines.append(("in band" if inband else "OUTSIDE", tier, d, got, dev))

    bad = [l for l in lines if l[0] != "in band" and (a.verbose or l[0] != "recorded")]
    if bad:
        w = max(len(l[2]["claim"]) for l in bad)
        for verdict, tier, d, got, dev in bad:
            u = d["expect"].get("unit", "")
            want = "-" if d["expect"].get("value") is None else f"{d['expect']['value']:g}"
            have = "-" if got is None else f"{got:.4f}"
            pct = "-" if dev is None else f"{100.0 * dev:+.1f}%"
            band = "-" if d["expect"].get("band") is None else f"{100.0 * d['expect']['band']:.0f}%"
            print(f"  [{tier}] {d['claim']:<{w}}  {want:>10} {have:>12} {u:<5} {pct:>8} {band:>5} "
                  f" {verdict}")
    if a.verbose:
        for verdict, tier, d, got, dev in lines:
            if verdict == "in band":
                print(f"  [{tier}] {d['claim']}  {got:.4f} {d['expect'].get('unit','')}  in band")

    # A HOLE IS VISIBLE (doc/testing.md §4): a source directory with no gating declaration is reported
    # here, because a subject nobody asserts anything about is exactly what the whole file argues
    # against — and it cannot be seen by looking at the tests that DO exist.
    subjects = {d["subject"] for d in decls if d["tier"] == "A"}
    unasserted = []
    for dp, dns, _ in os.walk(SRC_DIR):
        dns[:] = sorted(x for x in dns if not x.startswith("."))
        rel = os.path.relpath(dp, SRC_DIR).replace(os.sep, "/")
        if rel != "." and rel not in subjects:
            unasserted.append(rel)

    sys.stdout.flush()   # the findings go to stderr; unflushed they would print before the table
    for e in errors:
        print(f"fb-test: {e}", file=sys.stderr)
    took = f" in {sum(secs.values()):.0f} s" if secs else ""
    print(f"fb-test: {len(decls)} declaration(s), {len(streams)} harness(es){took} — "
          f"{n['band']} in band, {n['outside']} OUTSIDE, {n['unmeasured']} not measured; "
          f"{n['recorded']} recorded, {n['hole']} declared hole(s) unmeasured, "
          f"{n['skipped']} not run")
    print(f"fb-test: {len(unasserted)} of {len(unasserted) + len(subjects)} source director(y/ies) "
          f"carry no tier-A declaration: {', '.join(unasserted)}")
    if errors or n["outside"] or n["unmeasured"]:
        sys.stdout.flush()
        print(f"fb-test: FAILED ({n['outside']} outside band, {n['unmeasured']} unmeasured, "
              f"{len(errors)} structural)", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""fb_duel_report.py — read one duel run and print both sides' eng_* debriefing side by side.

Not a build target and not part of any client: the campaign's ANALYSIS tool, in the same relationship
to `fb-gym` that `fb_tournament.py` has. Everything it prints comes out of the run's own artefacts —
the last line of each `telemetry*.csv` (the eng_* channels survive the engagement, doc/pilot.md §8)
plus the `events.log` lines the systems emitted. It recomputes nothing.

Usage:  fb_duel_report.py <outdir> [<outdir> ...]
        fb_duel_report.py --table <outdir> ...        one row per run, geometry x outcome
"""
import csv
import os
import re
import sys

ENG = ["eng_state", "eng_detect_s", "eng_lock_s", "eng_shot_s", "eng_shot_nm", "eng_shot_ata",
       "eng_shot_aspect", "eng_shot_rtr_nm", "eng_shot_raero_nm", "eng_tta_s", "eng_tti_s",
       "eng_support_s", "eng_support_f", "eng_pitbull", "eng_threat_s", "eng_react_s",
       "eng_defend_s", "eng_shots", "eng_chaff", "eng_es", "eng_es_min"]

STATE_NAMES = ["idle", "search", "closing", "attack", "support", "defend", "abort"]


def last_row(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    return rows[-1] if rows else {}


def all_rows(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def units_of(outdir):
    """The aircraft of the run: telemetry.csv is the primary actor, telemetry_<cs>.csv the rest.
    Weapon units are skipped — a round has no eng_* columns."""
    out = []
    for fn in sorted(os.listdir(outdir)):
        m = re.fullmatch(r"telemetry(?:_(.+))?\.csv", fn)
        if not m:
            continue
        path = os.path.join(outdir, fn)
        with open(path, newline="") as f:
            head = f.readline()
        # A launched round is an FBSimUnit like any other and its guidance is an FBPilot derivation, so
        # it carries eng_* too. The `msl_*` block is what only a weapon has.
        if "eng_state" not in head or "msl_" in head:
            continue
        out.append((m.group(1) or "<primary>", path))
    return out


def events(outdir):
    p = os.path.join(outdir, "events.log")
    return open(p).read().splitlines() if os.path.exists(p) else []


def field(line, key):
    m = re.search(r"\b" + re.escape(key) + r"=(\"[^\"]*\"|\S+)", line)
    return m.group(1).strip('"') if m else None


def tof(line):
    m = re.match(r"t=(\S+)", line)
    return float(m.group(1)) if m else None


def outcome(outdir):
    res = {}
    for ln in events(outdir):
        if "UNIT_RESULT" in ln:
            u = field(ln, "unit")
            if u and "_aim" not in u and "_r27" not in u and "_r73" not in u and "_aim9" not in u:
                res[u] = (field(ln, "result"), field(ln, "reason"))
    return res


def emcon(outdir, unit):
    """When each set was radiating, off its OWN telemetry column (fcr_on / n019_on)."""
    for cs, path in units_of(outdir):
        if cs != unit and not (cs == "<primary>" and unit == "<primary>"):
            continue
        rows = all_rows(path)
        col = "fcr_on" if "fcr_on" in rows[0] else ("n019_on" if "n019_on" in rows[0] else None)
        if not col:
            return None
        spans, on, start = [], False, 0.0
        for r in rows:
            v = float(r[col]) > 0.5
            t = float(r["t"])
            if v and not on:
                on, start = True, t
            elif not v and on:
                on = False
                spans.append((start, t))
        if on:
            spans.append((start, float(rows[-1]["t"])))
        return spans
    return None


def report(outdir):
    print("=" * 100)
    print(f"RUN {outdir}")
    res = outcome(outdir)
    us = units_of(outdir)
    names = [cs for cs, _ in us]
    print(f"{'channel':<18}" + "".join(f"{n:>20}" for n in names))
    rows = {cs: last_row(p) for cs, p in us}
    for k in ENG:
        vals = []
        for cs in names:
            v = rows[cs].get(k, "")
            try:
                fv = float(v)
                if k == "eng_state":
                    v = STATE_NAMES[int(fv)] if 0 <= int(fv) < len(STATE_NAMES) else str(int(fv))
                else:
                    v = f"{fv:.2f}"
            except (TypeError, ValueError):
                pass
            vals.append(v)
        print(f"{k:<18}" + "".join(f"{v:>20}" for v in vals))
    for k in ("dmg_failed", "dmg_effective"):
        print(f"{k:<18}" + "".join(f"{rows[cs].get(k, ''):>20}" for cs in names))
    print("-- result --")
    for u, (r, why) in res.items():
        print(f"   {u:<12} {r:<9} {why}")
    print("-- radiating (s) --")
    for cs in names:
        sp = emcon(outdir, cs)
        if sp is not None:
            print(f"   {cs:<12} " + ", ".join(f"{a:.1f}-{b:.1f}" for a, b in sp) or "   (never)")
    print("-- timeline --")
    pat = re.compile(r"RADAR_CONTACT|RADAR_DESIGNATE|RADAR_DROP|SUPPORT_BINDING|sms RELEASE|"
                     r"LAUNCH_SOLUTION|DETONATION|EXPIRED|damage KILL|THREAT_NEW|ILLUMINATION_LOST|"
                     r"EMISSION|CM_DISPENSE|NOTCH|IRST_")
    for ln in events(outdir):
        if pat.search(ln) and "_aim120_" not in (field(ln, "unit") or "") \
                and "_r27r_" not in (field(ln, "unit") or "") or "DETONATION" in ln \
                or "ILLUMINATION_LOST" in ln or "EXPIRED" in ln:
            print("   " + ln[:190])


def table(dirs):
    print(f"{'run':<26}{'winner':<12}{'loser':<12}{'det_lead_s':>11}{'shot_lead_s':>12}"
          f"{'shots A/B':>11}{'end_s':>8}")
    for d in dirs:
        res = outcome(d)
        us = units_of(d)
        rows = {cs: last_row(p) for cs, p in us}
        win = [u for u, (r, _) in res.items() if r == "SUCCESS"]
        lose = [u for u, (r, _) in res.items() if r == "FAIL"]
        names = [cs for cs, _ in us]
        det = [float(rows[c].get("eng_detect_s", -1) or -1) for c in names]
        sh = [float(rows[c].get("eng_shot_s", -1) or -1) for c in names]
        nsh = [rows[c].get("eng_shots", "?") for c in names]
        endt = max(float(rows[c].get("t", 0)) for c in names)
        dl = (det[1] - det[0]) if len(det) > 1 and min(det) >= 0 else float("nan")
        sl = (sh[1] - sh[0]) if len(sh) > 1 and min(sh) >= 0 else float("nan")
        print(f"{os.path.basename(d):<26}{(win[0] if win else '-'):<12}{(lose[0] if lose else '-'):<12}"
              f"{dl:>11.1f}{sl:>12.1f}{'/'.join(str(int(float(x))) for x in nsh):>11}{endt:>8.1f}")


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--table":
        table(args[1:])
    else:
        for d in args:
            report(d)

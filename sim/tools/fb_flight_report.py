#!/usr/bin/env python3
"""Read one formation run's artefacts and say what the flight did.

Recomputes nothing: every number below is a column of a `telemetry*.csv` or a line of `events.log`
(doc/formation.md section 8). Usage:

    tools/fb_flight_report.py /tmp/run                  # one run, per unit + per flight
    tools/fb_flight_report.py --diff /tmp/a /tmp/b      # two runs side by side (with vs without flight)
"""
import csv
import os
import re
import statistics
import sys

FLT = ["flt_pos", "flt_mates", "flt_src", "flt_assign", "flt_switch", "flt_dup", "flt_free",
       "flt_bound", "flt_mate_bound", "flt_both_s", "flt_cover_s", "flt_exposed_s", "flt_defer_s",
       "flt_sta"]


def primary_callsign(d):
    """The primary actor writes the canonical telemetry.csv; its UNIT_RESULT line names both."""
    ev = os.path.join(d, "events.log")
    if os.path.exists(ev):
        for line in open(ev):
            m = re.search(r"UNIT_RESULT unit=([A-Za-z0-9_-]+).*telemetry=(\S*/telemetry\.csv)$", line.rstrip())
            if m:
                return m.group(1)
    return "<primary>"


def unit_name(path, primary):
    b = os.path.basename(path)
    return b[len("telemetry_"):-len(".csv")] if b.startswith("telemetry_") else primary


def read_run(d):
    """-> {unit: metrics}, plus the run-level event tallies."""
    units = {}
    primary = primary_callsign(d)
    for f in sorted(os.listdir(d)):
        if not (f.startswith("telemetry") and f.endswith(".csv")):
            continue
        rows = list(csv.DictReader(open(os.path.join(d, f))))
        if not rows or "flt_pos" not in rows[0]:
            continue
        if "msl_phase" in rows[0]:          # a launched round is a unit too; it flies no formation
            continue
        last = rows[-1]
        sta = [float(r["flt_sta"]) for r in rows if float(r["flt_sta"]) >= 0.0]
        m = {
            "pos": int(last["flt_pos"]),
            "mates": max(int(r["flt_mates"]) for r in rows),
            "src": max(int(r["flt_src"]) for r in rows),
            "assigned_ticks": sum(1 for r in rows if r["flt_assign"] != "0"),
            "switches": int(last["flt_switch"]),
            # THE violation: sharing a target while another one was unengaged
            "dup_free": sum(1 for r in rows if r["flt_dup"] == "1" and int(r["flt_free"]) > 0),
            "dup_ticks": sum(1 for r in rows if r["flt_dup"] == "1"),
            "free_ticks": sum(1 for r in rows if int(r["flt_free"]) > 0),
            "bound_s": round(sum(1 for r in rows if r["flt_bound"] == "1") * 0.1, 1),
            "both_s": float(last["flt_both_s"]),
            "cover_s": float(last["flt_cover_s"]),
            "exposed_s": float(last["flt_exposed_s"]),
            "defer_s": float(last["flt_defer_s"]),
            "sta_n": len(sta),
            "sta_med": round(statistics.median(sta), 1) if sta else None,
            "sta_max": round(max(sta), 1) if sta else None,
            "shots": 0, "hit_by": 0, "failed_max": 0, "result": "-",
        }
        units[unit_name(os.path.join(d, f), primary)] = m

    ev = os.path.join(d, "events.log")
    if os.path.exists(ev):
        for line in open(ev):
            u = re.search(r"unit=([A-Za-z0-9_-]+)", line)
            if not u:
                continue
            u = u.group(1)
            if u not in units:
                continue
            if " sms RELEASE" in line or " stores SEPARATION" in line:
                units[u]["shots"] += 1
            if " damage DAMAGE " in line:
                units[u]["hit_by"] += 1
                fm = re.search(r"failed=(\d+)", line)
                if fm:
                    units[u]["failed_max"] = max(units[u]["failed_max"], int(fm.group(1)))
            r = re.search(r"UNIT_RESULT unit=%s result=(\w+)" % re.escape(u), line)
            if r:
                units[u]["result"] = r.group(1)
    return units


def table(d):
    units = read_run(d)
    src = {0: "-", 1: "coop", 2: "contract"}
    print("run %s" % d)
    print("%-10s %3s %5s %8s %6s %7s %8s %6s %6s %7s %7s %7s %6s %8s %8s" %
          ("unit", "pos", "mates", "src", "asgnTk", "switch", "dup&free", "shots", "hitBy",
           "bound_s", "both_s", "defer_s", "staN", "staMed", "staMax"))
    for name, m in units.items():
        print("%-10s %3d %5d %8s %6d %7d %8d %6d %6d %7.1f %7.1f %7.1f %6d %8s %8s" %
              (name, m["pos"], m["mates"], src.get(m["src"], "?"), m["assigned_ticks"],
               m["switches"], m["dup_free"], m["shots"], m["hit_by"], m["bound_s"], m["both_s"],
               m["defer_s"], m["sta_n"], m["sta_med"], m["sta_max"]))
    tot = {k: sum(m[k] for m in units.values()) for k in ("dup_free", "shots", "hit_by")}
    both = sum(m["both_s"] for m in units.values())
    print("  FLIGHT TOTAL   double-engagement-while-free %d   shots %d   hits taken %d   "
          "both-bound %.1f s" % (tot["dup_free"], tot["shots"], tot["hit_by"], both))
    return units


def main():
    args = sys.argv[1:]
    if args and args[0] == "--diff":
        a, b = args[1], args[2]
        ua, ub = table(a), (print() or table(b))
        print()
        print("%-10s %-28s %-28s" % ("unit", os.path.basename(a), os.path.basename(b)))
        for name in sorted(set(ua) | set(ub)):
            fa, fb = ua.get(name), ub.get(name)
            fmt = lambda m: ("-" if not m else "shots %d hitBy %d %s" %
                             (m["shots"], m["hit_by"], m["result"]))
            print("%-10s %-28s %-28s" % (name, fmt(fa), fmt(fb)))
        return
    for d in args or ["."]:
        table(d)
        print()


if __name__ == "__main__":
    main()

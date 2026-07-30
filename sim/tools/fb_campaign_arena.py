#!/usr/bin/env python3
"""FlightBox — THE CAMPAIGN ARENA. doc/doctrine-evolution.md §7 (round `E5`), and the instrument
doc/campaigns/w1-red-flag.md said the gate did not have:

    tools/fb_campaign_arena.py --cells tools/arena-campaign.txt --levers tools/levers-genome.txt

WHY IT EXISTS. `fb_arena_check.py` flies a fixed six-variant yardstick over GENERATED geometries — two
pilots, one lane, both seats swappable. A campaign rung is a hand-authored file with a cast, a ground
half and a clock, and neither seat can be swapped without destroying the scenario. W1 pointed the gate
at its own ten rungs by hand and had to name its denominator in prose. This file is that instrument:
the population per cell is the LEVER SET applied to ONE declared side of ONE committed mission, which
is exactly the population S2 is defined on, and S1's modal share is taken in it.

WHAT IT MAY NOT DO, AND THE CHECK IS HERE RATHER THAN IN THE PROSE. The committed missions are never
written: a cell is copied to the output tree, the genome's `set` lines are spliced into the copy, and
`git status --porcelain sim/missions sim/assets` must be empty before and after. A run that moved a
mission or a deck is VOID.

THE SIDE IS DECLARED, NOT DERIVED. A cell is `<mission> <team> <module>`: the units the doctrine is
flown by, and the ones its side key is summed over (fb_fitness.side_key). Deriving it from "the
friendly team" would be wrong in five of the ten campaigns, where the subject of the campaign sits on
the hostile side.

Stdlib only, no build target, one dependency: build/fb-gym.
"""

import argparse
import collections
import csv
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fb_evolve as evo
import fb_fitness as fit
import fb_tournament as tour

SIM_DIR = tour.SIM_DIR
REPO_DIR = os.path.dirname(SIM_DIR)
MISSION_DIR = os.path.join(SIM_DIR, "missions")
CAMPAIGN_DIR = os.path.join(SIM_DIR, "campaigns")

kModalMax = 0.60      # [SET] doc/doctrine-evolution.md §4.2 S1, unchanged and not loosened here
kMoversMin = 3        # [SET] S2, unchanged
kMoverFrac = 3.0 / 9.0  # [SET] E10: the declared nine are the gate's own denominator, so a LONGER
                        # lever file may not buy a pass — S2 wants 3 movers AND 3 of every 9 swept
kGeometriesMin = 6    # S4
kInformativeMin = 3   # S5


class Cell:
    """One arena cell: a committed mission, the team and module the doctrine is flown by, and the
    campaign clock the mission is graded under. The clock is part of the cell because nine of W1's ten
    files declare no `time` of their own and take the campaign's — [MESS, w1-03] the clock alone moves
    2 telemetry columns on the shooter and 7 on the aggressor."""

    def __init__(self, mission, team, module):
        self.mission = mission
        self.team = team
        self.module = module
        self.path = os.path.join(MISSION_DIR, mission + ".fbm")
        self.campaign = mission.split("-")[0]
        self.time = campaign_time(self.campaign)

    @property
    def name(self):
        return "%s:%s" % (self.mission, self.module)


def campaign_time(prefix):
    for f in sorted(os.listdir(CAMPAIGN_DIR)):
        if not f.endswith(".fbc") or not f.startswith(prefix + "-"):
            continue
        for raw in open(os.path.join(CAMPAIGN_DIR, f)):
            line = raw.split("#", 1)[0].strip()
            if line.startswith("time "):
                return line.split()[1]
    return ""


def undisplaced_genes(field):
    """E15 (doc/doctrine-evolution.md §9): the genes NO member of a fixed field moves off the field's
    own baseline. A field may only gate a genome it is commensurate with, and this is the whole check —
    read out of `fb_evolve.GENES`, so it can never drift from the genome it is asked about."""
    base = field[0]
    displaced = set()
    for v in field[1:]:
        displaced |= {k for k, val in v.params.items() if val != base.params.get(k)}
        if (v.dl, v.sort) != (base.dl, base.sort):
            displaced.add("sort")
    return sorted({k for k, blocked in evo.GENES if not blocked} - displaced)


def load_levers(path):
    """The lever set, with ONE difference from `fb_tournament.load_variants`: a line that does not say
    `dl=` leaves the mission's OWN datalink briefing alone. The tournament writes its missions from
    scratch and can default the channel to `off`; a campaign rung has already declared it, and
    overwriting it in the baseline would make every cell's baseline a lever of its own."""
    out = []
    for v in tour.load_variants(path):
        v.dl = ""
        out.append(v)
    for raw in open(path):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        t = line.split()
        for v in out:
            if v.name != t[0]:
                continue
            for tok in t[1:]:
                if tok.startswith("dl="):
                    v.dl = tok[3:]
    return out


def load_cells(path):
    cells = []
    for lineno, raw in enumerate(open(path), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        t = line.split()
        if len(t) != 3:
            sys.exit("%s:%d: want '<mission> <team> <module>'" % (path, lineno))
        c = Cell(*t)
        if not os.path.exists(c.path):
            sys.exit("%s:%d: no such mission %s" % (path, lineno, c.path))
        if any(x.name == c.name for x in cells):
            sys.exit("%s:%d: '%s' is not a unique cell name — two teams of one mission flying the "
                     "same module cannot be told apart in a result table" % (path, lineno, c.name))
        cells.append(c)
    return cells


# ---------------------------------------------------------------------------------------------
# Splicing a genome into a committed mission — the one write path, and it writes only into --out.
# ---------------------------------------------------------------------------------------------
UNIT_RE = re.compile(r"^unit\s+(\S+)")


def splice_mission(text, team, module, variant):
    """Two passes, because a unit block declares `team` and `module` in either order: the first reads
    the blocks, the second writes them."""
    lines = text.splitlines(True)
    starts = [i for i, l in enumerate(lines) if UNIT_RE.match(l)]
    bounds = [(s, starts[k + 1] if k + 1 < len(starts) else len(lines))
              for k, s in enumerate(starts)]

    keys = set(variant.params)
    if variant.dl:
        keys.add("datalink")
    if variant.sort:
        keys.add("brief_sort")

    chosen, drop = [], set()
    inject = {}
    for a, b in bounds:
        block = lines[a:b]
        call = UNIT_RE.match(block[0]).group(1)
        got = {"team": "", "module": ""}
        for l in block:
            t = l.split("#", 1)[0].split()
            if len(t) > 1 and t[0] in got:
                got[t[0]] = t[1]
        if got["team"] != team or got["module"] != module:
            continue
        chosen.append(call)
        add = ["  set %s %g\n" % kv for kv in sorted(variant.params.items())]
        if variant.dl and module == "f16":
            add.append("  set datalink %s\n" % variant.dl)
        if variant.sort:
            add.append("  set brief_sort %s\n" % variant.sort)
        inject[a] = add
        for i in range(a, b):
            t = lines[i].split("#", 1)[0].split()
            if len(t) > 2 and t[0] == "set" and t[1] in keys:
                drop.add(i)

    out = []
    for i, l in enumerate(lines):
        if i in drop:
            continue
        out.append(l)
        if i in inject:
            out.extend(inject[i])
    return "".join(out), chosen


# ---------------------------------------------------------------------------------------------
# Running and reading back
# ---------------------------------------------------------------------------------------------
TELEM_RE = re.compile(r'UNIT_RESULT unit=(\S+) .*telemetry=(\S+)')


def run_cell(job):
    """Fly one (cell, variant), read it, and PRUNE it. A campaign rung writes one telemetry file per
    unit — package Q writes 24 aircraft plus their rounds — so a 2,500-run arena keeps a five-figure
    number of them alive if nothing deletes them. The run is read here, in the worker, and the tree
    goes away with it unless --keep."""
    gym, outroot, tag, cell, text, threads, elev, calls, foe_calls, keep, sink, vname = job
    d = os.path.join(outroot, tag)
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, "mission.fbm")
    with open(path, "w") as f:
        f.write(text)
    cmd = [gym, "--mission", path, "--out", d, "--threads", str(threads), "--elev", elev]
    if cell.time:
        cmd += ["--campaign-time", cell.time]
    r = subprocess.run(cmd, cwd=SIM_DIR, capture_output=True, text=True)
    sides, foes, duration, telem = read_cell(d, calls, foe_calls)
    if not sides:
        sys.exit("%s: no telemetry for %s\n%s%s" % (tag, ",".join(calls), r.stdout[-1500:],
                                                    r.stderr[-1500:]))
    key, items = key_of(sides, foes, duration)
    chan = channels(d, telem, calls)
    chan["exit"] = r.returncode
    chan["durationS"] = duration
    if not keep:
        shutil.rmtree(d, ignore_errors=True)
    if sink:
        sink.note(cell, vname, key, chan)
    return tag, key, items, chan


# The published channels §2.1 names for each gene, plus the two the ground half is graded through.
# Read here rather than in the report so that "which gene can act on this cell" is a MEASUREMENT and
# not a reading of the mission text.
CHAN_COLS = ("flt_mates", "flt_src", "flt_assign", "flt_switch", "flt_dup", "flt_free",
             "flt_both_s", "flt_defer_s", "flt_cover_s", "bfm_ctrl_s", "bfm_es", "eng_shots",
             "eng_shot_s", "dmg_effective")
RELEASE_RE = re.compile(r'ATTACK_RELEASE unit=(\S+) mode=(\S+) accepted=(\d+).*?biasS=([-0-9.e]+)')
DELIVERY_RE = re.compile(r'stores DELIVERY unit=(\S+) mode=(\S+) .*aimErrM=([-0-9.e]+) '
                         r'aimLongM=([-0-9.e]+) aimAcrossM=([-0-9.e]+)')
SORT_RE = re.compile(r'SORT_ASSIGN unit=(\S+)')


def channels(outdir, telem, calls):
    """The MAX over the side's members of each column's LAST row. `fb_fitness.read_side` keeps only the
    `eng_*` columns — it is the fitness's reader and the fitness needs nothing else — so the flight,
    BFM and damage channels are read here, off the same file."""
    out = {c: 0.0 for c in CHAN_COLS}
    for path in telem:
        with open(path, newline="") as f:
            rows = list(csv.reader(f))
        idx = {h: i for i, h in enumerate(rows[0])}
        cols = [(c, idx[c]) for c in CHAN_COLS if c in idx]
        # The MAX over every row, not the last one: `flt_src`/`flt_assign`/`flt_dup` are the current
        # state of the picture and are 0 again once the fight is over, so a last-row read of them
        # measures the debrief and not the engagement. The accumulating columns are unaffected.
        for row in rows[1:]:
            for c, i in cols:
                try:
                    v = float(row[i])
                except (ValueError, IndexError):
                    continue
                if v > out[c]:
                    out[c] = v
    mine = set(calls)
    rel, sort, aim = 0, 0, []
    for line in open(os.path.join(outdir, "events.log")):
        m = RELEASE_RE.search(line)
        if m and m.group(1) in mine:
            rel += 1
        if SORT_RE.search(line) and SORT_RE.search(line).group(1) in mine:
            sort += 1
        d = DELIVERY_RE.search(line)
        if d and d.group(1).split("_")[0] in mine:
            aim.append(float(d.group(3)))
    out["releases"] = rel
    out["sort_assign"] = sort
    out["deliveries"] = len(aim)
    out["aimErrM_mean"] = sum(aim) / len(aim) if aim else -1.0
    out["aimErrM_min"] = min(aim) if aim else -1.0
    return out


def read_cell(outdir, calls, foe_calls):
    """The side's key out of its own telemetry. The call→file map is read from the runner's own
    `UNIT_RESULT … telemetry=` lines rather than from the actor order, because a campaign mission
    declares ground objects and missiles between its aircraft."""
    log = os.path.join(outdir, "events.log")
    paths, duration = {}, 0.0
    for line in open(log):
        m = TELEM_RE.search(line)
        if m:
            paths[m.group(1)] = m.group(2)
        if "mission SUMMARY" in line:
            d = re.search(r"durationS=([0-9.]+)", line)
            if d:
                duration = float(d.group(1))
    sides, foes, mine = [], [], []
    for group, calls_ in ((sides, calls), (foes, foe_calls)):
        for c in calls_:
            p = paths.get(c)
            if p and os.path.exists(p):
                group.append(fit.read_side(p, c))
                if group is sides:
                    mine.append(p)
    by_call = {s.name: s for s in sides + foes}
    fit.read_events(log, by_call)
    return sides, foes, duration, mine


class NullFoe:
    """The opposing view for a cell with no opposing AIRCRAFT — a strike rung against dirt. It answers
    every engagement column with "never", which is what the craft terms mean by an absent opponent;
    the side's own gate then decides whether C exists at all."""
    name = "-"
    effective = False
    hits = 0
    deliveries = ()

    def g(self, _key, default=-1.0):
        return default


def key_of(sides, foes, duration):
    foe = fit.FlightView(foes) if foes else NullFoe()
    return fit.side_key(sides, foe, duration)


class Sink:
    """Every finished run, appended the moment it finishes. An arena pass over the whole campaign
    breadth is 2,720 runs and about an hour of wall clock; a tool that prints only at the end throws
    all of it away when the shell that started it goes. The same file is the RESUME index — a run
    whose (cell, lever) is already in it is not flown again."""

    def __init__(self, path):
        self.path = path
        self.lock = threading.Lock()
        self.have = {}
        if path and os.path.exists(path):
            for row in csv.DictReader(open(path)):
                key = (int(row["V"]), int(row["M"]),
                       fit.GATE if row["C_air"] == "GATE"
                       else (float(row["C_air"]), float(row["C_aim"])))
                self.have[(row["cell"], row["lever"])] = (key, {k: num(v) for k, v in row.items()})
        if path and not os.path.exists(path):
            with open(path, "w") as f:
                f.write(",".join(CSV_COLS) + "\n")

    def note(self, cell, vname, key, chan):
        if not self.path:
            return
        gate = key[2] == fit.GATE
        row = [cell.name, cell.mission, cell.module, vname, key[0], key[1],
               "GATE" if gate else "%.1f" % key[2][0], "GATE" if gate else "%.1f" % key[2][1]]
        row += ["%g" % chan.get(c, 0.0) for c in CSV_COLS[8:]]
        with self.lock:
            with open(self.path, "a") as f:
                f.write(",".join(str(x) for x in row) + "\n")


def num(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return v


CSV_COLS = ["cell", "mission", "module", "lever", "V", "M", "C_air", "C_aim"] + list(CHAN_COLS) + \
           ["releases", "sort_assign", "deliveries", "aimErrM_mean", "aimErrM_min", "exit",
            "durationS"]


def fly(gym, outroot, cells, variants, jobs, threads, elev, keep=False, tagpfx="", sink=None):
    """Every (cell, variant) as one run. Returns {(cell.name, variant.name): (key, calls, items,
    channels)}."""
    jobslist, meta, out = [], [], {}
    for cell in cells:
        text = open(cell.path).read()
        for v in variants:
            spliced, calls = splice_mission(text, cell.team, cell.module, v)
            if not calls:
                sys.exit("%s: no unit with team=%s module=%s" % (cell.mission, cell.team, cell.module))
            tag = "%s%s-%s-%s" % (tagpfx, cell.mission, cell.module, v.name)
            if sink and (cell.name, v.name) in sink.have:
                key, chan = sink.have[(cell.name, v.name)]
                out[(cell.name, v.name)] = (key, calls, [], chan)
                continue
            jobslist.append((gym, outroot, tag, cell, spliced, threads, elev, calls,
                             other_calls(cell, calls), keep, sink, v.name))
            meta.append((tag, cell, v, calls))
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        done = {}
        for tag, key, items, chan in ex.map(run_cell, jobslist):
            done[tag] = (key, items, chan)
    for tag, cell, v, calls in meta:
        key, items, chan = done[tag]
        out[(cell.name, v.name)] = (key, calls, items, chan)
    return out


_OTHER = {}


def other_calls(cell, mine):
    """The opposing aircraft of a cell: every unit of the OTHER team whose module flies (f16/mig29).
    Cached per cell — it is a property of the committed file and cannot change between variants."""
    if cell.name in _OTHER:
        return _OTHER[cell.name]
    calls = []
    lines = open(cell.path).read().splitlines()
    starts = [i for i, l in enumerate(lines) if UNIT_RE.match(l)]
    for k, a in enumerate(starts):
        b = starts[k + 1] if k + 1 < len(starts) else len(lines)
        got = {"team": "", "module": ""}
        call = UNIT_RE.match(lines[a]).group(1)
        for l in lines[a:b]:
            t = l.split("#", 1)[0].split()
            if len(t) > 1 and t[0] in got:
                got[t[0]] = t[1]
        if got["team"] != cell.team and got["module"] in ("f16", "mig29") and call not in mine:
            calls.append(call)
    _OTHER[cell.name] = calls
    return calls


def tree_clean():
    r = subprocess.run(["git", "status", "--porcelain", "sim/missions", "sim/assets"], cwd=REPO_DIR,
                       capture_output=True, text=True)
    return r.stdout.strip() == ""


# ---------------------------------------------------------------------------------------------
# The gate
# ---------------------------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", required=True)
    ap.add_argument("--gym", default=os.path.join(SIM_DIR, "build", "fb-gym"))
    ap.add_argument("--cells", default=os.path.join(SIM_DIR, "tools", "arena-campaign.txt"))
    ap.add_argument("--levers", default=os.path.join(SIM_DIR, "tools", "levers-genome.txt"))
    ap.add_argument("--variants", default="",
                    help="the FIXED YARDSTICK, flown on the same cells: S1's population as "
                         "doc/doctrine-evolution.md §4.2 defines it ('per geometry, against a fixed "
                         "field'). Without it S1 is taken in the LEVER population, which is what "
                         "doc/campaigns/w1-red-flag.md had to do and which bounds the modal share "
                         "from below by the number of levers that are inert on the cell")
    ap.add_argument("--variant-channels", default="")
    ap.add_argument("--elev", default="const")
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--threads", type=int, default=2)
    ap.add_argument("--items", action="store_true", help="print the craft itemisation per cell")
    ap.add_argument("--keep", action="store_true", help="keep every run tree (it is large)")
    ap.add_argument("--channels", default="",
                    help="the per-run published channels, appended as they finish — and the "
                         "RESUME index: a (cell, lever) already in it is not flown again")
    args = ap.parse_args()

    cells = load_cells(args.cells)
    levers = [tour.Variant("baseline", {}, dl="")] + load_levers(args.levers)
    os.makedirs(args.out, exist_ok=True)
    if not tree_clean():
        sys.exit("sim/missions or sim/assets is dirty BEFORE the run")

    yard = load_levers(args.variants) if args.variants else []
    blind = undisplaced_genes(yard) if yard else []
    if blind:
        sys.exit("E15 (doc/doctrine-evolution.md §9): the fixed field %s is INCOMMENSURATE with the "
                 "genome — no member displaces %s from the field's own baseline, so S1 would ask "
                 "whether the cell is spread over doctrines the evolution cannot express while S2 asks "
                 "whether the genome moves it. Their conjunction would be empty for a reason that "
                 "belongs to the instrument. Extend the field (E16); do not loosen S1."
                 % (os.path.basename(args.variants), ", ".join(blind)))

    print("campaign arena: %d cells x %d levers = %d runs, elev %s"
          % (len(cells), len(levers), len(cells) * len(levers), args.elev))
    if yard:
        print("fixed field: %d members, commensurate — every gene of the genome is displaced by one"
              % len(yard))
    keys = fly(args.gym, args.out, cells, levers, args.jobs, args.threads, args.elev,
               args.keep, "", Sink(args.channels))
    ykeys = fly(args.gym, args.out, cells, yard, args.jobs, args.threads, args.elev, args.keep,
                "y-", Sink(args.variant_channels or args.channels + ".yardstick")) if yard else {}

    print("\n" + "=" * 110)
    print("PER CELL — the outcome class is (V, M), summed over the side's members (fb_fitness.side_key)")
    print("=" * 110)
    print("%-26s %-7s %8s %7s %-10s %7s %-28s %s"
          % ("cell", "module", "distinct", "modal", "modal cls", "movers", "which", "S1 S2"))
    res = []
    for cell in cells:
        s1pop = ([fit.outcome_class(ykeys[(cell.name, v.name)][0]) for v in yard] if yard
                 else [fit.outcome_class(keys[(cell.name, v.name)][0]) for v in levers])
        classes = s1pop
        counts = collections.Counter(classes)
        modal_class, modal_n = counts.most_common(1)[0]
        modal = modal_n / float(len(classes))
        base = fit.outcome_class(keys[(cell.name, "baseline")][0])
        movers = [v.name for v in levers[1:]
                  if fit.outcome_class(keys[(cell.name, v.name)][0]) != base]
        s1 = len(counts) >= 2 and modal <= kModalMax
        s2 = len(movers) >= kMoversMin and len(movers) >= kMoverFrac * (len(levers) - 1)
        res.append({"cell": cell, "s1": s1, "s2": s2, "informative": s1 and s2,
                    "vector": tuple(classes), "movers": movers, "modal": modal})
        print("%-26s %-7s %8d %6.1f%% %-10s %7d %-28s %s %s"
              % (cell.mission, cell.module, len(counts), 100 * modal, str(modal_class), len(movers),
                 ",".join(movers)[:28] or "-", "ok" if s1 else "NO", "ok" if s2 else "NO"))
        if args.items:
            for v in levers:
                k, calls = keys[(cell.name, v.name)][:2]
                print("      %-16s %s  [%s]" % (v.name, fit.key_str(k), ",".join(calls)))

    informative = [r for r in res if r["informative"]]
    seen, dupes = {}, []
    for r in informative:
        if r["vector"] in seen:
            dupes.append((seen[r["vector"]], r["cell"].mission))
        seen[r["vector"]] = r["cell"].mission
    print("\nS4 size        : %d cells                (>= %d) %s"
          % (len(res), kGeometriesMin, "ok" if len(res) >= kGeometriesMin else "NO"))
    print("S5 yield       : %d informative          (>= %d) %s   [%s]"
          % (len(informative), kInformativeMin,
             "ok" if len(informative) >= kInformativeMin else "NO",
             ", ".join(r["cell"].mission for r in informative) or "-"))
    print("S6 distinctness: %d identical pair(s)                  %s   %s"
          % (len(dupes), "ok" if not dupes else "NO", dupes or ""))
    print("S3             : n/a — both airframes are FlightBox's read-only model copies")
    ok = (len(res) >= kGeometriesMin and len(informative) >= kInformativeMin and not dupes)
    print("\nARENA: %s   (%d cells, %d informative)"
          % ("PASSED" if ok else "REFUSED", len(res), len(informative)))
    if not tree_clean():
        print("VOID: sim/missions or sim/assets moved during the run")
        return 1
    print("mission and model tree clean after the run")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

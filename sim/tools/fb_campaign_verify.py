#!/usr/bin/env python3
"""fb_campaign_verify — the measuring instrument of the campaign layer (doc/missions/campaign.md §5).

It does not run a campaign; fb-gym --campaign does. This computes the two fingerprints the layer is
accepted on and drives the two proofs:

  fingerprint DIR --exit N   one run's fingerprint: SHA-256 over all telemetry*.csv + the NORMALISED
                             events.log + the exit code. Normalisation removes exactly two classes of
                             field, both of which say where and when the run happened rather than what
                             it computed: wallS/speedup (host speed) and the absolute path in
                             telemetry= (the --out directory). Nothing else is touched.
  campaign DIR               the campaign fingerprint: SHA-256 over, in campaign order, each mission's
                             own fingerprint, each campaign-state.txt after it, and the campaign exit.
  determinism FBC            criterion 1 — REPS x --threads 1/2/4 runs, one hash expected.
  replay FBC --ref DIR       criterion 2 — every step re-run STANDALONE with the previous step's state
                             file, per-mission fingerprint compared against the campaign's own. This is
                             the statement that the campaign layer adds no hidden state.

A fingerprint is only comparable between runs over the SAME GROUND, so the elevation source is read
from the reference tree's campaign-summary.txt and never guessed; --elev is an OVERRIDE, not a default,
and a tree without the record is refused rather than replayed against an assumption.

Stdlib only. Exit 0 = every comparison held, 1 = at least one diverged.
"""
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys

WALL_FIELDS = re.compile(r" (?:wallS|speedup)=\S+")
TELEMETRY_PATH = re.compile(r"(telemetry=)(\S+)")


def normalise_events(text):
    out = []
    for line in text.splitlines(keepends=True):
        line = WALL_FIELDS.sub("", line)
        line = TELEMETRY_PATH.sub(lambda m: m.group(1) + os.path.basename(m.group(2)), line)
        out.append(line)
    return "".join(out)


def mission_fingerprint(run_dir, exit_code):
    h = hashlib.sha256()
    names = sorted(n for n in os.listdir(run_dir) if n.startswith("telemetry") and n.endswith(".csv"))
    for name in names:
        h.update(name.encode())
        with open(os.path.join(run_dir, name), "rb") as f:
            h.update(f.read())
    with open(os.path.join(run_dir, "events.log"), encoding="utf-8", errors="replace") as f:
        h.update(normalise_events(f.read()).encode())
    h.update(b"exit=%d" % exit_code)
    return h.hexdigest()


def read_summary(campaign_dir):
    """campaign-summary.txt -> (steps, campaign exit, carry mask, environment). Canonical, written by
    the runner. The environment is READ, never guessed: a fingerprint is only comparable between runs
    over the same ground."""
    steps, exit_code, carry = [], None, "units+ground+stores"
    env = {}
    path = os.path.join(campaign_dir, "campaign-summary.txt")
    if not os.path.isfile(path):
        sys.exit("%s: no campaign-summary.txt — not a campaign output tree, or written by a build "
                 "before the environment record existed. Re-run the campaign with the current "
                 "fb-gym; the ground base must be read, never guessed." % campaign_dir)
    with open(path, encoding="utf-8") as f:
        for line in f:
            tok = line.split()
            if not tok:
                continue
            if tok[0] == "mission" and len(tok) >= 6:
                steps.append({"index": int(tok[1]), "file": tok[2], "exit": int(tok[3]),
                              "result": tok[4], "dir": tok[5]})
            elif tok[0] == "exit":
                exit_code = int(tok[1])
            elif tok[0] == "carry":
                carry = tok[1]
            elif tok[0] in ("elev", "swiss_dem", "base", "threads"):
                env[tok[0]] = tok[1] if len(tok) > 1 else ""
    return steps, exit_code, carry, env


def campaign_fingerprint(campaign_dir, verbose=False):
    steps, exit_code, _, _ = read_summary(campaign_dir)
    h = hashlib.sha256()
    per_step = []
    for s in steps:
        run_dir = os.path.join(campaign_dir, s["dir"])
        fp = mission_fingerprint(run_dir, s["exit"])
        per_step.append(fp)
        if verbose:
            print(f"  {s['index']:02d} {s['dir']:<24} exit={s['exit']} {s['result']:<8} {fp[:16]}")
        h.update(fp.encode())
        with open(os.path.join(run_dir, "campaign-state.txt"), "rb") as f:
            h.update(f.read())
    h.update(b"exit=%d" % exit_code)
    return h.hexdigest(), per_step, steps, exit_code


def env_flags(env):
    """The recorded environment as fb-gym flags. Absent = fb-gym's own default, which is what a
    campaign run without --elev used."""
    flags = []
    if env.get("elev"):
        flags += ["--elev", env["elev"]]
    if env.get("elev") == "swiss" and env.get("swiss_dem"):
        flags += ["--swiss-dem", env["swiss_dem"]]
    if env.get("elev") == "tiles" and env.get("base"):
        flags += ["--base", env["base"]]
    return flags


def run_campaign(gym, fbc, out, threads, elev):
    if os.path.isdir(out):
        shutil.rmtree(out)
    cmd = [gym, "--campaign", fbc, "--out", out, "--threads", str(threads)]
    if elev:
        cmd += ["--elev", elev]
    return subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode


def run_mission(gym, mission, out, threads, env, state=None, carry=None):
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    cmd = [gym, "--mission", mission, "--out", out, "--threads", str(threads)] + env_flags(env)
    if state:
        cmd += ["--state", state]
        if carry:
            cmd += ["--carry", carry]
    return subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode


def campaign_missions(fbc):
    base = os.path.dirname(os.path.abspath(fbc))
    paths = []
    with open(fbc, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line.startswith("mission "):
                rel = line[len("mission "):].strip()
                paths.append(rel if os.path.isabs(rel) else os.path.join(base, rel))
    return paths


def cmd_determinism(a):
    fps = {}
    for threads in a.threads:
        for rep in range(1, a.reps + 1):
            out = os.path.join(a.work, f"t{threads}-r{rep}")
            code = run_campaign(a.gym, a.campaign, out, threads, a.elev)
            fp, _, _, _ = campaign_fingerprint(out)
            fps.setdefault(fp, []).append(f"threads={threads} rep={rep} exit={code}")
            print(f"threads={threads} rep={rep} exit={code} campaign-fp={fp}")
    print(f"\n{sum(len(v) for v in fps.values())} runs, {len(fps)} distinct campaign fingerprint(s)")
    for fp, runs in fps.items():
        print(f"  {fp}\n    " + "\n    ".join(runs))
    return 0 if len(fps) == 1 else 1


def cmd_replay(a):
    _, per_step, steps, _ = campaign_fingerprint(a.ref, verbose=False)
    _, _, carry, env = read_summary(a.ref)
    if "elev" not in env:
        print(f"{a.ref}/campaign-summary.txt records no environment (`elev`): this output tree predates\n"
              f"the record and its steps cannot be replayed comparably — re-run the campaign.",
              file=sys.stderr)
        return 1
    if a.elev:
        env = dict(env, elev=a.elev)
        print(f"note: --elev {a.elev} OVERRIDES the recorded '{read_summary(a.ref)[3]['elev']}'")
    carry_list = ",".join(carry.split("+"))
    missions = campaign_missions(a.campaign)
    print(f"replaying against {a.ref} — elev={env['elev']} (recorded)")
    bad = 0
    for i, s in enumerate(steps):
        state = None if i == 0 else os.path.join(a.ref, steps[i - 1]["dir"], "campaign-state.txt")
        out = os.path.join(a.work, f"replay-{s['dir']}")
        code = run_mission(a.gym, missions[i], out, a.threads, env, state, carry_list)
        fp = mission_fingerprint(out, code)
        ok = fp == per_step[i] and code == s["exit"]
        bad += 0 if ok else 1
        print(f"{s['index']:02d} {s['dir']:<24} campaign exit={s['exit']} fp={per_step[i][:16]}  "
              f"standalone exit={code} fp={fp[:16]}  {'MATCH' if ok else 'DIVERGED'}")
        if not ok:
            print(f"     state-in: {state}\n     out: {out}")
    return 0 if bad == 0 else 1


def cmd_fingerprint(a):
    print(mission_fingerprint(a.dir, a.exit))
    return 0


def cmd_campaign(a):
    fp, _, _, exit_code = campaign_fingerprint(a.dir, verbose=True)
    print(f"campaign exit={exit_code} campaign-fp={fp}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("fingerprint")
    p.add_argument("dir")
    p.add_argument("--exit", type=int, required=True)
    p.set_defaults(fn=cmd_fingerprint)

    p = sub.add_parser("campaign")
    p.add_argument("dir")
    p.set_defaults(fn=cmd_campaign)

    for name, fn in (("determinism", cmd_determinism), ("replay", cmd_replay)):
        p = sub.add_parser(name)
        p.add_argument("campaign")
        p.add_argument("--gym", default="build/fb-gym")
        # No default: `determinism` then runs the campaign exactly as fb-gym runs it, and `replay`
        # takes the ground from the reference tree's own record. A default here is what made this
        # tool report four false divergences.
        p.add_argument("--elev", default=None, help="OVERRIDE the elevation source (default: "
                                                    "fb-gym's own for determinism, the recorded one for replay)")
        p.add_argument("--work", default="build/campaign-verify")
        if name == "determinism":
            p.add_argument("--reps", type=int, default=3)
            p.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4])
        else:
            p.add_argument("--ref", required=True)
            p.add_argument("--threads", type=int, default=1)
        p.set_defaults(fn=fn)

    a = ap.parse_args()
    if getattr(a, "work", None):
        os.makedirs(a.work, exist_ok=True)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())

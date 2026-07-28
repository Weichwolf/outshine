#!/usr/bin/env python3
"""The 16-approach BFM/gun sweep — 8 pursuer geometries x {straight, turning} defender.

doc/pilot.md 5.7/5.7.3 measure the BFM roll law against "the 16-approach sweep"; until this file
existed the sweep was a scratch script and every round had to guess it back. This IS the sweep now:
one generated .fbm per cell, run through fb-gym, and the three numbers that carry information.

    python3 tools/fb_bfm_sweep.py OUTDIR

The defender is gun-bfm.fbm's, unchanged (straight east at 300 KCAS, or bfm-basic.fbm's sustained
max-rate left turn around the centre of its own circle). The pursuer is gun-bfm's viper, moved to
eight approach geometries around it and always spawned pointing at it, with the same gun control
band. Cell `trail2-str` reproduces gun-bfm.fbm to the digit.

READ IT LIKE THIS: the KILL COUNT is coarse. Perturbing a spawn by ~1 m flips a cell between kill
and no kill (doc/pilot.md 5.7.3), so a +-2 kill change is chaos. The DEPARTURE COUNT and the pooled
roll-rate statistics are the parts that carry a signal.
"""
import csv
import math
import os
import subprocess
import sys

SIM = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GYM = os.path.join(SIM, "build", "fb-gym")

# gun-bfm.fbm's defender, to the digit.
BLAT, BLON, BALT, BHDG, BSPD = 46.67485, 6.90000, 4000.0, 90.0, 300.0
NM = 1852.0
# gun-bfm.fbm's briefed steerpoint (the fire-control block needs a nav solution behind it).
WLAT, WLON = 46.70000, 6.90000

# name, range nm, bearing from the defender (180 = astern, 90 = its left beam), altitude offset m
GEOM = [
    ("trail2", 2.0, 180.0, 0.0),
    ("trail3", 3.0, 180.0, 0.0),
    ("high", 2.0, 180.0, 1000.0),
    ("low", 2.0, 180.0, -1000.0),
    ("q45l", 2.0, 135.0, 0.0),
    ("q45r", 2.0, 225.0, 0.0),
    ("beaml", 3.0, 90.0, 0.0),
    ("beamr", 3.0, 270.0, 0.0),
]


def offset(lat, lon, brg_deg, dist_m):
    b = math.radians(brg_deg)
    return (lat + dist_m * math.cos(b) / 111320.0,
            lon + dist_m * math.sin(b) / (111320.0 * math.cos(math.radians(lat))))


def bearing(lat1, lon1, lat2, lon2):
    dn = (lat2 - lat1) * 111320.0
    de = (lon2 - lon1) * 111320.0 * math.cos(math.radians(lat1))
    return (math.degrees(math.atan2(de, dn)) + 360.0) % 360.0


def mission(name, rng, brg, dalt, turning):
    plat, plon = offset(BLAT, BLON, (BHDG + brg) % 360.0, rng * NM)
    phdg = bearing(plat, plon, BLAT, BLON)
    if turning:                                   # bfm-basic's bandit: the centre of its own turn
        wlat, wlon = offset(BLAT, BLON, (BHDG - 90.0) % 360.0, 2.8 * NM)
    else:                                         # gun-bfm's bandit: straight ahead, 115 km east
        wlat, wlon = BLAT, 8.40000
    return f"""name sweep-{name}
timeout 600

unit viper
  module f16
  team friendly
  spawn {plat:.5f} {plon:.5f} {BALT + dalt:.0f} {phdg:.1f} 380
  set gear up
  set fuel_pct 60
  set datalink off
  set fcr_mode acm_hud
  set task bfm
  set brief_master_arm arm
  set pilot_bfm_ctrl_min_nm 0.15
  set pilot_bfm_ctrl_max_nm 0.40
  wp {WLAT:.5f} {WLON:.5f} {BALT:.0f} 380

unit bandit
  module f16
  team hostile
  spawn {BLAT:.5f} {BLON:.5f} {BALT:.0f} {BHDG:.1f} {BSPD:.0f}
  set gear up
  set fuel_pct 60
  set datalink off
  set fcr_mode off
  wp {wlat:.5f} {wlon:.5f} {BALT:.0f} {BSPD:.0f}
"""


def roll_stats(path, cap_deg_s):
    """Peak rate, roll flown per 2 s window, longest stretch above 60 deg/s, seconds above the cap."""
    peak = worst = longest = run = above = 0.0
    prev = None
    cum = 0.0
    window = []
    for r in csv.DictReader(open(path)):
        t, roll = float(r["t"]), float(r["rollDeg"])
        if prev is not None and t > prev[0]:
            d = (roll - prev[1] + 180.0) % 360.0 - 180.0
            rate = d / (t - prev[0])
            peak = max(peak, abs(rate))
            cum += d
            window.append(cum)
            if len(window) > 21:
                window.pop(0)
            worst = max(worst, abs(cum - window[0]))
            above += 0.1 if abs(rate) > cap_deg_s else 0.0
            run = run + 0.1 if abs(rate) > 60.0 else 0.0
            longest = max(longest, run)
        prev = (t, roll)
    return peak, worst, longest, above


def run_cell(tag, text, outdir):
    os.makedirs(outdir, exist_ok=True)
    fbm = os.path.join(outdir, tag + ".fbm")
    open(fbm, "w").write(text)
    d = os.path.join(outdir, tag)
    rc = subprocess.run([GYM, "--mission", fbm, "--out", d],
                        capture_output=True, text=True, cwd=SIM).returncode
    events = open(os.path.join(d, "events.log")).read().splitlines()
    kill = any("damage KILL" in l and "unit=bandit" in l for l in events)
    loc = any("monitor KO" in l and "reason=LOC" in l for l in events)
    return (rc, kill, loc) + roll_stats(os.path.join(d, "telemetry.csv"), 90.0)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    kills = {False: 0, True: 0}
    deps = 0
    peak = worst = longest = above = 0.0
    for name, rng, brg, dalt in GEOM:
        for turning in (False, True):
            tag = f"{name}-{'turn' if turning else 'str'}"
            rc, kill, loc, pk, wo, lo, ab = run_cell(
                tag, mission(name, rng, brg, dalt, turning), out)
            kills[turning] += 1 if kill else 0
            deps += 1 if loc else 0
            peak, worst, longest, above = max(peak, pk), max(worst, wo), max(longest, lo), above + ab
            print(f"{tag:<14} rc={rc} {'KILL' if kill else '-   '} {'LOC' if loc else '-  '} "
                  f"peak={pk:6.1f} roll/2s={wo:6.1f} >60run={lo:4.1f}s")
    print(f"\nkills straight={kills[False]}/8 turning={kills[True]}/8 "
          f"= {kills[False] + kills[True]}/16   departures={deps}/16")
    print(f"peak roll {peak:.1f} deg/s ({peak / 90.0:.2f}x the 90 deg/s cap), "
          f"max roll per 2 s window {worst:.1f} deg, longest |w|>60 stretch {longest:.1f} s "
          f"(the monitor trips at 3.0), seconds above the cap {above:.1f}")


if __name__ == "__main__":
    main()

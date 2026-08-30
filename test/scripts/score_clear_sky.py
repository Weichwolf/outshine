#!/usr/bin/env python3
"""Score this tree's sky against a MEASURED one.

Two independent sources travel together here and they check each other before either checks us:
ASTM G173's global-tilt spectrum is CONSTRUCTED to integrate to 1000 W/m^2 -- that is the standard's
own definition of one sun -- and Kider et al.'s clear day at Egbert peaks at its own measured
figure. Where the two agree, the corpus is sound; where our reading of them disagrees with the
published number, our reading is wrong before anything else is.

WHAT THIS DOES NOT GRADE, and it is the larger half: the engine's own sky. That wants the sun's
elevation at each of Kider's samples, which the site and the day supply, and a scenario that
publishes the sky's irradiance for a declared elevation. board:2013 carries it.
"""
import json
import os
import pathlib
import sys

TREE = pathlib.Path(__file__).resolve().parents[2]
kOneSunWPerM2 = 1000.0
kOneSunAllows = 1.0


def prepared_root():
    return pathlib.Path(os.environ.get("TMPDIR", "/tmp")) / "outshine-prepared" / "clearsky-egbert"


def columns(path):
    return [[float(one) for one in line.split()] for line in path.read_text().splitlines() if line.strip()]


def integrated(rows, column):
    summed = 0.0
    for low, high in zip(rows, rows[1:]):
        summed += 0.5 * (low[column] + high[column]) * (high[0] - low[0])
    return summed


def main():
    stated = json.loads((TREE / "test" / "clearsky" / "egbert" / "manifest.json").read_text())
    red = 0
    for name, said in stated["files"].items():
        held = prepared_root() / name
        if not held.exists():
            print(f"UNPREPARED {name}: run test/scripts/fetch_clear_sky.py")
            return 2
        if held.stat().st_size != said["bytes"]:
            print(f"FAIL {name}: {held.stat().st_size} bytes, the manifest pins {said['bytes']}")
            red += 1

    astm = columns(prepared_root() / "astm-g173.txt")
    outside = integrated(astm, 1)
    tilted = integrated(astm, 2)
    direct = integrated(astm, 3)
    apart = abs(tilted - kOneSunWPerM2)
    print(f"ASTM G173 extraterrestrial  {outside:8.2f} W/m2  over {astm[0][0]:.0f}..{astm[-1][0]:.0f} nm")
    print(f"ASTM G173 global tilt       {tilted:8.2f} W/m2  the standard states 1000")
    print(f"ASTM G173 direct+circumsolar{direct:9.2f} W/m2")
    if apart > kOneSunAllows:
        print(f"FAIL the global tilt integrates to {tilted:.2f} and ASTM G173 IS the definition of "
              f"one sun at 1000 W/m2; {apart:.2f} apart is more than the {kOneSunAllows:.0f} this "
              f"trapezoid rule may cost")
        red += 1
    else:
        print(f"HELD one sun integrates to {tilted:.2f}, {apart:.2f} from the standard's 1000")

    kider = columns(prepared_root() / "kider_full_day_irradiance_raw_2013_5_27.txt")
    peak = max(row[1] for row in kider)
    at = max(range(len(kider)), key=lambda one: kider[one][1])
    print(f"Kider's clear day           {peak:8.2f} W/m2 at its peak, sample {at} of {len(kider)}, "
          f"minute {kider[at][0]:.1f}")
    fromEachOther = abs(peak - tilted) / tilted
    if fromEachOther > 0.02:
        print(f"FAIL a measured clear day and the reference one sun stand {fromEachOther * 100:.2f} "
              f"per cent apart; two independent sources of the same quantity do not")
        red += 1
    else:
        print(f"HELD the measured day and the reference one sun agree to "
              f"{fromEachOther * 100:.2f} per cent")

    print("\nNOT GRADED HERE: the engine's own sky. This case pins the ORACLE -- that the files are "
          "what they claim and that our reading of them reproduces the published number -- and an "
          "oracle nobody has checked grades nothing. board:2013 carries the other half.")
    return 1 if red else 0


if __name__ == "__main__":
    sys.exit(main())

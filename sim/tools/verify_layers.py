#!/usr/bin/env python3
"""verify-layers: sim/src is a STACK, and every #include points DOWN it. The gate that keeps the
directory tree honest — a layer that quietly grows an upward dependency stops compiling only much
later, if ever, so the structure is asserted here instead of hoped for.

The layers, bottom to top (RANK below). The order is the measured include graph, not a wish:

  core        value types, the two channels (FBLog/FBTelemetry), the incorruptible judges
              (FBFlightMonitor/FBMissionMonitor/FBSystemHealth/FBDamageModel), the camera/matrix
              maths. Includes NOTHING above itself — that is what makes it the anti-cheat anchor.
  fdm         the JSBSim adapter (one FBFdm per airframe).
  units       world-entity identity + the registry of who exists.
  sensors     the slots that may READ that registry: datalink, radar, RWR, countermeasures.
  weapons     the stores + gun slots.
  systems     the airframe-agnostic flight/avionics slots + the CPU-side HUD geometry/font.
              ABOVE sensors because FBSystemSlots.h aggregates the sensor slots for FBModule.
  pilot       the mission layer above guidance: FBPilot, FBBfmTrack, FBEngagement, FBPilotTuning.
  modules     the airframes composing all of the above.
  missions    the GPU-free mission orchestrator.
  clients     the entry points.
  render      the WebGPU renderer, driven by clients, borrowing systems/ and core/.
  world       FBWorld + tile streaming, on world/terrain (a LEAF: it includes nothing of ours).

Four rules from CLAUDE.md's "Kein Cheaten" become directory rules here, each enforced by name:
  * units/FBUnitRegistry.h reaches ONLY the sensor slots and the missile's uplink RECEIVER.
  * pilot/ includes neither units/ nor sensors/ — a pilot sees the world only through FBState.
  * fdm/FBFdmBoot.h (the only door to a JSBSim IC) is named only by missions/ and clients/.
  * core/ includes nothing above itself.

Stdlib only, no build dependency. Exit 0 = clean, 1 = at least one illegal include.
"""
import argparse
import os
import re
import sys

# Rank per directory, bottom-up. A file may include its own rank and anything BELOW it.
RANK = {
    "world/terrain": 0,   # leaf: pure geometry/mesh, includes nothing of ours
    "core": 1,
    "fdm": 2,
    "units": 3,
    "sensors": 4,
    "weapons": 5,
    "systems": 6,
    "pilot": 7,
    "modules": 8,
    "modules/f16": 8,
    "modules/f16/displays": 8,
    "modules/stores": 8,
    "modules/missile": 8,
    "modules/ground": 8,
    "missions": 9,
    "render": 10,
    "render/stages": 10,
    "world": 10,
    "clients": 11,
}

# Files whose LAYER is not their directory's. Each is a real statement about the graph, not a waiver:
# the file genuinely sits at this rank and is held to it.
FILE_RANK = {
    # FBSimUnit OWNS the FBModule it flies, so it sits above modules/ — while FBUnit.h/
    # FBUnitRegistry.h (the identity + "who exists" that the sensor slots read) sit below them.
    # One directory, two layers; splitting the directory would separate a unit from its own registry.
    "units/FBSimUnit.h": 8.5,
    "units/FBSimUnit.cpp": 8.5,
    # The I/O edge of core's two channels. Deliberately NOT in core (CLAUDE.md: core stays I/O-free),
    # but attached by the mission orchestrator as well as by every client, so they cannot BE the
    # client layer. They include core and nothing else, and this rank pins that.
    "clients/FBLogSinks.h": 1.5,
    "clients/FBLogSinks.cpp": 1.5,
    "clients/FBTelemetrySinks.h": 1.5,
    "clients/FBTelemetrySinks.cpp": 1.5,
}

# Named exceptions: (includer-pattern, included-file) pairs allowed against the rank order.
# EMPTY on purpose — every placement was resolved by moving the file, not by waiving the rule.
EXCEPTIONS = ()

# The include is legal by rank AND the target is restricted to an explicit list of includers OUTSIDE
# its own directory (inside it, the header is the seam's own implementation detail — FBFdm.cpp is
# FBFdmBoot's friend by declaration, so hiding the header from it would be theatre).
RESTRICTED = {
    "units/FBUnitRegistry.h": (
        # Cross-unit truth reaches only simulated SENSORS (CLAUDE.md "Kein Cheaten").
        "sensors/FBDatalinkSystem.cpp",
        "sensors/FBRadarSystem.cpp",
        "sensors/FBRwrSystem.cpp",
        "modules/missile/FBMissileUplink.cpp",  # a RECEIVER listening to a published emission
        "missions/FBMissionRunner.h",
        "missions/FBMissionRunner.cpp",
        "missions/FBMissionBoot.h",
        "clients/FBAppWasm.cpp",
    ),
    "fdm/FBFdmBoot.h": (
        # The only door to a JSBSim initial condition: mission boot and the test harnesses.
        "missions/FBMissionBoot.h",
        "clients/FBAppWasm.cpp",
        "clients/FBTestCornerSpeed.cpp",
        "clients/FBTestHardLanding.cpp",
        "clients/FBTestLocDeparture.cpp",
        "clients/FBTestMissileAirframe.cpp",
        "clients/FBTestTwoFdm.cpp",
    ),
}

# Layers that may not appear ANYWHERE in a directory's include closure, regardless of rank.
FORBIDDEN_DIRS = {
    # A pilot sees other units only through FBState, written by the sensor slots.
    "pilot": ("units", "sensors"),
}

SRC_EXT = (".h", ".cpp")


def scan(root):
    """path -> (layer-dir, set of quoted include basenames). Paths are root-relative, '/'-separated."""
    files = {}
    for dp, dns, fns in os.walk(root):
        dns[:] = [d for d in dns if not d.startswith(".")]
        for fn in fns:
            if not fn.endswith(SRC_EXT):
                continue
            p = os.path.relpath(os.path.join(dp, fn), root).replace(os.sep, "/")
            with open(os.path.join(dp, fn), encoding="utf-8", errors="replace") as f:
                incs = re.findall(r'^\s*#\s*include\s+"([^"]+)"', f.read(), re.M)
            files[p] = incs
    return files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.join(os.path.dirname(__file__), "..", "src"))
    a = ap.parse_args()
    root = os.path.normpath(a.root)
    files = scan(root)

    # The build uses flat -I roots and bare filenames, so an include resolves by BASENAME.
    by_base = {}
    for p in files:
        by_base.setdefault(os.path.basename(p), []).append(p)
    dup = {b: v for b, v in by_base.items() if len(v) > 1}

    def rank_of(path):
        if path in FILE_RANK:
            return FILE_RANK[path]
        d = os.path.dirname(path)
        if d not in RANK:
            return None
        return RANK[d]

    errors = []
    for b, v in sorted(dup.items()):
        errors.append(f"basename collision on a flat -I include path: {b} -> {', '.join(sorted(v))}")

    unknown = sorted({os.path.dirname(p) for p in files if rank_of(p) is None})
    for d in unknown:
        errors.append(f"directory '{d}' has no layer: add it to RANK in tools/verify_layers.py")
    if errors:
        return report(errors)

    n_edges = 0
    for p in sorted(files):
        pr = rank_of(p)
        for inc in files[p]:
            cands = by_base.get(os.path.basename(inc))
            if not cands:
                continue  # external / vendored header
            t = cands[0]
            if t == p:
                continue
            n_edges += 1
            tr = rank_of(t)
            if tr > pr and (p, t) not in EXCEPTIONS:
                errors.append(f"UPWARD include: {p} -> {t} (rank {pr} -> {tr})")
            allowed = RESTRICTED.get(t)
            same_dir = os.path.dirname(p) == os.path.dirname(t)
            if allowed is not None and not same_dir and p not in allowed:
                errors.append(f"RESTRICTED header: {p} -> {t} is not on that header's includer list")
            banned = FORBIDDEN_DIRS.get(os.path.dirname(p), ())
            if os.path.dirname(t).split("/")[0] in banned:
                errors.append(f"FORBIDDEN layer: {p} -> {t} ({os.path.dirname(p)}/ may not see "
                              f"{os.path.dirname(t).split('/')[0]}/)")

    if errors:
        return report(errors)
    print(f"verify-layers: {len(files)} files, {n_edges} internal include(s), "
          f"{len(set(RANK.values()))} layers — no upward include, "
          f"{len(RESTRICTED)} restricted header(s) respected")
    return 0


def report(errors):
    for e in errors:
        print(f"verify-layers: {e}", file=sys.stderr)
    print(f"verify-layers: FAILED ({len(errors)} violation(s))", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""verify-layers: sim/src is a STACK, every #include points DOWN it, and every file declares the
NAMESPACE of the layer it lies in. The gate that keeps the directory tree honest — a layer that
quietly grows an upward dependency stops compiling only much later, if ever, so the structure is
asserted here instead of hoped for.

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

The namespace half (NAMESPACE below) says the same thing to a reader that the include half says to
the build: core/ IS the root `namespace FlightBox` — its value types are everywhere, and nesting them
would be noise — while every layer ABOVE it nests one level, so a cross-layer name carries its layer
at the point of use (`Fdm::FBFdm`, `Systems::FBAutopilot`, `Units::FBUnitRegistry`).

Stdlib only, no build dependency. Exit 0 = clean, 1 = at least one illegal include or namespace.
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
        "clients/FBTestMig29Envelope.cpp",
        "clients/FBTestMissileAirframe.cpp",
        "clients/FBTestTwoFdm.cpp",
    ),
}

# Layers that may not appear ANYWHERE in a directory's include closure, regardless of rank.
FORBIDDEN_DIRS = {
    # A pilot sees other units only through FBState, written by the sensor slots.
    "pilot": ("units", "sensors"),
}

# The namespace a directory's files declare. core/ IS the root: its value types (FBState, FBLog,
# FBGeodesy, the judges) appear in every layer's signatures, and nesting them would add a qualifier
# to every line without adding information. Everything ABOVE core nests exactly one level, so a
# cross-layer name carries its layer where it is USED, not where it is declared.
LAYER_NS = {
    "world/terrain": None,   # C island, see C_ISLAND
    "core": "FlightBox",
    "fdm": "FlightBox::Fdm",
    "units": "FlightBox::Units",
    "sensors": "FlightBox::Sensors",
    "weapons": "FlightBox::Weapons",
    "systems": "FlightBox::Systems",
    "pilot": "FlightBox::Pilot",
    "modules": "FlightBox::Modules",
    "modules/f16": "FlightBox::Modules",
    "modules/f16/displays": "FlightBox::Modules",
    "modules/stores": "FlightBox::Modules",
    "modules/missile": "FlightBox::Modules",
    "modules/ground": "FlightBox::Modules",
    "missions": "FlightBox::Missions",
    "render": "FlightBox::Render",
    "render/stages": "FlightBox::Render",
    "world": "FlightBox::World",
    "clients": "FlightBox::Clients",
}
KNOWN_NS = {v for v in LAYER_NS.values() if v}

# modules/ deliberately does NOT get a second level per airframe. FlightBox::Modules::F16 would be
# the only member with more than a handful of files, the FB-prefixed class names already carry the
# airframe (FBF16Fcr, FBStoreModule, FBGroundModule), and nothing outside modules/ names any of them:
# the whole layer is reached through FBModule* and the registry's string key.

# The C ISLAND: files that are not C++ FlightBox code but a C-shaped seam, and whose namelessness is
# therefore the point, not an omission. A namespace here would either break the contract or lie about
# it. Each entry names the contract it serves. (world/terrain/* is the same statement made by
# directory, above: LAYER_NS[world/terrain] is None.)
C_ISLAND = {
    # The tile-streaming C ABI (fb_terrain_*/fb_stream_*), whose header is `extern "C"`-guarded and
    # whose declarations must stay unmangled for the browser side that calls them.
    "world/FBTerrainLoader.h",
    "world/FBTerrainLoader.cpp",
    # The tile worker is its OWN wasm module; every _fbtw_* in the Makefile's EXPORTED_FUNCTIONS is
    # an extern "C" definition in this TU, and a mangled name would silently fail to export.
    "clients/FBTileWorkerMain.cpp",
    # Force-included (emcc -include) into the PINNED JSBSim sources so the submodule stays vanilla:
    # it must be valid in whatever translation unit it lands in, including C ones.
    "fdm/em_compat.h",
    # A standalone static-file host: its own binary, no FlightBox type in it, C throughout.
    "clients/FBSimHost.cpp",
}

# A TU that defines the global `main` cannot be wrapped — C++ requires main at global scope. That is
# a law, not a waiver, so it is DETECTED rather than listed: such a TU reaches FlightBox through a
# file-scope `using namespace` and keeps its own helpers in an anonymous namespace.
RE_MAIN = re.compile(r"^\s*(?:int|auto)\s+main\s*\(", re.M)
# `namespace [X] {` opening a block at column 0. A one-liner that also CLOSES on its line is a forward
# declaration (`namespace FlightBox::Units { class FBUnit; }`) and is judged separately. The tree's
# own `} // namespace ...` marker is what closes one here (every file that opens one carries it), so
# nesting is tracked without parsing C++: only a DEPTH-0 opener names the file's layer.
RE_NS_OPEN = re.compile(r"^namespace\s*([A-Za-z_][A-Za-z0-9_:]*)?\s*\{(.*)$")
RE_NS_CLOSE = re.compile(r"^\}\s*//\s*namespace")
RE_USING_NS = re.compile(r"^\s*using\s+namespace\s+([A-Za-z_][A-Za-z0-9_:]*)\s*;", re.M)


def ns_openers(src):
    """(name, depth) for every column-0 namespace opener; name None = anonymous. One-line forward
    declarations are returned separately — they open no scope."""
    opens, fwd, depth = [], [], 0
    for line in src.splitlines():
        m = RE_NS_OPEN.match(line)
        if m:
            if "}" in m.group(2):
                fwd.append(m.group(1))
            else:
                opens.append((m.group(1), depth))
                depth += 1
        elif RE_NS_CLOSE.match(line):
            depth = max(0, depth - 1)
    return opens, fwd

SRC_EXT = (".h", ".cpp")


def scan(root):
    """path -> (quoted include names, full text). Paths are root-relative, '/'-separated."""
    files, text = {}, {}
    for dp, dns, fns in os.walk(root):
        dns[:] = [d for d in dns if not d.startswith(".")]
        for fn in fns:
            if not fn.endswith(SRC_EXT):
                continue
            p = os.path.relpath(os.path.join(dp, fn), root).replace(os.sep, "/")
            with open(os.path.join(dp, fn), encoding="utf-8", errors="replace") as f:
                src = f.read()
            files[p] = re.findall(r'^\s*#\s*include\s+"([^"]+)"', src, re.M)
            text[p] = src
    return files, text


def check_namespaces(files, text):
    """Every file declares the namespace of the layer it lies in — see the module docstring."""
    errors, checked = [], 0
    # RANK and LAYER_NS are two statements about the same set of directories; a layer known to one
    # and not the other would fall through this check silently, so they are pinned to each other.
    for d in sorted(set(RANK) ^ set(LAYER_NS)):
        errors.append(f"NAMESPACE: directory '{d}' is in RANK or LAYER_NS but not both")
    for p in sorted(files):
        d = os.path.dirname(p)
        want = LAYER_NS.get(d)
        src = text[p]

        # `using namespace` in a HEADER leaks into every translation unit that includes it, so the
        # qualifier a layer boundary is made of would vanish at exactly the call sites that need it.
        for m in RE_USING_NS.finditer(src):
            if p.endswith(".h"):
                errors.append(f"NAMESPACE: {p} has a file-scope `using namespace {m.group(1)};` in a "
                              f"header — it leaks into every includer")

        opens, fwd = ns_openers(src)
        top = [n for n, depth in opens if depth == 0 and n]
        if p in C_ISLAND or want is None:
            if top:
                errors.append(f"NAMESPACE: {p} is on the C island but opens `namespace {top[0]}` — "
                              f"take it off the island or off the C ABI")
            continue
        checked += 1
        for name in fwd:
            if name not in KNOWN_NS:
                errors.append(f"NAMESPACE: {p} forward-declares into `{name}`, which is no layer's "
                              f"namespace")
        for name in top:
            if name != want:
                errors.append(f"NAMESPACE: {p} opens `namespace {name}` at file scope but lies in "
                              f"{d}/ ({want})")
        if want not in top and not RE_MAIN.search(src):
            errors.append(f"NAMESPACE: {p} declares no `namespace {want}` (its layer's)")
    return errors, checked


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.join(os.path.dirname(__file__), "..", "src"))
    a = ap.parse_args()
    root = os.path.normpath(a.root)
    files, text = scan(root)

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

    ns_errors, n_ns = check_namespaces(files, text)
    errors += ns_errors

    if errors:
        return report(errors)
    print(f"verify-layers: {len(files)} files, {n_edges} internal include(s), "
          f"{len(set(RANK.values()))} layers — no upward include, "
          f"{len(RESTRICTED)} restricted header(s) respected, "
          f"{n_ns} file(s) in their layer's namespace ({len(C_ISLAND)} C-island file(s) exempt)")
    return 0


def report(errors):
    for e in errors:
        print(f"verify-layers: {e}", file=sys.stderr)
    print(f"verify-layers: FAILED ({len(errors)} violation(s))", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""verify-layers: sim/src (plus sim/test above it) is a STACK, every #include points DOWN it, and
every file declares the NAMESPACE of the layer it lies in. The gate that keeps the directory tree honest — a layer that
quietly grows an upward dependency stops compiling only much later, if ever, so the structure is
asserted here instead of hoped for.

The layers, bottom to top (RANK below). The order is the measured include graph, not a wish:

  core        value types, the two channels (Log/Telemetry), the incorruptible judges
              (FlightMonitor/MissionMonitor/SystemHealth/DamageModel), the camera/matrix
              maths. Includes NOTHING above itself — that is what makes it the anti-cheat anchor.
  fdm         the JSBSim adapter (one Fdm per airframe).
  units       world-entity identity + the registry of who exists.
  sensors     the slots that may READ that registry: datalink, radar, RWR, IRST, visual,
              countermeasures.
  weapons     the stores + gun slots.
  systems     the airframe-agnostic flight/avionics slots + the CPU-side HUD geometry/font.
              ABOVE sensors because SystemSlots.h aggregates the sensor slots for Module.
  pilot       the mission layer above guidance: Pilot, BfmTrack, Engagement, PilotTuning.
  modules     the airframes composing all of the above.
  missions    the GPU-free mission orchestrator.
  clients     the entry points.
  render      the WebGPU renderer, driven by clients, borrowing systems/ and core/.
  world       World + tile streaming, on world/terrain (a LEAF: it includes nothing of ours).
  test        sim/test, mirroring sim/src path for path (make verify-trees) and ranked above every
              layer it judges. A harness may reach anything; nothing may reach a harness.

Four rules from CLAUDE.md's "Kein Cheaten" become directory rules here, each enforced by name:
  * units/UnitRegistry.h reaches ONLY the sensor slots (datalink, radar, RWR, IRST, visual) and
    the missile's uplink RECEIVER.
  * pilot/ includes neither units/ nor sensors/ — a pilot sees the world only through State.
  * fdm/FdmBoot.h (the only door to a JSBSim IC) is named only by missions/ and clients/.
  * core/ includes nothing above itself.

The namespace half (NAMESPACE below) says the same thing to a reader that the include half says to
the build: core/ IS the root `namespace outshine` — its value types are everywhere, and nesting them
would be noise — while every layer ABOVE it nests one level, so a cross-layer name carries its layer
at the point of use (`Fdm::Fdm`, `Systems::Autopilot`, `Units::UnitRegistry`).

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
    "modules/mig29": 8,
    "modules/stores": 8,
    "modules/missile": 8,
    "modules/ground": 8,
    "modules/air": 8,
    "missions": 9,
    "render": 10,
    "render/stages": 10,
    "world": 10,
    "clients": 11,
    # test/ mirrors src/ (make verify-trees) and sits ABOVE every layer it judges: a harness may reach
    # anything, and nothing may reach a harness. The path carries the SUBJECT — test/modules/f16 judges
    # src/modules/f16 — so an include that points up the stack is still caught inside the test tree.
    "test/core": 12,
    "test/fdm": 12,
    "test/weapons": 12,
    "test/modules/f16": 12,
    "test/modules/mig29": 12,
    "test/modules/air": 12,
    "test/modules/missile": 12,
}

# Files whose LAYER is not their directory's. Each is a real statement about the graph, not a waiver:
# the file genuinely sits at this rank and is held to it.
FILE_RANK = {
    # SimUnit OWNS the Module it flies, so it sits above modules/ — while Unit.h/
    # UnitRegistry.h (the identity + "who exists" that the sensor slots read) sit below them.
    # One directory, two layers; splitting the directory would separate a unit from its own registry.
    "units/SimUnit.h": 8.5,
    "units/SimUnit.cpp": 8.5,
    # The I/O edge of core's two channels. Deliberately NOT in core (CLAUDE.md: core stays I/O-free),
    # but attached by the mission orchestrator as well as by every client, so they cannot BE the
    # client layer. They include core and nothing else, and this rank pins that.
    "clients/LogSinks.h": 1.5,
    "clients/LogSinks.cpp": 1.5,
    "clients/TelemetrySinks.h": 1.5,
    "clients/TelemetrySinks.cpp": 1.5,
}

# Named exceptions: (includer-pattern, included-file) pairs allowed against the rank order.
# EMPTY on purpose — every placement was resolved by moving the file, not by waiving the rule.
EXCEPTIONS = ()

# The include is legal by rank AND the target is restricted to an explicit list of includers OUTSIDE
# its own directory (inside it, the header is the seam's own implementation detail — Fdm.cpp is
# FdmBoot's friend by declaration, so hiding the header from it would be theatre).
# THE PERCEPTION BOUNDARY, as a LIST and not as a sentence: the files that may read who exists in the
# world. Every one of them is a simulated SENSOR (or a weapon's RECEIVER), and each pays a stated price
# for the privilege — doc/sensors.md 1.2/9.2. Its LENGTH is the number the whole anti-cheat promise
# hangs on, so it is printed at the end of a run: a reader added anywhere in the tree moves it, and a
# human reading the gate's output sees that it moved.
PERCEPTION_READERS = (
    "sensors/DatalinkSystem.cpp",
    "sensors/RadarSystem.cpp",
    "sensors/RwrSystem.cpp",
    "sensors/IrstSystem.cpp",
    "sensors/VisualSystem.cpp",
    "modules/missile/MissileUplink.cpp",  # a RECEIVER listening to a published emission
)

# The OWNER side of the same header, which is a different thing and is deliberately counted separately:
# the client that OWNS the simulation builds the registry and hands it out. It perceives nothing.
REGISTRY_OWNERS = (
    # THE SIMULATION LOOP. It builds nothing and reads nothing out of the registry: it hands the borrowed
    # list DOWN to the modules' sensor slots on the step it owns, which is the owner's role and not a
    # perceiver's.
    "missions/MissionSim.h",
    "missions/MissionSim.cpp",
    "missions/MissionRunner.h",
    "missions/MissionRunner.cpp",
    "missions/MissionBoot.h",
    # The owner's ordnance book: a released store BECOMES a unit, so the thing that creates it is the
    # thing that must enter it in the registry. It reads nothing out of it -- Register() is the only
    # member it names -- which is exactly what separates this list from PERCEPTION_READERS above.
    "missions/Ordnance.cpp",
    "clients/AppWasm.cpp",
)

# THE THIRD CATEGORY, and it is neither a perceiver nor an owner: the DRAWING side. World turns the
# registry's PUBLISHED poses into render/UnitDraw records once a frame so the picture can show the
# cast the simulation already has. It is counted separately because the price it pays is different:
# it reads everything a unit publishes, degrades none of it, and cannot feed anything back — an
# UnitDraw carries no simulation type, and render/ sits ABOVE modules/ and pilot/ in the rank order,
# so no module and no pilot can reach it. A reader added here does NOT widen what an AI may know; one
# added to PERCEPTION_READERS does, which is why the two counts stay apart.
DRAW_VIEWERS = (
    "world/World.cpp",
)

# The include is legal by rank AND the target is restricted to an explicit list of includers OUTSIDE
# its own directory (inside it, the header is the seam's own implementation detail — Fdm.cpp is
# FdmBoot's friend by declaration, so hiding the header from it would be theatre).
RESTRICTED = {
    "units/UnitRegistry.h": PERCEPTION_READERS + REGISTRY_OWNERS + DRAW_VIEWERS,
    # A DECLARED BELT is judge data. core/ reaches it inside its own directory (MissionFile parses it,
    # MissionMonitor judges it); OUTSIDE core/ nobody may name it at all -- not a module, not a pilot,
    # not a sensor. This entry is a NARROWING: a pilot able to read a declared zone would know where the
    # SAMs are without a sensor. doc/air-defence-network.md 7.
    "core/Zone.h": (),
    "fdm/FdmBoot.h": (
        # The only door to a JSBSim initial condition: mission boot and the test harnesses.
        "missions/MissionBoot.h",
        "clients/AppWasm.cpp",
        "test/core/TestHardLanding.cpp",
        "test/core/TestLocDeparture.cpp",
        "test/fdm/TestTwoFdm.cpp",
        "test/modules/f16/TestCornerSpeed.cpp",
        "test/modules/mig29/TestMig29Envelope.cpp",
        "test/modules/air/TestAirEnvelope.cpp",
        "test/modules/missile/TestMissileAirframe.cpp",
    ),
}

# WHERE AN ANTENNA MAY BE POINTED FROM, and it is one file. An antenna command is BODY-referenced;
# three rounds in a row wrote a WORLD angle into one (a controller's true bearing, a range-angle
# elevation, a spawn-tick pose that was still the identity — doc/pilot.md 2.15, doc/sensors.md). Every
# one of them compiled, because both frames are `double` and both are degrees.
#
# So the frame became a TYPE (core/BodyAngle.h, obtainable only through a named conversion) and the
# posting became a single door (CommandBus::PostAntennaAz/El). This list is the door's own guard: the
# tokens RadarSlewAz/RadarSlewEl may appear in a POST expression in exactly these files. A fourth cue
# source cannot reach the antenna without either going through the conversion or moving this number —
# and the number is printed at the end of a run, exactly like the registry-reader count above it.
SLEW_POSTERS = ("core/CommandBus.h",)
RE_SLEW_POST = re.compile(r"Post\w*\(\s*CommandTarget::RadarSlew(?:Az|El)")

# WHO DRIVES A SIMULATION TICK, and it is ONE file. A client that steps units itself is a client that
# writes its own loop, and a second loop is a second set of rules: the browser had one, forgot the end
# rule in it, and flew a CFIT'd F-16 on while the frame loop kept integrating. Since then the tick
# surface of units/SimUnit is private with a single friend (missions/MissionSim), so a second driver
# does not COMPILE — this list is that guarantee's readable half, and its LENGTH is printed at the end
# of a run exactly like the perception-reader count above: a driver added anywhere moves the number.
# units/SimUnit.* are the definition site and name these members without calling them on an object.
TICK_DRIVERS = ("missions/MissionSim.cpp",)
RE_TICK_CALL = re.compile(r"(?:->|\.)\s*(?:PublishPose|PrimeState|RunMonitors|FinalizeMission|"
                          r"CheckEnvelope|UpdateGroundAsl|UpdateWind|UpdateSky|UpdateSolar)\s*\(")
TICK_DEFINITION = ("units/SimUnit.h", "units/SimUnit.cpp")

# Layers that may not appear ANYWHERE in a directory's include closure, regardless of rank.
FORBIDDEN_DIRS = {
    # A pilot sees other units only through State, written by the sensor slots.
    "pilot": ("units", "sensors"),
}

# The namespace a directory's files declare. core/ IS the root: its value types (State, Log,
# Geodesy, the judges) appear in every layer's signatures, and nesting them would add a qualifier
# to every line without adding information. Everything ABOVE core nests exactly one level, so a
# cross-layer name carries its layer where it is USED, not where it is declared.
LAYER_NS = {
    "world/terrain": None,   # C island, see C_ISLAND
    "core": "outshine",
    "fdm": "outshine::Fdm",
    "units": "outshine::Units",
    "sensors": "outshine::Sensors",
    "weapons": "outshine::Weapons",
    "systems": "outshine::Systems",
    "pilot": "outshine::Pilot",
    "modules": "outshine::Modules",
    "modules/f16": "outshine::Modules",
    "modules/f16/displays": "outshine::Modules",
    "modules/mig29": "outshine::Modules",
    "modules/stores": "outshine::Modules",
    "modules/missile": "outshine::Modules",
    "modules/ground": "outshine::Modules",
    "modules/air": "outshine::Modules",
    "missions": "outshine::Missions",
    "render": "outshine::Render",
    "render/stages": "outshine::Render",
    "world": "outshine::World",
    "clients": "outshine::Clients",
    # One namespace for the whole test tree: a harness is a `main` (exempt by law, see RE_MAIN) and the
    # few headers beside them are declaration tables, not a layer anyone links against.
    "test/core": "outshine::Test",
    "test/fdm": "outshine::Test",
    "test/weapons": "outshine::Test",
    "test/modules/f16": "outshine::Test",
    "test/modules/mig29": "outshine::Test",
    "test/modules/air": "outshine::Test",
    "test/modules/missile": "outshine::Test",
}
KNOWN_NS = {v for v in LAYER_NS.values() if v}

# modules/ deliberately does NOT get a second level per airframe. outshine::Modules::F16 would be
# the only member with more than a handful of files, the FB-prefixed class names already carry the
# airframe (FBF16Fcr, StoreModule, GroundModule), and nothing outside modules/ names any of them:
# the whole layer is reached through Module* and the registry's string key.

# The C ISLAND: files that are not C++ outshine code but a C-shaped seam, and whose namelessness is
# therefore the point, not an omission. A namespace here would either break the contract or lie about
# it. Each entry names the contract it serves. (world/terrain/* is the same statement made by
# directory, above: LAYER_NS[world/terrain] is None.)
C_ISLAND = {
    # The tile-streaming C ABI (fb_terrain_*/fb_stream_*), whose header is `extern "C"`-guarded and
    # whose declarations must stay unmangled for the browser side that calls them.
    "world/TerrainLoader.h",
    "world/TerrainLoader.cpp",
    # The tile worker is its OWN wasm module; every _fbtw_* in the Makefile's EXPORTED_FUNCTIONS is
    # an extern "C" definition in this TU, and a mangled name would silently fail to export.
    "clients/TileWorkerMain.cpp",
    # Force-included (emcc -include) into the PINNED JSBSim sources so the submodule stays vanilla:
    # it must be valid in whatever translation unit it lands in, including C ones.
    "fdm/em_compat.h",
    # A standalone static-file host: its own binary, no outshine type in it, C throughout.
    "clients/SimHost.cpp",
}

# A TU that defines the global `main` cannot be wrapped — C++ requires main at global scope. That is
# a law, not a waiver, so it is DETECTED rather than listed: such a TU reaches outshine through a
# file-scope `using namespace` and keeps its own helpers in an anonymous namespace.
RE_MAIN = re.compile(r"^\s*(?:int|auto)\s+main\s*\(", re.M)
# `namespace [X] {` opening a block at column 0. A one-liner that also CLOSES on its line is a forward
# declaration (`namespace outshine::Units { class Unit; }`) and is judged separately. The tree's
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


def scan(root, prefix=""):
    """path -> (quoted include names, full text). Paths are root-relative, '/'-separated; the test
    tree carries a `test/` prefix so one path space covers both roots."""
    files, text = {}, {}
    for dp, dns, fns in os.walk(root):
        dns[:] = [d for d in dns if not d.startswith(".")]
        for fn in fns:
            if not fn.endswith(SRC_EXT):
                continue
            p = prefix + os.path.relpath(os.path.join(dp, fn), root).replace(os.sep, "/")
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
    ap.add_argument("--test-root", default=os.path.join(os.path.dirname(__file__), "..", "test"))
    a = ap.parse_args()
    root = os.path.normpath(a.root)
    files, text = scan(root)
    test_files, test_text = scan(os.path.normpath(a.test_root), "test/")
    files.update(test_files)
    text.update(test_text)

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

    for p in sorted(text):
        if RE_TICK_CALL.search(text[p]) and p not in TICK_DRIVERS and p not in TICK_DEFINITION:
            errors.append(f"SECOND SIM LOOP: {p} steps units itself — one tick body and one end rule "
                          f"live in missions/MissionSim; a client ASKS it to advance (see that "
                          f"header)")
        if RE_SLEW_POST.search(text[p]) and p not in SLEW_POSTERS:
            errors.append(f"ANTENNA FRAME: {p} posts a RadarSlew target directly — an antenna command "
                          f"is body-referenced, so it goes through CommandBus::PostAntennaAz/El with "
                          f"a core/BodyAngle (see that header)")

    if errors:
        return report(errors)
    print(f"verify-layers: {len(files)} files, {n_edges} internal include(s), "
          f"{len(set(RANK.values()))} layers — no upward include, "
          f"{len(RESTRICTED)} restricted header(s) respected, "
          f"{len(PERCEPTION_READERS)} registry reader(s) inside the perception boundary, "
          f"{len(DRAW_VIEWERS)} drawing-side viewer(s), "
          f"{len(SLEW_POSTERS)} antenna-cue poster(s), "
          f"{len(TICK_DRIVERS)} simulation-loop driver(s), "
          f"{n_ns} file(s) in their layer's namespace ({len(C_ISLAND)} C-island file(s) exempt)")
    return 0


def report(errors):
    for e in errors:
        print(f"verify-layers: {e}", file=sys.stderr)
    print(f"verify-layers: FAILED ({len(errors)} violation(s))", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

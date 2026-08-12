#!/usr/bin/env python3
"""verify-clients: AN ENTRY POINT IS AN ENTRY POINT.

The defect this gate exists for, measured 2026-08-08 on 3827f7e: one entry point named the tree 72
times and the second named it 0 times. One of the two clients had never drawn a tree, and both builds
were green the whole time, because a green build proves that a client COMPILES, not that it SHOWS the
same thing. The second client is gone; what put the world into one main() instead of into the one
builder has not, so the three rules stand.

  1. AN ENTRY POINT BUILDS NOTHING. It may not include a render/ or world/ header and may not name
     `Renderer` or `World::World`. It constructs the system and hands it the output medium.

  2. `main()` IS SHORT. C++ Core Guidelines F.3. The ceiling is a number here so that "it grew a
     little" is a failing build and not a matter of taste.

  3. THE SCENE IS BUILT IN EXACTLY ONE TRANSLATION UNIT. Every call that feeds the renderer a piece
     of the world -- the vegetation table, the sky and wind clocks, the tree ranks, the stands, the
     impostor, the moon, the stars, the camera basis, the scene state -- may appear only in
     `src/clients/Outshine.cpp`, which is that place. A bench that reaches past the system to set up
     a SUBJECT is named, counted and printed, so a new one shows up in a diff.

Stdlib only, no build dependency. Exit 0 = clean, 1 = at least one violation.
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
# The library and the tests over it. Both are scanned, because a scene built in a test is a second
# builder exactly as a scene built in a second library file would be.
TREES = ("src", "test")

ENTRY_POINTS = ("AppWalk.cpp",)
# An entry point's whole world. Everything else it might want is Outshine's or the bench's.
ENTRY_INCLUDES = {"Log.h", "LogSinks.h", "Outshine.h", "Walker.h", "Scene.h", "Snapshot.h",
                  "Mod.h", "Stage.h", "SceneRunner.h", "Artifacts.h", "FileArtifacts.h", "Env.h",
                  # WHERE THE RUN'S TEXT GOES. A path, stdout or stderr, named by the consumer and
                  # by nothing else -- the log and the state channel are the entry point's to place.
                  "TextTarget.h", "LogSinks.h", "CsvTelemetry.h", "Sanitisers.h",
                  # THE HOST MEDIA. An output medium and a wire are the two things an entry point
                  # supplies and the library declares but never implements; the delay decorator over
                  # the wire is what imposes an arrival order for the still gate, in process.
                  "CurlTransport.h", "DelayedTransport.h",
                  # The run's name plate. Only an entry point knows which mod and which scene it is —
                  # none of that is a piece of the world.
                  "RunIdentity.h"}
# F.3, and the owner's order of magnitude. Deliberately not generous.
MAIN_LINES_MAX = 40
# Types an entry point may not name: naming one means it is building the thing instead of asking for
# it.
ENTRY_FORBIDDEN = ("Renderer", "World::World", "VegetationTemplates", "GroundMaterials",
                   "TreeGrower", "TreeField", "Forest", "fb_stream_", "TerrainLoader")

# THE SCENE-BUILDING SURFACE. Each token is a renderer call that puts a piece of the world into the
# picture; the round that added trees to one client and not to the other added seven of them to one
# main().
BUILD_CALLS = re.compile(
    r"\.(SetVegetationTable|SetSkyClock|SetWind|SetWindClock|SetExposure|SetFovDeg|SetOrthoM|"
    r"SetPrototypeLevel|SetPrototypeSheets|SetPrototypeDetail|SetPrototypeMaterial|"
    r"SetPrototypeHeightM|SetPrototypeBounds|SetPrototypeSubject|SetPrototypeInstances|"
    r"BakePrototypeImpostor|SetMoonTexture|SetStars|SetCameraBasis|SetSceneState)\s*\(")
# The ONE place a scene is built. `world/` was on this list while it still drove the renderer; the
# headless target is what removed that, so a scene-building call under world/ is now a stray.
BUILDERS = ("src/clients/Outshine.cpp",)
# THE SUBJECT BENCH reaches past the system on purpose: it replaces the world and the light with
# declared ones and judges a single plant. Its length is printed so that a second one moves a number
# in a diff.
BENCH_BUILDERS = ("src/clients/SceneRunner.cpp", "src/clients/SubjectBench.cpp")

RE_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
RE_MAIN = re.compile(r"^(?:int|auto)\s+main\s*\(", re.M)


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def main_body_lines(src):
    """Lines from `int main(` to the closing brace at column 0."""
    m = RE_MAIN.search(src)
    if not m:
        return None
    lines = src[m.start():].splitlines()
    for i, line in enumerate(lines):
        if i and line.startswith("}"):
            return i + 1
    return len(lines)


def scan():
    files = {}
    for tree in TREES:
        for dp, dns, fns in os.walk(os.path.join(ROOT, tree)):
            dns[:] = [d for d in dns if not d.startswith(".")]
            for fn in fns:
                if fn.endswith((".h", ".cpp")):
                    p = os.path.relpath(os.path.join(dp, fn), ROOT).replace(os.sep, "/")
                    files[p] = read(os.path.join(dp, fn))
    return files


def main():
    files = scan()
    errors = []

    entries = [p for p in files if os.path.basename(p) in ENTRY_POINTS]
    if len(entries) != len(ENTRY_POINTS):
        errors.append(f"expected entry points {ENTRY_POINTS}, found {sorted(entries)}")
        return report(errors)

    shape = []
    for p in sorted(entries):
        src = files[p]
        for inc in RE_INCLUDE.findall(src):
            if inc not in ENTRY_INCLUDES:
                errors.append(f"ENTRY POINT BUILDS: {p} includes {inc} — an entry point constructs "
                              f"Outshine and hands it an output medium, it does not assemble a world")
        for tok in ENTRY_FORBIDDEN:
            if tok in src:
                errors.append(f"ENTRY POINT BUILDS: {p} names `{tok}`")
        n = main_body_lines(src)
        if n is None:
            errors.append(f"ENTRY POINT: {p} defines no main()")
            continue
        if n > MAIN_LINES_MAX:
            errors.append(f"main() TOO LONG: {p} main() is {n} lines, ceiling {MAIN_LINES_MAX} "
                          f"(C++ Core Guidelines F.3)")
        shape.append((p, n))

    builders, benches, strays = 0, 0, []
    for p in sorted(files):
        hits = len(BUILD_CALLS.findall(files[p]))
        if not hits:
            continue
        if p in BUILDERS:
            builders += hits
        elif p in BENCH_BUILDERS:
            benches += hits
        else:
            strays.append((p, hits))
    for p, hits in strays:
        errors.append(f"SECOND SCENE BUILDER: {p} makes {hits} scene-building renderer call(s) — "
                      f"they belong in src/clients/Outshine.cpp, where every client gets them")

    if errors:
        return report(errors)
    shape_txt = ", ".join(f"{os.path.basename(p)} main()={n}" for p, n in shape)
    print(f"verify-clients: {len(entries)} entry point(s) ({shape_txt}), "
          f"{builders} scene-building call(s) in the one builder, "
          f"{benches} in {len(BENCH_BUILDERS)} declared subject-bench file(s), 0 elsewhere")
    return 0


def report(errors):
    for e in errors:
        print(f"verify-clients: {e}", file=sys.stderr)
    print(f"verify-clients: FAILED ({len(errors)} violation(s))", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

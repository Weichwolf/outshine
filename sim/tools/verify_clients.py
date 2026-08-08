#!/usr/bin/env python3
"""verify-clients: ONE PROGRAM, TWO TRANSLATIONS.

The defect this gate exists for, measured 2026-08-08 on 3827f7e: `AppWalk.cpp` named `Tree` 72 times
and `AppWasm.cpp` named it 0 times. The browser had never drawn a tree. `make wasm` was green the
whole time, because a green build proves that a client COMPILES, not that it SHOWS the same thing.

So the shape is asserted instead of hoped for. Three rules, each one a thing that went wrong:

  1. AN ENTRY POINT BUILDS NOTHING. `AppWalk.cpp` and `AppWasm.cpp` may not include a render/ or
     world/ header, and may not name `Renderer` or `World::World`. They construct the system and
     hand it the output medium.

  2. `main()` IS SHORT. C++ Core Guidelines F.3. The ceiling is a number here so that "it grew a
     little" is a failing build and not a matter of taste.

  3. THE SCENE IS BUILT IN EXACTLY ONE TRANSLATION UNIT. Every call that feeds the renderer a piece
     of the world -- the vegetation table, the sky and wind clocks, the tree ranks, the stands, the
     impostor, the moon, the stars, the camera basis, the scene state -- may appear only in
     `clients/Outshine.cpp` (which is that place) or in `world/` (which is what it draws from). A
     bench that reaches past the system to set up a SUBJECT is named, counted and printed, so a new
     one shows up in a diff.

Stdlib only, no build dependency. Exit 0 = clean, 1 = at least one violation.
"""
import os
import re
import sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")

ENTRY_POINTS = ("AppWalk.cpp", "AppWasm.cpp")
# An entry point's whole world. Everything else it might want is Outshine's or the bench's.
ENTRY_INCLUDES = {"Log.h", "LogSinks.h", "Outshine.h", "Walker.h", "Scene.h", "Snapshot.h",
                  "Mod.h", "SceneRunner.h", "Artifacts.h", "FileArtifacts.h", "Env.h",
                  "ServerLog.h", "ServerTelemetry.h", "ServerArtifacts.h",
                  # The run's name plate. Only an entry point knows which mod, which scene and which
                  # browser it is — none of that is a piece of the world.
                  "RunIdentity.h"}
# F.3. 40 native / 30 browser was the owner's order of magnitude; this is that with headroom for the
# emscripten glue, and it is deliberately not generous.
MAIN_LINES_MAX = 40
# Types an entry point may not name: naming one means it is building the thing instead of asking for
# it.
ENTRY_FORBIDDEN = ("Renderer", "World::World", "VegetationTemplates", "GroundMaterials",
                   "TreeGrower", "TreeField", "Forest", "fb_stream_", "TerrainLoader")

# THE SCENE-BUILDING SURFACE. Each token is a renderer call that puts a piece of the world into the
# picture; the round that added trees to the native client and not to the browser added seven of
# them to one main().
BUILD_CALLS = re.compile(
    r"\.(SetVegetationTable|SetSkyClock|SetWind|SetWindClock|SetExposure|SetFovDeg|SetOrthoM|"
    r"SetTreeBark|SetTreeCards|SetTreeStands|SetTreeStand|SetTreeCrown|SetTreeLook|"
    r"BakeTreeImpostor|SetMoonTexture|SetStars|SetCameraBasis|SetSceneState)\s*\(")
# The ONE place a scene is built, plus the layer it is built FROM.
BUILDERS = ("clients/Outshine.cpp",)
BUILDER_DIRS = ("world",)
# THE SUBJECT BENCH reaches past the system on purpose: it replaces the world and the light with
# declared ones and judges a single plant (doc/goal.md §3). Its length is printed, exactly like
# verify-layers prints its registry-reader count -- a second one moves the number.
BENCH_BUILDERS = ("clients/SceneRunner.cpp", "clients/SubjectBench.cpp")

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


def scan(root):
    files = {}
    for dp, dns, fns in os.walk(root):
        dns[:] = [d for d in dns if not d.startswith(".")]
        for fn in fns:
            if fn.endswith((".h", ".cpp")):
                p = os.path.relpath(os.path.join(dp, fn), root).replace(os.sep, "/")
                files[p] = read(os.path.join(dp, fn))
    return files


def main():
    root = os.path.normpath(SRC)
    files = scan(root)
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
        if p in BUILDERS or os.path.dirname(p).split("/")[0] in BUILDER_DIRS:
            builders += hits
        elif p in BENCH_BUILDERS:
            benches += hits
        else:
            strays.append((p, hits))
    for p, hits in strays:
        errors.append(f"SECOND SCENE BUILDER: {p} makes {hits} scene-building renderer call(s) — "
                      f"they belong in clients/Outshine.cpp, where BOTH translations get them")

    if errors:
        return report(errors)
    shape_txt = ", ".join(f"{os.path.basename(p)} main()={n}" for p, n in shape)
    print(f"verify-clients: {len(entries)} entry point(s) ({shape_txt}), "
          f"{builders} scene-building call(s) in the one builder + world/, "
          f"{benches} in {len(BENCH_BUILDERS)} declared subject-bench file(s), 0 elsewhere")
    return 0


def report(errors):
    for e in errors:
        print(f"verify-clients: {e}", file=sys.stderr)
    print(f"verify-clients: FAILED ({len(errors)} violation(s))", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

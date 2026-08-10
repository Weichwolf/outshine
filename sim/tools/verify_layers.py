#!/usr/bin/env python3
"""verify-layers: sim/src is a STACK, every #include points DOWN it, and every file declares the
NAMESPACE of the layer it lies in. The gate that keeps the directory tree honest — a layer that
quietly grows an upward dependency stops compiling only much later, if ever, so the structure is
asserted here instead of hoped for.

The layers, bottom to top (RANK below). The order is the measured include graph, not a wish:

  world/terrain  the lean terrain API (a LEAF: it includes nothing of ours).
  core           value types, the two channels (Log/Telemetry), the camera/matrix maths, the JSON
                 reader and the cluster DAG. Includes NOTHING above itself.
  render         the WebGPU renderer, driven by clients.
  world          World + tile streaming.
  clients        the entry points.

THE EDGE THE NEXT STEP HAS TO INVERT is counted rather than described: every call `world/` makes on a
Render::Renderer is printed with its name, and beside it the census of every OTHER way world/ reaches
into render/ — headers included and Render:: names used off a Renderer. Both numbers are read off the
tree instead of guessed (doc/todo.md 4), and this file dies with step 4.

The namespace half (LAYER_NS below) says the same thing to a reader that the include half says to the
build: core/ IS the root `namespace outshine` — its value types are everywhere, and nesting them would
be noise — while every layer ABOVE it nests one level, so a cross-layer name carries its layer at the
point of use (`Render::Renderer`, `World::TileLoader`).

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
    "render": 10,
    "render/stages": 10,
    "world": 10,
    "clients": 11,
}

# Files whose LAYER is not their directory's. Each is a real statement about the graph, not a waiver:
# the file genuinely sits at this rank and is held to it.
FILE_RANK = {
    # The I/O edge of core's two channels. Deliberately NOT in core (CLAUDE.md: core stays I/O-free),
    # but attached by every client, so they cannot BE the client layer. They include core and nothing
    # else, and this rank pins that.
    "clients/LogSinks.h": 1.5,
    "clients/LogSinks.cpp": 1.5,
}

# Named exceptions: (includer-pattern, included-file) pairs allowed against the rank order.
# EMPTY on purpose — every placement was resolved by moving the file, not by waiving the rule.
EXCEPTIONS = ()

# The namespace a directory's files declare. core/ IS the root: its value types (State, Log,
# Geodesy, Json, the cluster DAG) appear in every layer's signatures, and nesting them would add a
# qualifier to every line without adding information. Everything ABOVE core nests exactly one level,
# so a cross-layer name carries its layer where it is USED, not where it is declared.
LAYER_NS = {
    "world/terrain": None,   # C island, see C_ISLAND
    "core": "outshine",
    "render": "outshine::Render",
    "render/stages": "outshine::Render",
    "world": "outshine::World",
    "clients": "outshine::Clients",
}
KNOWN_NS = {v for v in LAYER_NS.values() if v}

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
    # A standalone static-file host: its own binary, no outshine type in it, C throughout.
    "clients/SimHost.cpp",
}

# A TU that defines the global `main` cannot be wrapped — C++ requires main at global scope. That is
# a law, not a waiver, so it is DETECTED rather than listed: such a TU reaches outshine through a
# file-scope `using namespace` and keeps its own helpers in an anonymous namespace.
RE_MAIN = re.compile(r"^\s*(?:int|auto)\s+main\s*\(", re.M)
# `namespace [X] {` opening a block at column 0. A one-liner that also CLOSES on its line is a forward
# declaration (`namespace outshine::Render { class Renderer; }`) and is judged separately. The tree's
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

# THE ONE EDGE THAT STILL POINTS FROM world/ INTO render/. A receiver is an identifier declared
# `Render::Renderer *x` or `&x` ANYWHERE in world/ (World's member is declared in the header and used
# in the .cpp), and a driving call is a call on such a receiver whose name Renderer.h actually
# declares — a name-only match would count world/'s own Ready() as an edge.
RE_RENDERER_API = re.compile(r"^\s{2}(?:[A-Za-z_][\w:<>,*& ]*?[\s*&])([A-Z][A-Za-z0-9_]*)\s*\(", re.M)
RE_RENDERER_RECV = re.compile(r"Render::Renderer\s*([*&])\s*([A-Za-z_]\w*)")
# EVERY OTHER WAY world/ REACHES render/. The call count above sees only members of a Renderer
# receiver; a value type, a free function or a constant borrowed out of render/ is the same edge and
# breaks the same link, so it is counted here rather than left invisible.
RE_RENDER_NAME = re.compile(r"\bRender::([A-Za-z_]\w*)")


def render_drivers(files, text):
    """(call sites, sorted distinct member names) for world/ -> Render::Renderer."""
    api = set()
    for p in files:
        if os.path.basename(p) == "Renderer.h":
            api = set(RE_RENDERER_API.findall(text[p]))
    world = [p for p in files if os.path.dirname(p) == "world"]
    receivers = set()
    for p in world:
        receivers |= set(RE_RENDERER_RECV.findall(text[p]))
    calls, names = 0, set()
    for p in world:
        for star, recv in receivers:
            op = r"->" if star == "*" else r"\."
            for m in re.finditer(r"\b" + re.escape(recv) + op + r"([A-Z]\w*)\s*\(", text[p]):
                if m.group(1) in api:
                    calls += 1
                    names.add(m.group(1))
    return calls, sorted(names)


def render_reach(files, text):
    """(render/ headers included by world/, Render:: names in world/ that are not Renderer, how
    often `Render::Renderer` itself is written). The last one is the surface step 4 has to invert:
    every occurrence is a receiver world/ declares, and the step is done when it reads 0."""
    render_files = {os.path.basename(p) for p in files
                    if os.path.dirname(p) in ("render", "render/stages")}
    world = [p for p in files if os.path.dirname(p) == "world"]
    headers, names, receivers = set(), set(), 0
    for p in world:
        headers |= {os.path.basename(i) for i in files[p]
                    if os.path.basename(i) in render_files}
        found = RE_RENDER_NAME.findall(text[p])
        names |= {n for n in found if n != "Renderer"}
        receivers += sum(1 for n in found if n == "Renderer")
    return sorted(headers), sorted(names), receivers


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

    n_drive, drivers = render_drivers(files, text)
    reach_headers, reach_names, n_recv = render_reach(files, text)
    ns_errors, n_ns = check_namespaces(files, text)
    errors += ns_errors

    if errors:
        return report(errors)
    print(f"verify-layers: {len(files)} files, {n_edges} internal include(s), "
          f"{len(set(RANK.values()))} layers — no upward include, "
          f"{n_drive} world/ -> Renderer call(s) over {len(drivers)} member(s), "
          f"{n_ns} file(s) in their layer's namespace ({len(C_ISLAND)} C-island file(s) exempt)")
    print(f"verify-layers: world/ drives render/ through: {', '.join(drivers)}")
    print(f"verify-layers: world/ also names {len(reach_headers)} render/ header(s) "
          f"[{', '.join(reach_headers)}] and {len(reach_names)} Render:: name(s) off a Renderer "
          f"[{', '.join(reach_names)}]")
    print(f"verify-layers: world/ writes `Render::Renderer` {n_recv} time(s)")
    return 0


def report(errors):
    for e in errors:
        print(f"verify-layers: {e}", file=sys.stderr)
    print(f"verify-layers: FAILED ({len(errors)} violation(s))", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

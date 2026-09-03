#!/usr/bin/env python3
"""Every symbol the archive defines that nothing in it calls, minus the door and the entry points.

clang-tidy cannot answer this and it is not a failing: it works one translation unit at a time, and
a public member in a header could be called from any OTHER unit, so no single-unit analysis may
declare it dead. The LINKER can, because it resolves the whole archive, and
`test/harness/shared/graph/callgraph.sh` already reads exactly what the linker resolves -- `nm -n`
over every object, relocations attributed to the enclosing symbol.

WHAT IS NOT DEAD, and both exclusions are load-bearing. A symbol the DOOR declares is called from
outside the archive by definition, so `include/` is read and anything named there is kept. And a
symbol nothing calls may still be REACHED -- a virtual through a vtable, a function pointer in a
table, a static initialiser -- so this counts a SUSPICION rather than a proof, which is why it
carries a baseline that may only fall rather than a verdict that goes red.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

TREE = pathlib.Path(__file__).resolve().parents[2]


def demangled(names):
    if not names:
        return {}
    said = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True)
    return dict(zip(names, said.stdout.splitlines()))


def main():
    archive = TREE / "build" / "liboutshine.a"
    if not archive.exists():
        print("UNPREPARED build/liboutshine.a: run make")
        return 2
    with tempfile.TemporaryDirectory() as scratch:
        edges = subprocess.run(
            ["sh", str(TREE / "test/harness/shared/graph/callgraph.sh"), str(archive), scratch],
            capture_output=True, text=True, cwd=TREE)
    defines, called = set(), set()
    for line in edges.stdout.splitlines():
        caller, _, callee = line.partition("\t")
        if caller:
            defines.add(caller)
        if callee:
            called.add(callee)
    door = " ".join(one.read_text() for one in (TREE / "include").rglob("*.h"))
    # A VIRTUAL IS REACHED THROUGH A VTABLE and leaves no call edge, so every name this tree
    # declares `virtual` or marks `override` is dispatched rather than dead. Read out of the source
    # rather than guessed from the mangling, which carries no such flag.
    dispatched = set()
    # WHAT THE LINKER CANNOT SEE. A call inside the translation unit that DEFINES its target is
    # bound directly or inlined away and leaves no relocation, so the graph reports the callee as
    # unreached. Measured: `Live::Reshape` is called five times in Live.cpp and stood in this list
    # beside `KeyLight` and `TowardTheKey`, both of which were extracted from a function that calls
    # them. Reading the SOURCE for the name is coarse -- an overload, or a same-named method of
    # another class, reads as a call -- but the error runs ONE WAY: it makes this walk report less,
    # and a missed candidate costs a look while a false one costs a deletion.
    calls = {}
    for one in list((TREE / "src").rglob("*.h")) + list((TREE / "src").rglob("*.cpp")):
        for name in re.findall(r"(\w+)\s*\(", one.read_text()):
            calls[name] = calls.get(name, 0) + 1
        for line in one.read_text().splitlines():
            if "virtual" in line or "override" in line:
                for name in re.findall(r"(\w+)\s*\(", line):
                    dispatched.add(name)
            # A MEMBER POINTER IN A TABLE is reached exactly as a virtual is, and leaves the same
            # absence: the relocation belongs to the DATA that holds the address, not to a caller.
            # `SceneRenderer::kExecutors` is twenty rows of them, so without this every render
            # stage's Configure and Encode counts as dead and ADDING a stage raises a ceiling that
            # may only fall.
            for name in re.findall(r"&\s*\w+::(\w+)\b", line):
                dispatched.add(name)
    suspect = []
    for name, plain in demangled(sorted(defines - called)).items():
        if name.startswith("___") or name.startswith("_GLOBAL__"):
            continue
        bare = re.sub(r"\(.*", "", plain).split("::")[-1].strip()
        if not bare or bare in door:
            continue
        # THE COMPILER CALLS SOME OF THESE WITHOUT AN EDGE. `operator new` and `operator delete` are
        # emitted at every allocation, a constructor and a destructor are called from wherever the
        # object stands, and a virtual is reached through a vtable rather than a call site. None of
        # the three leaves a relocation the graph can see, so all three are excluded rather than
        # counted as suspicion nobody can act on.
        owner = re.sub(r"\(.*", "", plain).split("::")
        if plain.startswith("operator ") or bare.startswith("~"):
            continue
        if len(owner) > 1 and bare == owner[-2].strip():
            continue
        if bare in dispatched:
            continue
        # A declaration and a definition are two mentions. A third is a CALL.
        if calls.get(bare, 0) > 2:
            continue
        suspect.append(plain)
    print(f"{len(defines)} symbol(s) defined, {len(called)} called, "
          f"{len(suspect)} that nothing in the archive calls and the door does not name")
    print("  NOT COVERED: a name mentioned three times in src/ is taken as called, so an overload "
          "of a dead function keeps it off this list. The walk reports less, never more.")
    for one in sorted(suspect)[:40]:
        print(f"  {one}")
    if len(suspect) > 40:
        print(f"  ... and {len(suspect) - 40} more")
    return 0


if __name__ == "__main__":
    sys.exit(main())

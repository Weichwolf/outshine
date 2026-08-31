#!/usr/bin/env python3
"""Every fragment entry the renderer NAMES must be one the shaders DEFINE, and the other way.

The subject's variant set is written twice: nineteen `fragment SFrag` entries across the .msl files,
and nineteen string literals in `SubjectDraw.cpp` that ask the device for them. Two lists in two
languages, agreeing by hand -- the same shape as the scenario grammar beside its reader, which drifted
eight times.

A name the renderer asks for and no shader defines is a pipeline that fails to compile at run time,
where the message is a driver's rather than a compiler's. A name a shader defines and nobody asks for
is a variant nothing can reach.
"""
import pathlib
import re
import sys

TREE = pathlib.Path(__file__).resolve().parents[2]


def main():
    defined = set()
    for one in (TREE / "src" / "render" / "shaders").glob("*.msl"):
        text = one.read_text()
        defined |= set(re.findall(r"fragment\s+\w+\s+(\w+)\s*\(", text))
        # The arms are macros, and a macro's first argument is the name it defines.
        defined |= set(re.findall(r"^SUBJECT_\w+_ARM\(\s*(\w+)", text, re.M))
        defined |= set(re.findall(r"vertex\s+\w+\s+(\w+)\s*\(", text))
    # A STAGE OTHER THAN THE SUBJECT'S MAY ASK TOO. The shadow pass asks for the depth-only arm by
    # name from its own file, and a check that reads one caller calls the other's entries dead.
    asked = set()
    for one in (TREE / "src" / "render").rglob("*.cpp"):
        asked |= set(re.findall(r'"(fs\w*|vs\w*)"', one.read_text()))
    missing = sorted(asked - defined)
    unreached = sorted(one for one in defined if one.startswith(("fs", "vs")) and one not in asked)
    print(f"{len(defined)} entry point(s) the shaders define, {len(asked)} the renderer names, "
          f"{len(missing)} named and undefined, {len(unreached)} defined and unnamed")
    # WHAT THIS DOES NOT COVER, and it went blind to it the day the vertex arms were generated
    # (board:2060). This reads TEXT: entry points spelled in an .msl and names spelled as string
    # literals beside the renderer. The sixteen vertex arms are now GENERATED from
    # VertexArms.h's table, so they appear in neither -- both of this check's inputs lost the
    # same fifteen names at once and the difference stayed zero. It stayed GREEN through the
    # change it exists to catch, from the other side.
    #
    # What holds them instead is stricter: two static_asserts in VertexArms.h prove every
    # VertexLayout has a row and every row sits at its own index, so a missing arm fails to
    # COMPILE rather than at pipeline creation. This check is now about the HAND-WRITTEN
    # fragment entries, and it says so rather than implying a coverage it lost.
    print("  NOT COVERED: the vertex arms, generated from VertexArms.h and held by its "
          "static_asserts instead. This counts the hand-written entries.")
    for one in missing:
        print(f"  NAMED AND UNDEFINED  {one}")
    for one in unreached:
        print(f"  DEFINED AND UNNAMED  {one}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())

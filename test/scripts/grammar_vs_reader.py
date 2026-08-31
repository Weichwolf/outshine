#!/usr/bin/env python3
"""Every child the scenario READER reads must be a child its GRAMMAR declares.

The two are kept by hand beside each other and they drift. Four drifts were found in one evening,
each of them a capability no declaration could reach: `<view>` required `follows` and `person` and
forbade children while the reader read `<at>` as a child; `<render>` read `<keep>` and the grammar
allowed only `output` and `stage`. Each cost an hour, and each was found by trying to WRITE a
scenario rather than by reading either file.

The whole answer is board:2052 -- derive the grammar from the declaration types, so the two cannot
be two -- and this is the guard that holds until it lands. It reads the reader's own calls:
`Child("x")`, `Children("x")` and `Declares(one, "x")` name a CHILD, and the grammar's second column
lists what a path may carry.
"""
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import scenario_grammar as grammar


def main():
    source = grammar.READER.read_text()
    allowed = grammar.declared()
    read = set()
    for name in re.findall(r'(?:\.Child|\.Children)\("(\w+)"\)', source):
        read.add(name)
    for name in re.findall(r'Declares\([^,]+,\s*"(\w+)"\)', source):
        read.add(name)
    every = grammar.children()
    astray = sorted(one for one in read if one not in every)
    print(f"{len(allowed)} path(s) in the grammar, {len(read)} child name(s) the reader reads, "
          f"{len(astray)} it reads and no path declares")
    for one in astray:
        print(f"  {one}")
    return 1 if astray else 0


if __name__ == "__main__":
    sys.exit(main())

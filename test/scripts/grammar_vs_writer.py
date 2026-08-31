#!/usr/bin/env python3
"""Every child the scenario's GRAMMAR declares must be a child its WRITER can write back.

`outshine-client roundtrip` reads each place, writes it, reads that back and writes it again, and
holds when the two texts agree. THAT CHECK CANNOT SEE A WRITER THAT DROPS A SECTION: the section is
missing from the first text, so the second read has nothing to read, so the second text matches.
Measured rather than reasoned -- with `<clock>` removed from the writer every place lost 59 bytes
and `roundtrip` still reported `0 place(s) apart`.

So the identity is not the guard; this is. A child the grammar declares and the writer never emits
is a capability a scenario can DECLARE and the engine can never hand back -- the drift board:2052
removes by deriving both from the declaration types. Until it lands the count carries a baseline
that may only fall.

WHAT THIS DOES NOT COVER: attributes. A child written with half its attributes passes here. It
counts CHILDREN, and it reads the writer's `"<name"` literals, so a child assembled from a computed
string would be missed.
"""
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import scenario_grammar as grammar

BASELINE = grammar.TREE / "test" / "writer-baseline"


def main():
    written = set(re.findall(r'"\s*<(\w+)', grammar.WRITER.read_text()))
    declared = grammar.children()
    missing = sorted(one for one in declared if one not in written)
    allowed = int(BASELINE.read_text().split()[0])
    print(f"{len(declared)} child(ren) the grammar declares, {len(declared) - len(missing)} the "
          f"writer writes back, {len(missing)} it cannot -- the baseline allows {allowed}")
    print("  NOT COVERED: attributes. A child written with half of its attributes passes here.")
    if len(missing) > allowed:
        for one in missing:
            print(f"  {one}")
        return 1
    if len(missing) < allowed:
        BASELINE.write_text(f"{len(missing)}\n")
        print(f"  the writer baseline SHRANK to {len(missing)} -- recorded.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

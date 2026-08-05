#!/usr/bin/env python3
"""verify-trees: doc/, src/ and test/ carry THE SAME DIRECTORY TREE.

  doc/<path>   what we want
  src/<path>   what we can do
  test/<path>  what we prove

One place per statement, and the same place in all three trees: given any one of them, the other two
are known BY PATH instead of by search. A directory that appears in two of the three is a named hole —
a want with no implementation, an implementation with no proof, or a proof nobody ordered.

The rule is a DIRECTORY rule, because that is what the instruction says and because it is the only half
a machine can check without knowing what a file means. Two shapes are reported apart, since the work
they imply is different:

  LEAF     doc/<path>.md exists where src/<path>/ is a directory — one document that has not been
           split into the directory its subject already is. A rename plus a split, not a new text.
  MISSING  nothing at that path at all.
  EXTRA    a directory in doc/ or test/ with no counterpart in src/ — either src/ owes a directory
           or the tree owes a home.

Stdlib only, no build dependency. Exit 0 = the three trees are congruent, 1 = at least one orphan.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SRC = os.path.join(HERE, "..", "src")
DEFAULT_TEST = os.path.join(HERE, "..", "test")
DEFAULT_DOC = os.path.join(HERE, "..", "..", "doc")

TREES = ("doc", "src", "test")


def dirs_of(root):
    """Relative directory paths under root, '' for the root itself. Hidden directories are not part
    of the tree; an unreadable root is an empty tree, which is a legible verdict rather than a stack
    trace (test/ genuinely does not exist until it is created)."""
    out = set()
    if not os.path.isdir(root):
        return out
    for dp, dns, _ in os.walk(root):
        dns[:] = sorted(d for d in dns if not d.startswith("."))
        out.add(os.path.relpath(dp, root).replace(os.sep, "/").lstrip("."))
    return out


def leaf_docs(root):
    """<path> for every <path>.md — the single-document form of a directory that has not been split."""
    out = set()
    if not os.path.isdir(root):
        return out
    for dp, dns, fns in os.walk(root):
        dns[:] = [d for d in dns if not d.startswith(".")]
        for fn in fns:
            if fn.endswith(".md"):
                p = os.path.relpath(os.path.join(dp, fn[:-3]), root).replace(os.sep, "/")
                out.add(p)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--doc", default=DEFAULT_DOC)
    ap.add_argument("--src", default=DEFAULT_SRC)
    ap.add_argument("--test", default=DEFAULT_TEST)
    a = ap.parse_args()
    roots = {"doc": os.path.normpath(a.doc), "src": os.path.normpath(a.src),
             "test": os.path.normpath(a.test)}

    have = {t: dirs_of(roots[t]) for t in TREES}
    leaves = {t: leaf_docs(roots[t]) for t in TREES}
    union = sorted(set().union(*have.values()))

    orphans, kinds = [], {"LEAF": 0, "MISSING": 0, "EXTRA": 0}
    for p in union:
        if p == "":
            continue
        state = {}
        for t in TREES:
            if p in have[t]:
                state[t] = "dir"
            elif p in leaves[t]:
                state[t] = "leaf"
            else:
                state[t] = "-"
        if all(v == "dir" for v in state.values()):
            continue
        if "src" not in [t for t in TREES if state[t] == "dir"]:
            kind = "EXTRA"
        elif any(state[t] == "leaf" for t in TREES):
            kind = "LEAF"
        else:
            kind = "MISSING"
        kinds[kind] += 1
        orphans.append((kind, p, state))

    for t in TREES:
        print(f"verify-trees: {t:<4} {len(have[t]):3d} director{'y' if len(have[t]) == 1 else 'ies'} "
              f"({roots[t]})")
    print(f"verify-trees: {len(union) - 1} distinct path(s) across the three trees, "
          f"{len(orphans)} orphan(s) "
          f"[{kinds['MISSING']} MISSING, {kinds['LEAF']} LEAF, {kinds['EXTRA']} EXTRA]")
    if not orphans:
        return 0
    sys.stdout.flush()   # the orphan list goes to stderr; unflushed it would print after it
    w = max(len(p) for _, p, _ in orphans)
    for kind, p, state in orphans:
        cols = "  ".join(f"{t}={state[t]}" for t in TREES)
        print(f"verify-trees:   {kind:<7} {p:<{w}}  {cols}", file=sys.stderr)
    print(f"verify-trees: FAILED ({len(orphans)} orphan(s))", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

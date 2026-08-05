#!/usr/bin/env python3
"""verify-trees: every subject carries its intent and its proof, at a known path.

THREE SCOPES, THREE RULES, because the subjects differ (doc/mods.md §3, doc/module-contract.md §2).

  engine   doc/<path>   what we want
           src/<path>   what we can do
           test/<path>  what we prove
           One place per statement, and the same place in all three trees: given any one of them, the
           other two are known BY PATH instead of by search. A directory that appears in two of the
           three is a named hole — a want with no implementation, an implementation with no proof, or
           a proof nobody ordered. The rule is a DIRECTORY rule, because that is what the instruction
           says and because it is the only half a machine can check without knowing what a file means.

  mod      mods/<id>/doc/   what we want
           mods/<id>/src/   what we can do, AND the proof — a mission is a declaration, so fb-gym
                            running it IS the assertion. A mod therefore has NO test/, and a test/
                            beside it would be the same claim written twice.
           Not a directory rule: a mod's doc/ is prose per subject (campaign, terrain, hud, sources)
           against a src/ of declarations, so path congruence would be a category error here. What is
           checkable without knowing what a mod is, is §3's own formulation — DOC PLUS PROOF.

  module   src/modules/<id>/  the FIXED PARTS a module directory carries, named one by one, so that
           integrating a module is unambiguous instead of learned by reading a neighbour:

             FB*Module.h                 the FBModule subclass
             FB*ModuleRegistration.cpp   exactly one — the single place the registry key is bound
             doc/modules/<id>/module.md  the contract statement, and in it:
             **Contributes:** …          the module's declaration, in words of the CLOSED VOCABULARY
                                         of src/modules/FBContribution.h — at least one State word
                                         and at least one Geometry word, because a module owns a piece
                                         of world state and derives geometry from it

           Only the engine tree has module directories: a mod ships no .cpp by rule (doc/mods.md §2.1),
           so a mod cannot own one.

Three shapes, reported apart since the work they imply differs:

  LEAF     doc/<path>.md exists where src/<path>/ is a directory — one document that has not been
           split into the directory its subject already is. A rename plus a split, not a new text.
  MISSING  nothing at that path at all.
  EXTRA    a directory in doc/ or test/ with no counterpart in src/ — either src/ owes a directory
           or the tree owes a home. For a mod: a test/ the rule forbids. For a module: a word that is
           not in the vocabulary, or a second registration.

Every orphan carries a REMEDY, because the reader is a machine as often as a person and "a developer
will see what was meant" is not a property.

Stdlib only, no build dependency. Exit 0 = congruent, 1 = at least one orphan in any scope.
"""
import argparse
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SRC = os.path.join(HERE, "..", "src")
DEFAULT_TEST = os.path.join(HERE, "..", "test")
DEFAULT_DOC = os.path.join(HERE, "..", "..", "doc")
DEFAULT_MODS = os.path.join(HERE, "..", "..", "mods")

TREES = ("doc", "src", "test")
MOD_TREES = ("doc", "src")
KINDS = ("MISSING", "LEAF", "EXTRA")

CONTRIB_HEADER = os.path.join("modules", "FBContribution.h")
CONTRIB_ROW = re.compile(r'^\s*X\(\s*(\w+)\s*,\s*(\w+)\s*,\s*"([^"]+)"')
CONTRIB_LINE = re.compile(r"^\s*\*\*Contributes:\*\*(.*)$")
MODULE_CLASS = "FB*Module.h"
MODULE_REG = "FB*ModuleRegistration.cpp"


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


def has_mission(root):
    """A runnable .fbm anywhere under root — a mod's proof (doc/mods.md §3)."""
    for dp, dns, fns in os.walk(root):
        dns[:] = [d for d in dns if not d.startswith(".")]
        if any(fn.endswith(".fbm") for fn in fns):
            return True
    return False


def engine_orphans(roots):
    have = {t: dirs_of(roots[t]) for t in TREES}
    leaves = {t: leaf_docs(roots[t]) for t in TREES}
    union = sorted(set().union(*have.values()))

    orphans = []
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
            fix = f"add src/{p}/, or remove " + ", ".join(
                f"{t}/{p}/" for t in TREES if state[t] == "dir")
        elif any(state[t] == "leaf" for t in TREES):
            kind = "LEAF"
            fix = "split " + ", ".join(f"{t}/{p}.md into {t}/{p}/" for t in TREES
                                       if state[t] == "leaf")
        else:
            kind = "MISSING"
            fix = "add " + ", ".join(f"{t}/{p}/" for t in TREES if state[t] == "-")
        orphans.append(("engine", kind, p, cols(TREES, state), fix))

    for t in TREES:
        print(f"verify-trees: {t:<4} {len(have[t]):3d} director{'y' if len(have[t]) == 1 else 'ies'} "
              f"({roots[t]})")
    print(f"verify-trees: engine  {len(union) - 1} distinct path(s) across three trees, "
          f"{fmt(orphans)}")
    return orphans


def mod_orphans(root):
    """DOC PLUS PROOF per mod, not path congruence — see the module docstring."""
    ids = sorted(d for d in os.listdir(root)
                 if not d.startswith(".") and os.path.isdir(os.path.join(root, d))) \
        if os.path.isdir(root) else []

    orphans = []
    for mod in ids:
        base = os.path.join(root, mod)
        state = {t: ("dir" if os.path.isdir(os.path.join(base, t)) else "-") for t in MOD_TREES}
        rel = os.path.relpath(base, os.path.dirname(root)).replace(os.sep, "/")
        c = cols(MOD_TREES, state)
        for t in MOD_TREES:
            if state[t] == "-":
                orphans.append((f"mod:{mod}", "MISSING", f"{rel}/{t}", c, f"add {rel}/{t}/"))
        if state["src"] == "dir" and not has_mission(os.path.join(base, "src")):
            orphans.append((f"mod:{mod}", "MISSING", f"{rel}/src/**.fbm", c,
                            "a mod's proof is a runnable mission — add one under src/"))
        if os.path.isdir(os.path.join(base, "test")):
            orphans.append((f"mod:{mod}", "EXTRA", f"{rel}/test", c,
                            f"a mod has no test/ — fold {rel}/test into a mission"))

    print(f"verify-trees: mods    {len(ids)} mod(s) under {root}, two trees each, {fmt(orphans)}")
    return orphans


def vocabulary(src_root):
    """The closed contribution vocabulary, read from the ONE table that defines it. The gate does not
    keep a second copy: a word exists because src/modules/FBContribution.h has a row for it."""
    path = os.path.join(src_root, CONTRIB_HEADER)
    words = {}
    try:
        with open(path, encoding="utf-8") as fh:
            for line in fh:
                m = CONTRIB_ROW.match(line)
                if m:
                    words[m.group(3)] = m.group(2)
    except OSError:
        pass
    return path, words


def module_orphans(roots):
    """The fixed parts of a module directory. Engine tree only — a mod ships no .cpp (doc/mods.md
    §2.1), so it cannot own a module directory."""
    src_modules = os.path.join(roots["src"], "modules")
    ids = sorted(d for d in os.listdir(src_modules)
                 if not d.startswith(".") and os.path.isdir(os.path.join(src_modules, d))) \
        if os.path.isdir(src_modules) else []

    vpath, vocab = vocabulary(roots["src"])
    sides = sorted(set(vocab.values()))
    legal = {s: sorted(w for w, sd in vocab.items() if sd == s) for s in sides}
    orphans = []

    if not vocab:
        orphans.append(("module", "MISSING", os.path.relpath(vpath, os.path.dirname(roots["src"])),
                        "vocab=-", "the vocabulary is this gate's input — restore the "
                                   "FB_MODULE_CONTRIBUTIONS table"))

    for mid in ids:
        base = os.path.join(src_modules, mid)
        doc = os.path.join(roots["doc"], "modules", mid, "module.md")
        declared, unknown, has_decl = set(), [], False
        if os.path.isfile(doc):
            with open(doc, encoding="utf-8") as fh:
                for line in fh:
                    m = CONTRIB_LINE.match(line)
                    if not m:
                        continue
                    has_decl = True
                    for w in re.split(r"[\s,·`]+", m.group(1).strip()):
                        if not w:
                            continue
                        if w in vocab:
                            declared.add(w)
                        else:
                            unknown.append(w)
                    break
        c = f"declares={'+'.join(sorted(declared)) if declared else '-'}"

        def orphan(kind, part, fix):
            orphans.append((f"module:{mid}", kind, part, c, fix))

        if not glob.glob(os.path.join(base, MODULE_CLASS)):
            orphan("MISSING", f"src/modules/{mid}/{MODULE_CLASS}",
                   f"add the FBModule subclass src/modules/{mid}/FB<Name>Module.h")
        regs = glob.glob(os.path.join(base, MODULE_REG))
        if not regs:
            orphan("MISSING", f"src/modules/{mid}/{MODULE_REG}",
                   f"add src/modules/{mid}/FB<Name>ModuleRegistration.cpp — the one place the "
                   f"registry key is bound")
        elif len(regs) > 1:
            orphan("EXTRA", f"src/modules/{mid}/{MODULE_REG}",
                   "a module has exactly one registration: " +
                   ", ".join(sorted(os.path.basename(r) for r in regs)))
        if not os.path.isfile(doc):
            orphan("MISSING", f"doc/modules/{mid}/module.md",
                   f"write doc/modules/{mid}/module.md and give it a '**Contributes:** …' line")
        elif not has_decl:
            orphan("MISSING", f"doc/modules/{mid}/module.md '**Contributes:**'",
                   "add a line '**Contributes:** <" + "> <".join(s.lower() for s in sides) +
                   ">' — " + " · ".join(f"{s.lower()}: {' '.join(legal[s])}" for s in sides))
        else:
            for w in unknown:
                orphan("EXTRA", f"doc/modules/{mid}/module.md '{w}'",
                       f"'{w}' is not in the vocabulary ({os.path.basename(vpath)}); legal words: " +
                       " ".join(sorted(vocab)))
            for s in sides:
                if not any(vocab[w] == s for w in declared):
                    orphan("MISSING", f"doc/modules/{mid}/module.md {s.lower()}",
                           f"the contract needs one {s.lower()} word: " + " ".join(legal[s]))

    print(f"verify-trees: modules {len(ids)} module(s) under {src_modules}, "
          f"{len(vocab)}-word vocabulary, {fmt(orphans)}")
    return orphans


def cols(trees, state):
    return "  ".join(f"{t}={state[t]}" for t in trees)


def fmt(orphans):
    n = {k: sum(1 for o in orphans if o[1] == k) for k in KINDS}
    return f"{len(orphans)} orphan(s) [" + ", ".join(f"{n[k]} {k}" for k in KINDS) + "]"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--doc", default=DEFAULT_DOC)
    ap.add_argument("--src", default=DEFAULT_SRC)
    ap.add_argument("--test", default=DEFAULT_TEST)
    ap.add_argument("--mods", default=DEFAULT_MODS)
    a = ap.parse_args()
    roots = {"doc": os.path.normpath(a.doc), "src": os.path.normpath(a.src),
             "test": os.path.normpath(a.test)}

    orphans = (engine_orphans(roots) + mod_orphans(os.path.normpath(a.mods))
               + module_orphans(roots))
    if not orphans:
        return 0
    print(f"verify-trees: {fmt(orphans)} in total")
    sys.stdout.flush()   # the orphan list goes to stderr; unflushed it would print after it
    ws, wp, wc = (max(len(o[i]) for o in orphans) for i in (0, 2, 3))
    for scope, kind, p, c, fix in orphans:
        print(f"verify-trees:   {scope:<{ws}}  {kind:<7} {p:<{wp}}  {c:<{wc}}  -> {fix}",
              file=sys.stderr)
    print(f"verify-trees: FAILED ({fmt(orphans)})", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())

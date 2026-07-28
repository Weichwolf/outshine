#!/usr/bin/env python3
"""verify-models: every FlightBox model copy differs from its pinned upstream by EXACTLY the deltas
that sim/assets/MODEL-DELTAS.md names — no unexplained byte, and no declared change that is not there.

The gate behind CLAUDE.md Prinzip 1: the submodule is the BASIS, sim/assets/aircraft
is what actually flies, and the delta list is what makes a deviation legitimate instead of drift. It
also makes Prinzip 5 hold — "the model is the reference" only means anything while every difference
from the pinned model is named and evidenced.

The comparison is a TEXT COMPARISON of canonical unified diffs (difflib, 3 lines of context), not an
attempted patch application: a fuzzy apply can swallow a real difference, an exact string compare
cannot, and on mismatch this can print the block the list would have to carry.

Stdlib only, no build dependency. Exit 0 = clean, 1 = a difference is unaccounted for.
"""
import argparse
import difflib
import os
import re
import sys

FIELDS = ("Datei", "Änderung", "Grund", "Beleg")


def fail(msgs):
    for m in msgs:
        print(f"verify-models: {m}", file=sys.stderr)
    return 1


def read_lines(path):
    with open(path, "rb") as f:
        data = f.read()
    try:
        return data.decode("utf-8").splitlines(keepends=True), None
    except UnicodeDecodeError:
        return None, data


def canonical_diff(rel, up_path, cp_path):
    """The one diff spelling this tool and the delta list both use. '' = identical."""
    up, up_bin = read_lines(up_path) if up_path and os.path.exists(up_path) else ([], None)
    cp, cp_bin = read_lines(cp_path) if cp_path and os.path.exists(cp_path) else ([], None)
    if up is None or cp is None:                       # binary: no text delta can describe it
        return None if up_bin != cp_bin else ""
    out = difflib.unified_diff(up, cp, fromfile=f"upstream/{rel}", tofile=f"flightbox/{rel}", n=3)
    return "".join(out)


def strip_comments(text):
    """Drop HTML comments — the file documents its own entry format inside one — but never touch the
    inside of a ```diff fence: an aircraft XML is comment-heavy, so a legitimate delta may well move
    `<!-- -->` lines, and eating those would make exactly that delta undeclarable. Whichever of the two
    opens first wins, so the format template (a fence INSIDE a comment) still disappears whole."""
    out, state = [], "text"
    for line in text.splitlines(keepends=True):
        if state == "comment":
            if "-->" in line:
                state = "text"
                line = line.split("-->", 1)[1]
            else:
                continue
        if state == "fence":
            out.append(line)
            if line.rstrip("\n") == "```":
                state = "text"
            continue
        line = re.sub(r"<!--.*?-->", "", line)
        if "<!--" in line:
            state = "comment"
            line = line.split("<!--", 1)[0]
        elif line.startswith("```diff"):
            state = "fence"
        out.append(line)
    return "".join(out)


def parse_delta_doc(path):
    """-> (origins [(copy_rel, upstream_rel|None)], deltas {copy_rel: diff_text}, errors)."""
    errors = []
    with open(path, encoding="utf-8") as f:
        text = f.read()
    text = strip_comments(text)

    origins = []
    m = re.search(r"^## Herkunft\s*$(.*?)^## ", text, flags=re.M | re.S)
    if not m:
        errors.append(f"{path}: no '## Herkunft' section")
    else:
        for row in re.finditer(r"^\|\s*`([^`]+)`\s*\|\s*(?:`([^`]+)`|—)\s*\|\s*$", m.group(1), flags=re.M):
            origins.append((row.group(1), row.group(2)))
        if not origins:
            errors.append(f"{path}: '## Herkunft' declares no model")

    deltas = {}
    body = text.split("## Deltas", 1)[1] if "## Deltas" in text else ""
    for entry in re.split(r"^### ", body, flags=re.M)[1:]:
        title = entry.splitlines()[0].strip()
        vals = {}
        for k in FIELDS:
            fm = re.search(r"^-\s+\*\*%s:\*\*\s*(.+?)(?=^-\s+\*\*|\n```|\Z)" % re.escape(k),
                           entry, flags=re.M | re.S)
            vals[k] = fm.group(1).strip() if fm else ""
            if not vals[k]:
                errors.append(f"delta '{title}': field '{k}' missing or empty")
        blocks = re.findall(r"^```diff\n(.*?)^```\s*$", entry, flags=re.M | re.S)
        if len(blocks) != 1:
            errors.append(f"delta '{title}': expected exactly one ```diff block, found {len(blocks)}")
            continue
        rel = vals["Datei"].strip().strip("`")
        want_head = f"--- upstream/{rel}\n+++ flightbox/{rel}\n"
        if not blocks[0].startswith(want_head):
            errors.append(f"delta '{title}': diff header must be exactly\n    {want_head.rstrip()}")
        deltas.setdefault(rel, "")
        deltas[rel] += blocks[0]
    return origins, deltas, errors


def walk(root, rel=""):
    p = os.path.join(root, rel) if rel else root
    if os.path.isfile(p):
        return {rel}
    out = set()
    for dirpath, _dirs, files in os.walk(p):
        for f in files:
            full = os.path.join(dirpath, f)
            out.add(os.path.relpath(full, root))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--copies", default="assets/aircraft")
    ap.add_argument("--upstream", default="vendor/jsbsim")
    ap.add_argument("--deltas", default="assets/MODEL-DELTAS.md")
    ap.add_argument("--emit", action="store_true",
                    help="print a paste-ready entry skeleton for every undeclared difference and exit 0. "
                         "Whitespace in a unified diff is significant (context lines keep their leading "
                         "space), so a delta block is generated, never retyped.")
    a = ap.parse_args()

    origins, deltas, errors = parse_delta_doc(a.deltas)
    if errors:
        return fail(errors)

    # Every top-level entry of the copy root must be declared. An undeclared model is an unverified one.
    declared_top = {o[0].split("/")[0] for o in origins}
    for name in sorted(os.listdir(a.copies)):
        if name.startswith("."):
            continue
        if name not in declared_top:
            errors.append(f"{a.copies}/{name} is not declared in {a.deltas} ('## Herkunft')")

    # Which copy files a directory-level origin covers: everything under it MINUS what a more specific
    # origin entry claims (f16/engine/*.xml come from the shared engine root, not from aircraft/f16).
    specific = {o[0] for o in origins if os.path.isfile(os.path.join(a.copies, o[0]))}
    actual = {}
    for copy_rel, up_rel in origins:
        cpath = os.path.join(a.copies, copy_rel)
        if not os.path.exists(cpath):
            errors.append(f"declared copy '{copy_rel}' does not exist under {a.copies}")
            continue
        if up_rel is None:
            continue                                   # FlightBox-own model: no upstream to compare
        upath = os.path.join(a.upstream, up_rel)
        if not os.path.exists(upath):
            errors.append(f"upstream '{up_rel}' does not exist under {a.upstream}")
            continue
        files = walk(a.copies, copy_rel) | {os.path.join(copy_rel, r) if r else copy_rel
                                            for r in walk(a.upstream, up_rel)}
        if os.path.isfile(cpath):
            files = {copy_rel}
        for rel in sorted(files):
            if rel != copy_rel and rel in specific:
                continue
            sub = os.path.relpath(rel, copy_rel) if rel != copy_rel else ""
            d = canonical_diff(rel, os.path.join(upath, sub) if sub else upath,
                               os.path.join(cpath, sub) if sub else cpath)
            if d is None:
                errors.append(f"{rel}: binary file differs from upstream — no text delta can describe it")
            elif d:
                actual[rel] = d

    if a.emit:
        for rel in sorted(actual):
            if actual[rel] == deltas.get(rel, ""):
                continue
            print(f"### D? — Kurztitel\n\n- **Datei:** `{rel}`\n- **Änderung:** \n- **Grund:** \n"
                  f"- **Beleg:** \n\n```diff\n{actual[rel]}```\n")
        return 0

    for rel in sorted(set(actual) | set(deltas)):
        got, want = actual.get(rel, ""), deltas.get(rel, "")
        if got == want:
            continue
        if not want:
            errors.append(f"{rel}: UNEXPLAINED difference from upstream. Either revert it, or add an "
                          f"entry to {a.deltas} carrying exactly this diff — generate it with "
                          f"`python3 tools/verify_models.py --emit`, do not retype it:\n"
                          + "".join("    " + l for l in got.splitlines(keepends=True)))
        elif not got:
            errors.append(f"{rel}: {a.deltas} declares a delta that is NOT present in the copy")
        else:
            errors.append(f"{rel}: the declared delta does not match the actual difference. Actual:\n"
                          + "".join("    " + l for l in got.splitlines(keepends=True)))

    if errors:
        return fail(errors)
    n_cmp = sum(1 for c, u in origins if u is not None)
    print(f"verify-models: {n_cmp} upstream-backed model path(s) match {a.deltas} "
          f"({len(deltas)} declared delta(s), {len(origins) - n_cmp} FlightBox-own)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Delete every comment in src/ and include/ that is not a Doxygen block.

THE RULE IS THE TREE'S OWN AND IT IS OLDER THAN THIS FILE: `src/` and `include/` carry no
commentary. Names and structure carry the meaning; a number's origin lives in its item and its
commit. Enforced by a claim it stayed broken for a year at 1606 lines, because a rule that only
NAGS is a rule somebody is always about to get to.

DELETING DOES NOT DESTROY, IT RELOCATES. Every line removed here is in the commit that added it, and
`git log -p` finds any of them -- which is precisely where the rule says a reason belongs.

A SCANNER AND NOT A REGEX. This tree assembles MSL shaders as C++ string literals, so a `//` inside
a string is SHADER SOURCE and a regex eats it. Everything below is a small state machine over the
five things a C++ file can be in the middle of: code, a string, a character, a raw string, or a
comment. Raw strings matter: `R"msl( ... )msl"` may hold anything at all.

WHAT SURVIVES, AND ONLY THERE: `///`, `//!`, `/** */` and `/*! */` in `include/` and
`src/client/`. Those two are DOORS -- the engine's public interface and its official command line --
and a door is documented where a generator can render it. Everywhere else in `src/`
nothing survives at all, which is the point: seeing the deletion on every build is what forces code
that speaks for itself.
"""
import re
import sys
import os

KEEP_LINE = ("///", "//!")
KEEP_BLOCK = ("/**", "/*!")

DOORS = ("include/", "src/client/")

def keeps_doxygen(path):
    return any(path.startswith(door) or ("/" + door) in path for door in DOORS)

def strip(text, doxygen=True):
    out = []
    at = 0
    end = len(text)
    while at < end:
        ch = text[at]
        two = text[at:at + 2]
        if ch == '"' or ch == "'":
            quote = ch
            out.append(ch)
            at += 1
            while at < end:
                if text[at] == "\\" and at + 1 < end:
                    out.append(text[at:at + 2])
                    at += 2
                    continue
                out.append(text[at])
                if text[at] == quote:
                    at += 1
                    break
                at += 1
            continue
        if ch == "R" and text[at:at + 2] == 'R"':
            shut = text.find("(", at + 2)
            if shut > 0:
                tag = text[at + 2:shut]
                close = ')' + tag + '"'
                stop = text.find(close, shut)
                stop = end if stop < 0 else stop + len(close)
                out.append(text[at:stop])
                at = stop
                continue
        if two == "//":
            if doxygen and text[at:at + 3] in KEEP_LINE:
                stop = text.find("\n", at)
                stop = end if stop < 0 else stop
                out.append(text[at:stop])
                at = stop
                continue
            stop = text.find("\n", at)
            stop = end if stop < 0 else stop
            # A comment that is the whole line takes the line with it; one that trails code leaves
            # the code and the newline behind.
            head = "".join(out)
            line_start = head.rfind("\n") + 1
            if head[line_start:].strip() == "":
                del out[len(head) - (len(head) - line_start):]
                out = [head[:line_start]]
            at = stop + 1 if stop < end and head[line_start:].strip() == "" else stop
            continue
        if two == "/*":
            if doxygen and text[at:at + 3] in KEEP_BLOCK:
                stop = text.find("*/", at + 2)
                stop = end if stop < 0 else stop + 2
                out.append(text[at:stop])
                at = stop
                continue
            stop = text.find("*/", at + 2)
            stop = end if stop < 0 else stop + 2
            head = "".join(out)
            line_start = head.rfind("\n") + 1
            whole = head[line_start:].strip() == "" and (stop >= end or text[stop:stop + 1] == "\n")
            if whole:
                out = [head[:line_start]]
                at = stop + 1
            else:
                at = stop
            continue
        out.append(ch)
        at += 1
    body = "".join(out)
    # AN ANONYMOUS NAMESPACE THAT HELD ONLY A COMMENT LEAVES ITS SHELL BEHIND. Fifteen of them
    # stood in eleven files -- `Laying.cpp` had two in a row -- because the strip removes what is
    # inside and keeps the braces. They compile and mean nothing, which is the definition of noise
    # in a tree whose whole argument for stripping is that the code should speak for itself.
    body = re.sub(r"\nnamespace \{\s*\}\n", "\n", body)
    kept = []
    blank = 0
    for line in body.split("\n"):
        trimmed = line.rstrip()
        blank = blank + 1 if trimmed == "" else 0
        if blank > 1:
            continue
        kept.append(trimmed)
    while kept and kept[-1] == "":
        kept.pop()
    return "\n".join(kept) + "\n"

def main(roots):
    touched = 0
    lines = 0
    for root in roots:
        for here, _, names in os.walk(root):
            if "/shaders" in here:
                continue
            for name in sorted(names):
                if not name.endswith((".h", ".cpp", ".hpp")):
                    continue
                path = os.path.join(here, name)
                with open(path, "r", encoding="utf-8") as reading:
                    was = reading.read()
                now = strip(was, keeps_doxygen(path))
                if now == was:
                    continue
                with open(path, "w", encoding="utf-8") as writing:
                    writing.write(now)
                touched += 1
                lines += was.count("\n") - now.count("\n")
    print("strip: %d file(s), %d line(s) gone -- they are in the commits that added them.\n"
          "       include/ and src/client/ keep their Doxygen; the rest of src/ keeps "
          "nothing." % (touched, lines))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:] or ["src", "include"]))

#!/usr/bin/env python3
"""Enforce source comment policy without changing literals or token boundaries."""
import os
from pathlib import Path
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parent.parent
KEEP_LINE = ("///", "//!")
KEEP_BLOCK = ("/**", "/*!")
RAW = re.compile(r'(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(')
IDENTIFIER = re.compile(r'[A-Za-z_][A-Za-z_0-9]*')
NUMBER = re.compile(r'(?:[0-9]|\.[0-9])(?:[A-Za-z_0-9.\']|[eEpP][+-])*')
SPLICE = re.compile(r'\\\r?\n')


def policy(path):
    try:
        parts = Path(path).resolve().relative_to(ROOT).parts
    except ValueError:
        return None
    if parts and parts[0] in ("src", "include"):
        return parts[0] == "include"
    return None


def keeps_doxygen(path):
    return policy(path) is True


def strip(text, doxygen=False):
    out = []
    at = 0
    end = len(text)

    def next_logical(position):
        while match := SPLICE.match(text, position):
            position = match.end()
        return position

    while at < end:
        raw = RAW.match(text, at)
        if raw:
            close = ')' + raw[1] + '"'
            stop = text.find(close, raw.end())
            if stop < 0:
                raise ValueError("unterminated raw string")
            stop += len(close)
            out.append(text[at:stop])
            at = stop
            continue
        if text[at] in ('"', "'"):
            quote = text[at]
            stop = at + 1
            while stop < end:
                if text[stop] == "\\":
                    stop += 2
                elif text[stop] == quote:
                    stop += 1
                    break
                else:
                    stop += 1
            else:
                raise ValueError("unterminated quoted literal")
            out.append(text[at:stop])
            at = stop
            continue
        second = next_logical(at + 1)
        if text[at] == '/' and second < end and text[second] in ('/', '*'):
            line = text[second] == '/'
            third = next_logical(second + 1)
            keep = doxygen and third < end and (
                text[third] in ('/', '!') if line else text[third] in ('*', '!'))
            stop = second + 1
            if line:
                while stop < end:
                    logical = next_logical(stop)
                    if logical != stop:
                        stop = logical
                    elif text[stop] == '\n':
                        break
                    else:
                        stop += 1
            else:
                while stop < end:
                    after = next_logical(stop + 1)
                    if text[stop] == '*' and after < end and text[after] == '/':
                        stop = after + 1
                        break
                    stop += 1
                else:
                    raise ValueError("unterminated block comment")
            comment = text[at:stop]
            if keep:
                out.append(comment)
            else:
                # Translation-phase splices are not logical line breaks.
                logical = SPLICE.sub('', comment)
                out.append(' ' + '\n' * logical.count('\n'))
            at = stop
            continue
        token = NUMBER.match(text, at) or IDENTIFIER.match(text, at)
        if token:
            out.append(token[0])
            at = token.end()
        else:
            out.append(text[at])
            at += 1
    return ''.join(out)


FORMATTER = os.environ.get("CLANG_FORMAT", "clang-format")


def reflowed(path, text):
    done = subprocess.run([FORMATTER, "--assume-filename=" + str(path)],
                          input=text, capture_output=True, text=True, check=True)
    return done.stdout


def settle(path):
    keep = policy(path)
    if keep is None:
        return 0, 0
    with open(path, encoding="utf-8", newline="") as reading:
        was = reading.read()
    now = reflowed(path, strip(was, keep))
    if strip(now, keep) != now:
        raise ValueError(f"{path}: formatter introduced forbidden comments")
    if now == was:
        return 0, 0
    with open(path, "w", encoding="utf-8", newline="") as writing:
        writing.write(now)
    return 1, was.count('\n') - now.count('\n')


def main(roots):
    paths = []
    for root in roots:
        for here, _, names in os.walk(root):
            paths.extend(Path(here, name) for name in sorted(names)
                         if name.endswith((".h", ".cpp", ".hpp")) and
                         policy(Path(here, name)) is not None)
    with ThreadPoolExecutor() as pool:
        done = list(pool.map(settle, sorted(set(paths))))
    print(f"strip: {sum(one for one, _ in done)} of {len(done)} file(s) rewritten; "
          "src/ keeps no comments, include/ keeps Doxygen, test/ is untouched")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:] or ["src", "include"]))

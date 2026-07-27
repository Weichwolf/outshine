#!/usr/bin/env python3
"""Druckt eine Quelldatei ohne Kommentare und ohne Leerraum-Unterschiede.

Der Zweck ist ein Beweis, kein Werkzeug für den Build: laesst man den gesamten Baum
vor und nach einer Kommentar-Runde hierdurch und vergleicht byte-weise, ist bewiesen,
dass NUR Kommentare geaendert wurden. Ein versehentlich geloeschtes Statement faellt
sofort auf, auch wenn es zufaellig noch kompiliert.

String- und Zeichenliterale werden respektiert — ein `//` in einem WGSL-Shaderstring
(davon hat render/stages/ viele) ist Code, kein Kommentar.

    python3 sim/tools/strip_comments.py DATEI...        # je Datei ein SHA-256
    python3 sim/tools/strip_comments.py --tree sim/src   # ein Hash ueber den Baum
"""
import hashlib
import sys
from pathlib import Path

SUFFIXES = {".h", ".cpp", ".c", ".hpp"}


def strip(src: str) -> str:
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c in ('"', "'"):
            quote = c
            out.append(c)
            i += 1
            while i < n:
                if src[i] == "\\" and i + 1 < n:
                    out.append(src[i : i + 2])
                    i += 2
                    continue
                out.append(src[i])
                if src[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        if src.startswith('R"(', i):  # roher String (WGSL) — bis )"
            end = src.find(')"', i + 3)
            end = n if end < 0 else end + 2
            out.append(src[i:end])
            i = end
            continue
        if src.startswith("//", i):
            i = src.find("\n", i)
            if i < 0:
                break
            continue
        if src.startswith("/*", i):
            end = src.find("*/", i + 2)
            i = n if end < 0 else end + 2
            out.append(" ")
            continue
        out.append(c)
        i += 1
    # Leerraum normalisieren: Kommentarentfernung hinterlaesst leere Zeilen
    return "\n".join(l for l in (x.strip() for x in "".join(out).splitlines()) if l)


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    if args[0] == "--tree":
        files = sorted(
            p for root in args[1:] for p in Path(root).rglob("*") if p.suffix in SUFFIXES
        )
        h = hashlib.sha256()
        for p in files:
            h.update(p.as_posix().encode())
            h.update(strip(p.read_text(errors="replace")).encode())
        print(f"{h.hexdigest()}  {len(files)} Dateien")
        return 0
    for a in args:
        p = Path(a)
        digest = hashlib.sha256(strip(p.read_text(errors="replace")).encode()).hexdigest()
        print(f"{digest}  {p}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

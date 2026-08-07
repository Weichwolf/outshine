#!/usr/bin/env python3
"""Paarvergleiche fuer sim/web/vote — Manifest bauen und Stimmen auswerten.

  vote.py build --set grass --group "Grasdichte" a.png=0.4 b.png=0.8 c.png=1.2
  vote.py build --set look  --group "Grading" --pair a.png=warm b.png=kalt
  vote.py tally ~/Downloads/votes-grass.json

`build` kopiert die Bilder nach web/vote/shots/ und schreibt web/vote/pairs.json.
Mehrere --group in einem Aufruf sind erlaubt; jede Gruppe wird fuer sich gepaart.
Ohne --pair wird round-robin gepaart (n Varianten -> n*(n-1)/2 Vergleiche).
"""
import argparse, itertools, json, pathlib, shutil, sys, collections

ROOT = pathlib.Path(__file__).resolve().parent.parent      # sim/
VOTE = ROOT / "web" / "vote"
SHOTS = VOTE / "shots"


def build(args):
    groups, cur = [], None
    for tok in args.rest:
        if tok.startswith("--group="):
            cur = {"name": tok.split("=", 1)[1], "variants": [], "pairwise": False}
            groups.append(cur)
        elif tok == "--pair":
            if not cur: sys.exit("--pair vor --group")
            cur["pairwise"] = True
        else:
            if not cur: sys.exit("Variante ohne --group: " + tok)
            path, _, label = tok.partition("=")
            p = pathlib.Path(path).expanduser()
            if not p.is_file(): sys.exit(f"nicht gefunden: {p}")
            cur["variants"].append((p, label or p.stem))

    if not groups: sys.exit("keine --group angegeben")
    SHOTS.mkdir(parents=True, exist_ok=True)

    pairs, seen = [], {}
    for g in groups:
        if len(g["variants"]) < 2: sys.exit(f"Gruppe '{g['name']}' hat < 2 Varianten")
        # Bilder einsammeln, Dateiname kollisionsfrei ueber Gruppe + Label
        local = []
        for src, label in g["variants"]:
            slug = "".join(c if c.isalnum() or c in "-_" else "_" for c in f"{g['name']}-{label}")
            dst = SHOTS / f"{slug}{src.suffix}"
            if seen.get(dst) != src:
                shutil.copyfile(src, dst); seen[dst] = src
            local.append((f"shots/{dst.name}", label))

        combos = zip(local[::2], local[1::2]) if g["pairwise"] else itertools.combinations(local, 2)
        for (pa, la), (pb, lb) in combos:
            pairs.append({
                "id": f"{g['name']}::{la}|{lb}",
                "group": g["name"],
                "question": g["name"],
                "a": pa, "b": pb,
                "meta": {"a": la, "b": lb},
            })

    VOTE.mkdir(parents=True, exist_ok=True)
    (VOTE / "pairs.json").write_text(
        json.dumps({"set": args.set, "pairs": pairs}, indent=2, ensure_ascii=False) + "\n")
    print(f"{len(pairs)} Paare in {len(groups)} Gruppe(n) -> {VOTE/'pairs.json'}")
    print(f"Bilder -> {SHOTS}")
    print("Aufrufen:  http://localhost:8080/vote/index.html   (F = Vollbild)")


def tally(args):
    d = json.loads(pathlib.Path(args.file).expanduser().read_text())
    meta = {p["id"]: p for p in d.get("pairs", [])}
    wins = collections.defaultdict(collections.Counter)
    duels = collections.defaultdict(collections.Counter)
    side = collections.Counter()
    for pid, v in d.get("votes", {}).items():
        p = meta.get(pid)
        if not p: continue
        g = p.get("group", "—")
        win = p["meta"][v["pick"]]
        lose = p["meta"]["b" if v["pick"] == "a" else "a"]
        wins[g][win] += 1
        wins[g].setdefault(lose, 0)
        duels[g][(win, lose)] += 1
        side["links" if v["pick"] == v["leftWas"] else "rechts"] += 1

    for g, c in wins.items():
        print(f"\n{g}")
        total = sum(c.values())
        for name, n in c.most_common():
            bar = "#" * round(n / max(total, 1) * 40)
            print(f"  {name:22} {n:3}  {bar}")

    tot = sum(side.values())
    if tot:
        l = side["links"] / tot * 100
        print(f"\nSeitenverteilung: links {side['links']} / rechts {side['rechts']}  ({l:.0f}% links)")
        if abs(l - 50) > 20:
            print("  ACHTUNG: deutlicher Seitenbias — die Randomisierung sollte das verhindern.")
            print("  Bei kleiner Stichprobe ist das normal; ab ~40 Stimmen ernst nehmen.")


ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
sub = ap.add_subparsers(dest="cmd", required=True)
b = sub.add_parser("build"); b.add_argument("--set", required=True); b.set_defaults(fn=build)
t = sub.add_parser("tally"); t.add_argument("file"); t.set_defaults(fn=tally)
# --group/--pair sehen wie Optionen aus, sind aber Stellungsargumente einer Gruppenliste:
# argparse.REMAINDER wuerde sie als unbekannt abweisen, also bleiben sie roh.
a, rest = ap.parse_known_args()
a.rest = rest
a.fn(a)

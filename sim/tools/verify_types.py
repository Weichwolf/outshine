#!/usr/bin/env python3
"""verify-types: HOW MUCH THE ENGINE KNOWS ABOUT NAMED AIRCRAFT TYPES, as a number.

CLAUDE.md Prinzip 3 draws the boundary epistemically: Outshine knows the WORLD, a mod knows only what
it knows -- and "what an F-16 is" is a mod's knowledge. Today the engine holds a great deal of it. An
intention without a number stays an intention, so this gate counts it the way verify-layers counts
registry readers: the number shrinks every round, and a round that RAISES it has to say so.

WHAT COUNTS. One occurrence of a real-world AIRCRAFT designation (F-16, MiG-29, Su-27, Fulcrum, f5e,
707, ...) or of a box that exists only because one such type does -- the F-16's APG-68 / ALR-56M /
ALE-47 / APX-113 / F110 / M61A1, the MiG-29's N019 / SPO-15 / PUR-31 / BVP-30 / SAU-451 / KOLS /
GSh-301 / RD-33. The second group is aircraft knowledge under another name: an engine that knows what
an APG-68 is knows what an F-16 is.

WHAT DOES NOT COUNT, as a decision and not an oversight: ORDNANCE and GROUND types (AIM-120, Mk-82,
R-73, S-75, ZSU-23). They are a SEPARATE inventory with a separate remedy -- no round of the aircraft
work removes one -- so folding them in would blur the very number this gate sharpens. They are counted
anyway and printed on their own line, so the size of the decision is visible instead of implied.

THE BREAKDOWN IS THE POINT, not the total: it says which share is cheap and which is expensive.

  dir      the file LIVES in a type-named directory (modules/f16/)   -> move the directory out
  symbol   the token is part of a C++ identifier -- class, member, enum value, factory, macro argument
           (FBF16Module, FBFlightControl::F16(), FBGunKind::Gsh301)  -> generic class + declaration
  key      a bare lowercase string literal, i.e. a registry/asset key ("f16", "su27", "m61a1")
                                                                     -> manifest out of the mod
  text     a string literal a human reads ("F-15C Eagle")            -> label out of the manifest
  value    a CONSTANT, DEFAULT or ENUM MEMBER in generic code whose value exists only because one
           named type does -- the F-16's number in the controller    -> parameter in the declaration
  comment  every other prose mention (rationale, doc reference, measurement note)  -> trivial

WHY `value` IS A TABLE AND NOT A REGEX. It is the one class no pattern can see: the knowledge is IN
THE NUMBER, and the number does not spell the type. Measured on this tree, the obvious heuristic
(a comment naming a type, hanging on a line that declares a number) fired 83 times and was right
about a quarter of them -- `float nzRad = 0.0f;` under prose about an F-16's afterburner plume is not
an F-16 number. So `value` is a curated list with a reason per entry, exactly like verify-layers'
PERCEPTION_READERS, and each entry is CHECKED to still resolve: a stale one fails the run. It is
counted once per DECLARATION, because a declaration is the unit of repair.

A CATALOGUE ROW IS NOT A `value`. core/FBAircraft.h holds eighteen airframes with their published
numbers, and those numbers are already in a table -- their remedy is `key`/`text` (move the table into
the mod manifest), not "extract a parameter". `value` is reserved for a type's number with NO table to
move: hidden in a controller, a sensor, a pilot hook, a core constant.

Sibling of verify-layers rather than part of it, on purpose. verify-layers asserts a structure that
must hold and is therefore GREEN; this one measures a debt that is large and is therefore RED. Folding
the count into a green gate would either turn that gate red or leave the count advisory inside it --
the failure mode this gate was ordered to avoid.

Stdlib only, no build dependency. Exit 0 = no mention left, 1 = the engine still knows types (today).
"""
import argparse
import os
import re
import sys

# THE VOCABULARY: canonical type key -> the spellings that name it. Found by scanning the tree, not
# assumed. Patterns whose plain form collides with ordinary code are narrowed and the collision is
# named -- see `e3` and `b707` below; both were MEASURED false positives, not guesses.
AIRFRAMES = {
    "f16":   [r"F-16", r"F16", r"\bf16\b", r"[Vv]iper", r"Fighting Falcon"],
    "mig29": [r"MiG-29", r"MIG-?29", r"Mig29", r"\bmig29\b", r"[Ff]ulcrum"],
    "f15c":  [r"F-15C", r"\bF15c\b", r"\bf15c\b", r"\bEagle\b"],
    "su27":  [r"Su-27", r"\bSu27\b", r"\bsu27\b", r"[Ff]lanker"],
    "mig21": [r"MiG-21", r"\bMig21\b", r"\bmig21\b", r"[Ff]ishbed"],
    "mig23": [r"MiG-23", r"\bMig23\b", r"\bmig23\b", r"[Ff]logger"],
    "mig25": [r"MiG-25", r"\bMig25\b", r"\bmig25\b", r"[Ff]oxbat"],
    "mig17": [r"MiG-17", r"\bMig17\b", r"\bmig17\b", r"[Ff]resco"],
    "su7":   [r"Su-7", r"\bSu7\b", r"\bsu7\b"],
    "su22":  [r"Su-22", r"Su-20", r"Su-17", r"\bSu22\b", r"\bsu22\b", r"[Ff]itter"],
    "mirf1": [r"Mirage", r"\bMirf1\b", r"\bmirf1\b"],
    "f5e":   [r"F-5E", r"Tiger II", r"\bF5e\b", r"\bf5e\b"],
    # `E3` alone is an ENU basis vector in clients/FBAppNative.cpp (double E3[3]), so the bare capital
    # form is matched only where the catalogue actually writes it: as a spec name or a macro argument.
    "e3":    [r"E-3\b", r"\bSentry\b", r"\bkE3\b", r"(?<=_ZONES\()E3\b", r"(?<=_LAYOUT\()E3\b",
              r"\be3\b"],
    "e2c":   [r"E-2C", r"[Hh]awkeye", r"\bE2c\b", r"\be2c\b"],
    "kc135": [r"KC-135", r"Stratotanker", r"\bKc135\b", r"\bkc135\b"],
    "tu95":  [r"Tu-95", r"\bTu95\b", r"\btu95\b"],
    "an26":  [r"An-26", r"\bAn26\b", r"\ban26\b"],
    "ef111": [r"EF-111", r"\bRaven\b", r"\bEf111\b", r"\bef111\b"],
    "mi8":   [r"Mi-8", r"\bMi8\b", r"\bmi8\b"],
    "ah64":  [r"AH-64", r"\bApache\b", r"\bAh64\b", r"\bah64\b"],
    # The 707 airframe family, named as the RCS stand-in and as the KC-135's basis. The digit guard is
    # the damping ratio 0,707 in pilot/FBPilot.cpp, which is not an aeroplane.
    "b707":  [r"(?<![0-9,.])707(?![0-9])", r"\bBoeing\b"],
}

# A BOX THAT EXISTS BECAUSE ONE AIRCRAFT DOES. Keyed by its carrier, so the report can say "this line
# is F-16 knowledge" about a line that never writes "F-16".
SUBSYSTEMS = {
    "f16":   [r"AN/APG-68", r"APG-68", r"ALR-56", r"AN/ALE-47", r"ALE-47", r"APX-113", r"\bF110\b",
              r"\bM61A1\b", r"\bkM61A1\b", r"\bm61a1\b", r"\bEEGS\b", r"\bDEEC\b"],
    "mig29": [r"N019", r"N003E", r"SPO-15", r"PUR-31", r"BVP-30", r"SAU-451", r"KOLS", r"OEPS",
              r"Shchel", r"GSh-301", r"GSh-30-1", r"\bGsh301\b", r"\bgsh301\b", r"\bRD-33\b",
              r"\bSOS\b", r"\bkSos[A-Za-z]*", r"(?<![0-9.])9-1[23]\b"],
    "mig21": [r"RP-22"],
    "f15c":  [r"AN/ALR-73", r"ALR-73"],
}

# DELIBERATELY NOT IN THE VOCABULARY, and each for the same reason: the designation started at one
# airframe and is used in this tree as a COMMON NOUN for a class of box, applied to rows that never had
# the original. `FLCS` (19 uses) is "a flight control computer that closes its own rate loops" and reads
# that way in every one of them, including the generic field `int Flcs;`; `EPU` and `HMCS` likewise.
# Counting them would put a number on English usage rather than on engine knowledge. `MAX7456` is a
# Maxim display chip and no aeroplane at all.

# The sibling inventory, counted and reported on one line of its own: real-world ORDNANCE and GROUND
# types. Not aircraft knowledge, not removed by any round of the aircraft work -- see the docstring.
OTHER_TYPES = [
    r"AIM-\d+[A-Z]?", r"AGM-\d+", r"GBU-\d+", r"Mk-?8[024]\b", r"CBU-\d+", r"FAB-?\d+",
    r"\bR-\d{2}[A-Z]?\b", r"\bK-?13\b", r"S-530F", r"Super 530", r"\bMagic\b",
    r"\bS-75\b", r"\bS-125\b", r"\b9K3[23]\b", r"\b9M3[38]\b", r"\bV-?601\b", r"\bV-?750\b",
    r"ZSU-23", r"ZU-23", r"\bAzp23\b", r"\bZu23\b", r"GSh-23", r"\bDEFA\b", r"\bShilka\b",
]
RE_OTHER = re.compile("|".join(OTHER_TYPES))

# A TYPE'S NUMBER WITH NO TABLE TO MOVE IT INTO. (file, anchor, type, why) -- `anchor` is the exact
# declaration text and must still be present, and a spelling of `type` must still stand within the
# twelve lines above it; either failing is a STALE entry and fails the run. One entry = one
# declaration = one unit of repair.
VALUES = (
    ("core/FBAircraft.h", "Peer,", "f16",
     "the top tier of the ladder IS 'everything the F-16 accepts'"),
    ("core/FBCommandBus.h", "kHotasLatencyS = 0.5", "f16",
     "the F-16's documented short/long press discriminator"),
    ("core/FBCountermeasure.h", "struct FBCmProgram", "f16",
     "the dispense program's schema is the AN/ALE-47's, field for field and range for range"),
    ("core/FBDamageModel.h", "kAuthorityDegraded = 0.5", "f16",
     "half authority because the F-16 has two independent hydraulic systems"),
    ("core/FBDirector.h", "enum class FBDirectorRefusal", "mig29",
     "every refusal is a documented boundary of the MiG-29's director delivery"),
    ("core/FBAvionicsCommand.h", "RadarEmission,", "mig29",
     "a command target only a set with a separate emission switch has (the PUR-31)"),
    ("core/FBAvionicsBlocks.h", "int  EmissionOrdinal = -1;", "mig29",
     "a published block field for the same switch; -1 = the aircraft that has none"),
    ("core/FBCommandBus.cpp", "case FBCommandTarget::RadarEmission:", "mig29",
     "classified Ded, not Hotas, because of where the MiG-29 puts that switch"),
    ("sensors/FBRadarSystem.h", "kRefRcsM2 = 1.2", "f16",
     "the range equation's calibration aircraft, so an F-16 gate against an F-16 is exactly 1.0"),
    ("sensors/FBCountermeasureSystem.h", "kProgramCount = 6", "f16",
     "PRGM 1-4 plus slap plus bypass is the F-16 dispenser's count"),
    ("units/FBUnit.h", "bool IffXpdr = false;", "f16",
     "the Mode 4 transponder is named as the AN/APX-113"),
    ("pilot/FBPilot.h", "virtual double RotationSpeedKt", "f16",
     "the takeoff/landing hook block's defaults are the F-16's speeds"),
    ("pilot/FBPilot.h", "virtual double BfmRollPlantA", "f16",
     "roll time constant, identified on the F-16"),
    ("pilot/FBPilot.h", "virtual double BfmRollPlantKDegS", "f16",
     "roll rate per full deflection, identified on the F-16"),
    ("pilot/FBPilot.h", "virtual double BfmRollRateMaxDegS", "f16",
     "the commanded roll cap 'bleibt die F-16'"),
    ("pilot/FBPilot.h", "virtual double BfmSearchRollCap", "f16",
     "default 1.0 chosen so the F-16's search stays bit-identical"),
    ("pilot/FBPilot.h", "virtual double BfmWvrCueDeg", "f16",
     "-1 = no helmet sight, which is the F-16's answer and not a neutral one"),
    ("pilot/FBPilot.h", "virtual int    BfmRadarModeOrdinal", "f16",
     "-1 = the F-16, whose ACM mode is not commanded from here"),
    ("pilot/FBPilot.h", "virtual bool SupportInhibitsDefend", "f16",
     "false = the F-16 shooter's doctrine; the MiG-29's is the other way"),
    ("pilot/FBPilotTuning.cpp", '{"pilot_lock_nm"', "f16",
     "the tuning band's 40 nm ceiling is the APG-68's gate"),
    ("systems/FBAutopilot.h", "double BankMaxDeg, KHdg, KAlt;", "f16",
     "the gain block's defaults are the flown F-16 preset"),
    ("systems/FBFlightControl.h", "double PitchStickMax;", "f16",
     "1.0 = no cap, and that encoding is the F-16's FLCS sitting behind the output"),
    ("systems/FBFlightControl.h", "double KqDamp, KpDampRoll;", "f16",
     "both 0 for a cell with its own FLCS, i.e. shaped around the F-16"),
    ("systems/FBFlightControl.h", "double AlphaLimitDeg;", "f16",
     "0 = off, and that is the F-16 (its limiter is in its own FLCS)"),
    ("systems/FBFlightControl.h", "double GLimitG;", "f16",
     "0 = off, and that is F-16 and MiG-29 both"),
    ("systems/FBFlightControl.cpp", "double byAlpha = kSosKp *", "mig29",
     "the AoA limiter's law and gains were measured on the MiG-29, and its name is that jet's SOS"),
    ("modules/air/FBAirFireControl.h", "kFunnelNearS = 0.178", "f16",
     "the F-16's published EEGS window divided by the M61A1's muzzle velocity"),
)

RE_HEX = re.compile(r"0[xX][0-9A-Fa-f]+")
RE_EXP = re.compile(r"\d[eE][-+]?\d")
RE_KEYISH = re.compile(r"^[a-z0-9_.-]{1,16}$")
SRC_EXT = (".h", ".cpp")

ART_ORDER = ("dir", "symbol", "key", "text", "value", "comment")
REMEDY = {
    "dir":     "move the directory out of the engine",
    "symbol":  "generic class + a declaration",
    "key":     "manifest out of the mod",
    "text":    "label out of the manifest",
    "value":   "parameter in the declaration",
    "comment": "prose, trivial",
}


def classify_chars(src):
    """Per character: 'c' code, '/' comment, '\"' string. A hand lexer, because a type name inside a
    string is a different FACT from one inside a comment and guessing would blur exactly that."""
    out = bytearray(b"c" * len(src))
    i, n = 0, len(src)
    while i < n:
        ch = src[i]
        if ch == "/" and i + 1 < n and src[i + 1] == "/":
            j = src.find("\n", i)
            j = n if j < 0 else j
            out[i:j] = b"/" * (j - i)
            i = j
        elif ch == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out[i:j] = b"/" * (j - i)
            i = j
        elif ch in "\"'":
            q, j = ch, i + 1
            while j < n and src[j] != q:
                j += 2 if src[j] == "\\" else 1
            j = min(j + 1, n)
            out[i:j] = b'"' * (j - i)
            i = j
        else:
            i += 1
    return out.decode()


def literal_around(src, pos):
    a = src.rfind('"', 0, pos)
    b = src.find('"', pos)
    return "" if a < 0 or b < 0 else src[a + 1:b]


def line_starts(src):
    return [0] + [m.end() for m in re.finditer(r"\n", src)]


def line_of(starts, pos):
    lo, hi = 0, len(starts) - 1
    while lo < hi:
        mid = (lo + hi + 1) // 2
        lo, hi = (mid, hi) if starts[mid] <= pos else (lo, mid - 1)
    return lo + 1


def build_patterns():
    return [(re.compile(a), key, family)
            for family, table in (("airframe", AIRFRAMES), ("subsystem", SUBSYSTEMS))
            for key, alts in table.items() for a in alts]


def scan_file(src, rel, pats):
    kinds = classify_chars(src)
    starts = line_starts(src)
    hits, seen = [], []
    for rx, key, family in pats:
        for m in rx.finditer(src):
            s, e = m.span()
            if any(s < b and e > a for a, b in seen):
                continue
            ctx = src[max(0, s - 2):e + 2]
            if RE_HEX.search(ctx) or RE_EXP.search(ctx):
                continue   # a designation inside a hex constant or an exponent is arithmetic
            seen.append((s, e))
            k = kinds[s]
            if k == '"':
                art = "key" if RE_KEYISH.match(literal_around(src, s)) else "text"
            elif k == "c":
                art = "symbol"
            else:
                art = "comment"
            hits.append((rel, line_of(starts, s), key, family, art, m.group(0)))
    return hits


def type_dirs(rel, keys):
    return sorted({p for p in os.path.dirname(rel).split("/") if p in keys})


RE_FILE_HEAD = re.compile(r"\A(?:\s|/\*.*?\*/|//[^\n]*\n)*", re.S)


def check_values(text, pats):
    """Every VALUES entry must still resolve: the anchor present, and its type named where the claim
    lives -- the twelve lines up to the end of the anchor's own line (a leading block or a trailing
    comment), or the file's leading comment when the claim is about the whole file. A curated list
    that has stopped pointing at anything is worse than no list."""
    hits, errors = [], []
    by_key = {}
    for rx, key, _f in pats:
        by_key.setdefault(key, []).append(rx)
    for rel, anchor, key, why in VALUES:
        src = text.get(rel)
        if src is None:
            errors.append(f"STALE: {rel} does not exist (VALUES entry '{anchor}')")
            continue
        starts = line_starts(src)
        head = RE_FILE_HEAD.match(src).group(0)
        found, first = None, None
        pos = src.find(anchor)
        while pos >= 0:
            ln = line_of(starts, pos)
            first = first or ln
            eol = starts[ln] if ln < len(starts) else len(src)
            window = src[starts[max(0, ln - 12)]:eol] + head
            if any(rx.search(window) for rx in by_key[key]):
                found = ln
                break
            pos = src.find(anchor, pos + 1)
        if first is None:
            errors.append(f"STALE: {rel} no longer declares `{anchor}`")
        elif found is None:
            errors.append(f"STALE: {rel}:{first} `{anchor}` no longer names {key} at its declaration")
        else:
            hits.append((rel, found, key, "airframe", "value", anchor))
    return hits, errors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.join(os.path.dirname(__file__), "..", "src"))
    ap.add_argument("--list", action="store_true", help="every mention, one per line")
    a = ap.parse_args()
    root = os.path.normpath(a.root)
    pats = build_patterns()
    keys = set(AIRFRAMES)

    hits, other, files, text = [], 0, 0, {}
    for dp, dns, fns in os.walk(root):
        dns[:] = [d for d in dns if not d.startswith(".")]
        for fn in sorted(fns):
            if not fn.endswith(SRC_EXT):
                continue
            files += 1
            rel = os.path.relpath(os.path.join(dp, fn), root).replace(os.sep, "/")
            with open(os.path.join(dp, fn), encoding="utf-8", errors="replace") as f:
                src = f.read()
            text[rel] = src
            for k in type_dirs(rel, keys):
                hits.append((rel, 0, k, "airframe", "dir", os.path.dirname(rel)))
            hits += scan_file(src, rel, pats)
            other += len(RE_OTHER.findall(src))

    vhits, errors = check_values(text, pats)
    hits += vhits
    if errors:
        for e in errors:
            print(f"verify-types: {e}", file=sys.stderr)
        print(f"verify-types: FAILED — {len(errors)} stale VALUES entr(y/ies) in tools/verify_types.py",
              file=sys.stderr)
        return 1

    by_art, by_key, by_file = {}, {}, {}
    for rel, ln, key, family, art, tok in hits:
        by_art[art] = by_art.get(art, 0) + 1
        by_key[key] = by_key.get(key, 0) + 1
        by_file[rel] = by_file.get(rel, 0) + 1

    if a.list:
        for rel, ln, key, family, art, tok in sorted(hits, key=lambda h: (h[0], h[1])):
            print(f"{rel}:{ln}\t{key}\t{family}\t{art}\t{tok}")

    ndirs = len({os.path.dirname(t) for r, l, k, f, art, t in hits if art == "dir"})
    print(f"verify-types: {len(hits)} mention(s) of a named aircraft type in {len(by_file)} of "
          f"{files} file(s) under src/")
    print("verify-types:   art      count  remedy")
    for art in ART_ORDER:
        extra = f" ({ndirs} tree(s))" if art == "dir" else ""
        print(f"verify-types:   {art:<8} {by_art.get(art, 0):>5}  {REMEDY[art]}{extra}")
    print("verify-types: by type: " +
          ", ".join(f"{k}={v}" for k, v in sorted(by_key.items(), key=lambda kv: (-kv[1], kv[0]))))
    print("verify-types: files (mentions):")
    for rel, n in sorted(by_file.items(), key=lambda kv: (-kv[1], kv[0])):
        print(f"verify-types:   {n:>4}  {rel}")
    print(f"verify-types: not counted, own inventory: {other} ordnance/ground-type mention(s)")
    sys.stdout.flush()
    if hits:
        print(f"verify-types: FAILED — the engine names {len(by_key)} aircraft type(s) in "
              f"{len(by_file)} file(s); Prinzip 3 wants none (doc/mods.md)", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

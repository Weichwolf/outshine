Type: bug
Area: core
Regresses: 1737

**The JSON number's out-of-range verdict reads underflow as zero, never as the far edge**

src/core/Json.cpp:178-180: every `result_out_of_range` from `from_chars` is mapped to
±DBL_MAX by the leading sign. But this platform's `from_chars` reports out_of_range for
UNDERFLOW too — measured on this machine: `"1e-999"` → `ec=34, v=0`; `"1e999"` →
`ec=34, v=inf`. The handler then overwrites the correct 0 with **+1.7976931348623157e308**:
a hostile glTF `{"scale": 1e-999}` becomes 1.8e308 instead of ~0 — the largest possible
misparse from the smallest possible number.

This is the RECURRENCE of 1701's class ("ToNumber's range verdict reads the magnitude, not
the sign's position"), fixed there in src/core/Script.cpp with the effective-decimal-exponent
rule — and reintroduced one directory over by the 1737 fix, in the same session. The tree
already carries the correct pattern; the fix did not consult it.

Demanded: on out_of_range, keep from_chars' own stored value where it is finite (underflow →
±0), and clamp only the infinite result to ±DBL_MAX — or derive the effective exponent as
1701 did. The proof gains the discriminating arms `1e-999` → 0, `-1e-999` → -0 (signbit),
beside the existing `1e999` edge in AJsonDoorRefusesWhatIsNotJsonAndSurvivesWhatIsHostile.
Negative control: the current clamp reverted-in must go FAIL on exactly these arms.

---

Closed -- the judge is ONE now: src/core/DecimalEdge.h holds the first-significant-digit
rule (1701's), Script.cpp's TextToNumber and Json.cpp's out-of-range arm both consult it,
and the second spelling that let the class recur died in the consolidation. A hostile
1e-999 lands at ±0 (never 1.8e308) and 401 fraction zeros judge the same; 1e999 still
lands at the far edge. Proven in AJsonDoorRefusesWhatIsNotJsonAndSurvivesWhatIsHostile
(tiny == 0, dust == 0, edge > 1e308; negative control: the sign-only clamp reverted fails
exactly these arms) with the Script suite standing unchanged on the shared judge.

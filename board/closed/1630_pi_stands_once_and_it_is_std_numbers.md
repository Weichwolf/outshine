Type: task
Area: core
Tags: hygiene, c++23

**Pi stands once, and it is std::numbers**

Three spellings of the same constant at HEAD: src/core/Units.h:6 `kPi = 3.14159265358979323846`,
src/ground/tiles/TileMath.h:12 the identical literal again, and src/data/Wgs84.h:9 a THIRD raw
literal inline in `kMercatorGirthM = 2.0 * 3.14159265358979323846 * kWgs84A` -- written this
hour (3a59ed6) under the very commit that put C++23 on the map. The 23 line makes the one
origin free: `std::numbers::pi_v<double>`.

Demanded: every pi in src/ derives from std::numbers::pi (kPi may remain as a named alias in
ONE header if the short name earns its keep); Wgs84.h and TileMath.h stop spelling digits.

---

Sharpened (review 2026-08-22, night round): the sweep (5b5cc642) converted the digit
spellings but left the MACRO spelling standing -- four `M_PI` at src/core/Camera.h:20,40 and
src/core/Ephemeris.h:14,26. M_PI is POSIX, not C++: it is a fourth origin for the same
constant and not even guaranteed by the standard this tree declares. And the alias now stands
in TWO headers where this item allows one: src/core/Units.h:8 and
src/ground/tiles/TileMath.h:13 both spell `constexpr double kPi = std::numbers::pi;` -- the
ground layer imports core, so TileMath's copy is a duplicate, not a need. The MSL raw strings
keep their literals by 6f9f13a6's ruling; that exception belongs to 1634/1580, not here.

---

Closed (review 2026-08-22 late): the sharpened residues are repaid in the tree. `grep -rn
M_PI src/` returns nothing — Camera.h and Ephemeris.h no longer spell the macro; `kPi` stands
in exactly one header (src/core/Units.h:8, `= std::numbers::pi`), TileMath.h's duplicate is
gone; the only remaining digit spellings sit inside the MSL raw strings (ParticipatingMedium.h
after :375, SkyStage's kernel text), which keep their literals by 6f9f13a6's ruling — that
exception is 1634/1580's ledger, not this item's.

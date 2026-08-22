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

# Core (`sim/src/core/`)

**Source**: the source files themselves — `sim/src/core/` (**27 files**), `namespace outshine`, no class
prefix.

**What this layer is responsible for**: the VALUE TYPES of the simulation, the shared PRIMITIVES
(geodesy, units, matrices, calendar, ephemeris, the elevation hook, the weather providers) and the two
OBSERVATION CHANNELS (log, telemetry).

**What it is NOT responsible for**: behaviour. No control law, no sensor, no brain, no renderer, no
solver seam. `core/` owns no entity. It is used by everything above it and uses none of it.

**This file is deliberately shorter than the directory.** The value types whose writers were deleted
with the simulation layer on 2026-08-07 are named once in `## State`'s last row and described nowhere:
describing a type that nothing produces would be documenting the past.

---

## Spec

`core/` is the value layer: the shared primitives and the two observation channels. It carries **no
behaviour**.

| Contract | Acceptance / measurement anchor |
|---|---|
| `core/` never points at any layer above it | include graph; `grep -rn '#include' sim/src/core` finds only `core/`'s own headers and standard headers |
| I/O-free, not format-free | no `FILE*`/`fstream`; `snprintf` into a local buffer is allowed ([`conventions.md`](conventions.md)) |
| Events run through `Log`, periodic state through `TelemetryBus` | no scattered `printf` outside the sink implementations and `clients/` |
| Every number carries its provenance | derived / measured / `[SET]` — see [`conventions.md`](conventions.md) |

## State

**`core/` is 27 files** and everything in it is reachable from the two clients.

| Piece | Status |
|---|---|
| `Log` / `Telemetry`, thread-local context for the parallel path | built |
| `State.h` + `AvionicsBlocks.h` — **two blocks left**, `Platform` and `Env`; the other 22 went with their writers | built, and read by `Renderer` and `SubjectBench` only |
| `ElevationProvider.h` — the hook | built, header-only. **No implementation ships**: constant, runway-plateau, baked-DEM and tiles providers are all deleted; both clients read ground straight out of `world/terrain`'s stream |
| `Geodesy`, `Units`, `Mat4`, `Camera` | built, header-only, subject-independent |
| Calendar (`CivilTime.h`) + sun/moon ephemeris (`Ephemeris.h`) | built. Pure functions, no state; the ephemeris sits in `core/` because `core/` may not include `render/` |
| Weather providers (`WeatherProvider.h` + `CalmWeather`, `ConstantWindWeather`) | built — see [`world/weather.md`](world/weather.md) |
| `Store.h`, `Countermeasure.h`, `Emitter.h`, `Flight.h`, `NetReport.h`, `Team.h`, `VisualContact.h`, `WeaponUplink.h`, `Mode.h`, `BlockStatus.h` | **the last combat value types**, alive only because `world/World.cpp` includes `units/Unit.h` for its effect path. Measured 2026-08-07: `Countermeasure.h` carries 3 of the 4 aircraft-type mentions `verify-types` still counts, plus most of its 68 uncounted ordnance mentions |

## Gaps

1. **There is no judge and no damage model.** What a physical K.O. means for a walking body, a vehicle
   and a rigid object is [`body-format.md`](body-format.md)'s work.
   **A type-level write gate — every mutator private, exactly one friend, and no self-healing that
   compiles — is the single most load-bearing shape that went with the simulation layer**, and nothing
   replaces it. `verify-guards`, which proved it by trying to break it, went too.
2. **`ElevationProvider` has no implementation in the tree.** The header is on both clients' include
   lines and nothing derives from it; ground comes out of `world/terrain`'s tile stream directly. Either
   an implementation lands or the hook goes.
3. **`core/` still carries ten value types it cannot justify** — see the last row of `## State`. They
   leave when `world/World.cpp`'s effect path stops needing `units/Unit.h`.

The **three-state validity head** (`Invalid` / `Valid` / `Held`) is the one idea from the deleted block
bus worth carrying forward on its own terms: a consumer that must tell "no value" from "a stale value"
from "a current value" is not an avionics problem. `BlockStatus.h` still carries it.

---

## Knowledge

Derivations, formulas and measured constants.

### 0. The rule that defines `core/`

| Rule | Evidenced by |
|---|---|
| `core/` NEVER includes `units/`, `render/`, `world/` or `clients/` | `grep -rn '#include' sim/src/core` finds exclusively `core/`'s own headers and standard headers |
| `core/` is I/O-free — no `FILE*`, no `fstream`, no `printf` | two named exceptions, below |
| Formatting is allowed | `snprintf` into a local buffer (`Log.cpp`, `Telemetry.cpp`) |
| Events run through `Log`, periodic state through `TelemetryBus` | no scattered `printf` anywhere below `clients/` |

**The I/O exceptions, both justified:**

| File | What it does | Why it is not a violation |
|---|---|---|
| `Log.cpp`, `Telemetry.cpp` | `<cstdio>` for `snprintf` — pure formatting into local buffers | no file handle, no stream; the SINKS live in `clients/` |

---

### 1. The two channels

The separation is sharp and mutually referenced in both banners:
**log = discrete events, telemetry = periodically sampled state.**

#### 1.1 `Log` — discrete, greppable events

`core/Log.h/.cpp`. A STATIC FACADE, not an owned object: logging is cross-cutting infrastructure every
layer needs, and threading a `Log&` through every `Run()` signature would touch the whole call graph
without gaining behaviour. A call site stays a one-liner:

```cpp
Log::Warn("walk", "sink_rate_high", {{"vs", -12.3}});
```

**I/O-free**: emission happens only if a `LogSink` is injected (`Log::SetSink`); without a sink it costs
a pointer comparison and no formatting. The concrete sinks (stdout/file/fan-out, plus the buffered sink)
live in `clients/LogSinks.h` — the one place where raw stdio is allowed.

| Level | `LogLevel::Debug / Info / Warn / Error` |
|---|---|
| Default level | `Debug` — the browser console is supposed to look unchanged; whoever wants a quieter channel raises the level explicitly |

**`LogField`** is a `key=val` field: numeric overloads format compactly (`%g`), `int` as a decimal,
`bool` as `0`/`1`, strings unchanged — the sink quotes a value containing spaces.

**Threading** — the decisive split for a parallel run:

| What | Storage class | Why |
|---|---|---|
| `Sink_`, `Level_` | process-wide static | CONFIGURATION, set once at boot |
| `TimeS_`, `Unit_[32]`, `ThreadSink_` | `thread_local` | CONTEXT: a thread computing one actor IS in a different context from one computing another. The alternative — a context object through every `Run()` signature — is exactly what this facade avoids |

Single-threaded clients see identical behaviour: one thread, one context.

**Unit attribution**: `Log::SetUnit(label)` — if set, every line carries `unit=<callsign>` as its FIRST
field (a script splits on the first field; a human sees whose line it is without scanning). If empty,
NOTHING is added: the lines of a single actor are the run's lines and need no attribution — which also
keeps them byte-identical to a baseline taken before multi-actor runs existed. `Unit_` is a fixed 32-byte
buffer: it changes per actor per tick and must never allocate.

**`Log::SetThreadSink(sink)`** redirects the output of THIS thread. A parallel runner points every worker
at the buffer OF THE ACTOR it is computing, never at the shared event log — a worker writing straight
through would make the line order a function of the scheduler. Buffers are drained at the tick barrier in
actor order. **Level and "is anybody listening at all" stay the question of the PROCESS sink** — a
capture buffer is a redirection of an already accepted line, not a second switch.

**Two RAII scopes**, so no state leaks past an actor:

| Class | Effect |
|---|---|
| `LogUnitScope(label)` | sets the attribution and clears it in the destructor — no label can leak onto the next actor's lines or onto the run-wide lines between loops |
| `LogThreadSinkScope(sink)` | the same discipline for the capture buffer — a worker returning without clearing would keep writing into it next tick, possibly into another actor's buffer |

#### 1.2 `Telemetry` — a time series with a schema

`core/Telemetry.h/.cpp`. Classes DECLARE themselves as a source; the emission is CENTRAL.

| Type | Role |
|---|---|
| `TelemetryChannel` | `{Name, Unit}` |
| `TelemetrySchema` | ordered channel list, `Add(name, unit="")` |
| `TelemetryRow` | field buffer; `Push(double)` formats with `%.6f`, plus `int`/`bool`/`string` overloads |
| `TelemetrySource` | interface: `TelemetryName()`, `DeclareTelemetry(schema)` (ONCE, at `Bus::Start()`), `SampleTelemetry(row)` (ONCE per `Bus::Tick()`) |
| `TelemetrySink` | interface: `Header(columns)`, `Row(fields)` — concrete implementation in `clients/` |
| `TelemetryBus` | the ONE emitter: `Register(src)` (a BORROWED pointer, the bus never owns a system), `SetSink`, `Start()`, `Tick(simTimeS)` |

**The rule that holds it together**: a row arises by CONCATENATION — every source pushes exactly as many
fields as it declared channels, in the same order. **Declaration = registration = column order**, no
string-indexed lookup at sample time.

`Start()` first creates the channel `t` (unit `s`), then every source in registration order, and pushes
the header out. `Tick()` starts itself if necessary, pushes the sim time and samples every source into
one row. **A null sink makes `Tick()` a cheap no-op.**

**The append rule**: new sources are ALWAYS appended at the end, so no column ever measured loses its
position. A new subject therefore registers its own source rather than adding columns to an existing
one.

---

### 2. The elevation hook

`core/ElevationProvider.h`. The ONE seam through which every core consumer of ground elevation goes —
spawn on the ground, AGL, ground-contact detection — so that "where is the ground" is an INJECTED
dependency and not a hard-wired tile-server wire.

```cpp
class ElevationProvider {
  virtual double GroundElevM(double latDeg, double lonDeg) const = 0;
  virtual bool GroundElevPatch(latMin, lonMin, latMax, lonMax, cols, rows, double *out) const;
};
```

**`GroundElevPatch`** is the area query for terrain-aware guidance: it fills `out` row by row
(`cols`×`rows`, **row 0 = southern edge, column 0 = western edge**). The default implementation loops over
`GroundElevM` — correct for every provider; an implementation may override it as soon as a real batch
path (one DEM tile decode over the whole patch) is worth the code. It returns `false` exactly when `out`
is null or the grid is degenerate (`cols<2` or `rows<2`); a single unresolved sample writes the sentinel
and does NOT make the patch fail.

**The sentinel:**

| Symbol | Value | Meaning |
|---|---|---|
| `kFBElevationUnresolved` | **−1e9** | "not resolved yet" — matches the streaming layer's own convention, so the tiles provider is a pure pass-through. One of the few surviving `FB`-prefixed symbols, and an unbooked leftover of the rename |
| `ElevationResolved(m)` | `m > -1e8` | the ONE "is this sample usable" check. Every caller used to write `sample > -1e8` by hand — the same magic threshold in three places; a named predicate keeps it one rule |

**All implementations are SYNCHRONOUS from the caller's point of view**: a client that is itself
asynchronous (WASM) polls `GroundElevM` until it stops returning the sentinel.

#### 2.1 No implementation ships

`ConstantElevation`, `RunwayPlateauElevation`, `BakedDemElevation` and `world/TilesElevation` were all
deleted on 2026-08-07 with the headless client that was their only caller. Both surviving clients read
ground out of `world/terrain`'s tile stream directly and never construct a provider. **The hook is a
header with no derived class in the tree** — see `## Gaps` 4.

### 3. Geodesy, units, mathematics

#### 3.1 `Geodesy` — the ONE planar ENU geodesy

`core/Geodesy.h`, header-only, no translation unit.

**Why this file exists**: the same five-line block `dlat*111320, dlon*111320*cos(lat)` stood in SIX
places — **and only some of them wrapped the longitude difference into [−180,180]**. The unwrapped copies
read a ~360° delta across the antimeridian, hence **~38 000 km of distance to a point one metre further
on**: at 180° of longitude, waypoint capture, plateau elevation and the displayed home distance were
simply wrong. The wrapping is now part of the primitive, not something every caller has to remember.

**CONVENTION**: the reference point comes FIRST and owns the cosine. `EnuOffsetM(ref, p)` returns the
offset of `p` FROM `ref`, with the longitude scaling at the REFERENCE latitude — one rule, so a bearing
and a distance computed by two different subsystems agree. Whoever only needs a distance may pass either
point as the reference (the offsets differ only in sign; the magnitude is identical).

**SCOPE**: deliberately planar and small-angle, matching what every call site did anyway. Real geodetic
mathematics belongs to whatever needs it, not to the callers of this file.

| Function | Contract |
|---|---|
| `GeoToEcef(lat, lon, alt, out[3])` | **WGS84 geodetic → ECEF (m). The one function here that is NOT small-angle** — the exact ellipsoid conversion the renderer's camera-relative ECEF world stands on (`a = 6378137.0`, `e² = 6.69437999014e-3`) |
| `EnuAxesEcef(lat, lon, E, N, U)` | the local ENU axes in ECEF — the rotation every ECEF vector/camera conversion begins with |
| `Wrap180(deg)` | angle difference in [−180,180]. The LOOP form (not `fmod`) is what every call site used; it is exact for the one-or-two-revolution deltas that occur |
| `EnuOffsetM(refLat, refLon, lat, lon, eastM, northM)` | planar offset: `north = Δlat · kMPerDeg`, `east = Wrap180(Δlon) · kMPerDeg · cos(refLat)` |
| `PlanarDistM(...)` | horizontal distance (unsigned) |
| `BearingDeg(ref, p)` | true bearing 0..360 (`atan2(e, n)`) |
| `EnuToBodyLos(roll, pitch, yaw, e, n, u, azDeg, elDeg)` | **line of sight → body frame**: ENU in, body-referenced azimuth/elevation out (+az right of the nose, +el above the boresight plane) — the standard NED→body Euler sequence `Rx(roll)·Ry(pitch)·Rz(yaw)` applied to the offset, hence **WHAT A SENSOR SEES** instead of what a map would show |
| `BodyLosToEnu(...)` | the exact inverse, unit length |
| `BodyVecToEnu(roll, pitch, yaw, fwd, right, down, e, n, u)` | a BODY vector (+forward/+right/+down, any unit) into local ENU. **Built on `BodyLosToEnu` instead of on a second copy of the Euler sequence** — two diverging spellings of one rotation are exactly the class of error this file exists against |
| `EnuToBodyVec(...)` | the exact inverse of that, built on `EnuToBodyLos`. A detonation happens at a point in the world, and what decides which systems it destroyed is where that point sits along the TARGET's own axis |
| `TrackProjectM(refLat, refLon, courseDeg, lat, lon, alongM, acrossM)` | **along/across projection** onto the line through the reference on the true course: +along down the course, +across to the right. **One definition, so "on the line" means the same for whoever flies it and whoever judges it** |

#### 3.2 `Units` — the ONE definition of every conversion factor

`core/Units.h`, header-only, `constexpr`.

**Why this file exists**: the same numbers were re-declared privately file by file — `kMPerDeg` six
times, `kMsToKt` five times, π six times — **and one of them had DRIFTED**: knots→m/s stood as
`0.51444444444` in one place and `0.5144444444` in another, so a declared speed and a commanded speed
were converted with different precision. Exactly the class of error that multiplies as soon as several
actors run at once, and that no reader can see from inside one file.

| Constant | Value | Status |
|---|---|---|
| `kPi` | 3.14159265358979323846 | |
| `kDeg2Rad` | `kPi/180` | |
| `kRad2Deg` | 57.29577951308232 | |
| `kMPerDeg` | **111320.0** | metres per degree of latitude, spherical approximation. Valid for the tens-of-kilometres scales this engine measures over, not for intercontinental geodesy |
| `kFtToM` | **0.3048** | **exact**, by definition of the international foot |
| `kMToFt` | `1/kFtToM` | |
| `kNmToM` | **1852.0** | **exact**, by definition of the nautical mile |
| `kMToNm` | `1/kNmToM` | |
| `kKtToMs` | `kNmToM/3600` | **exact**: 1 kt = 1 nm/h |
| `kMsToKt` | **1.9438444924406** | **a historical 14-digit literal, deliberately kept**: every consuming site already agreed on it bit-for-bit, and re-deriving it as `3600/1852` would shift measured numbers for no gain |

**Values are EXACT definitions where one exists** — writing the ratio instead of a truncated decimal is
both more accurate and self-documenting. The project is metric and decimal throughout; the imperial
factors exist because external data and instruments still speak them, not because anything internal does.

#### 3.3 `Mat4` — renderer mathematics

`core/Mat4.h`, header-only, `namespace outshine`. **Column-major, OpenGL convention**: element `m[c*4+r]`
is column c, row r; multiplication with a column vector `v` gives `m*v`.

**Why it is a file of its own**: it is the one part of the renderer that needs NO GPU context — pure
float mathematics, hence directly assertable instead of judged on pixels. **A wrong sign here does not
crash**; it silently mirrors the world or turns the camera inside out — exactly the class of error a unit
test catches and a pair of eyes does not.

| Function | Contract |
|---|---|
| `Mat4Identity(m)` | identity matrix |
| `Mat4Mul(o, a, b)` | `o = a·b` (via an intermediate buffer, hence aliasing-safe) |
| `Mat4Perspective(m, fovy, asp, zn, zf)` | **REVERSED-Z perspective**: near maps to NDC z=+1 (window depth 1.0), far to −1 (0.0) — the standard `zn`↔`zf` swap in the z row. Together with a cleared depth of 0, a `GEQUAL` test and a 32-bit float depth buffer, the projection's 1/z curve cancels the distribution of the float mantissa and delivers **nearly uniform precision over 0.01 m…240 km**, where plain depth z-fights distant terrain into shimmering. x/y (`m[0]`, `m[5]`) and w (`m[11]`) are unchanged, so screen projection, manual overlay projection and frustum extraction stay untouched |
| `Mat4LookAt(m, eye, ctr, up)` | world → view: camera at `eye`, looking at `ctr`, with approximately `up` as up |
| `Vec3Normalize(v)` | normalises (a no-op below length 1e-6) |
| `Vec3Cross(o, a, b)` | cross product |

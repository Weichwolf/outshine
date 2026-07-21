# FlightBox Mission Format

A mission is **declarative data** (JSON/YAML). The test runner plays it through real iNav-in-the-loop and
**verifies** it. One format spans the whole spectrum: point-to-point, reconnaissance, observation/overwatch,
interception, patrol/CAP, area search, escort, relay, perimeter, strike pass, terrain-following ingress, SAR.

Two things are kept strictly separate:

- **COMMANDABLE** — what the Command Center tells iNav to fly (RC modes + `MSP_SET_WP` actions + safehome/
  approach). iNav flies it natively; the CC synthesises no attitude. Bounded by real iNav limits (§6).
- **VERIFIABLE** — predicates evaluated over the flown telemetry stream. The runner is a **predicate
  evaluator**, not per-type bespoke code: a new mission type = new geometry + a predicate composition.

A mission passes iff **every phase completes, every task's `verify` predicates hold, and the global
`envelope` was never violated**. The runner **aborts immediately on the first violation** (fail-fast) — it
names the phase/task/predicate that failed with the measured value, and stops the run, so development
iterates quickly instead of flying out a doomed mission to timeout.

---

## 1. Measurement model — what the runner can actually assert

Predicates are defined over the sampled flight track `S` (one tuple per tick). Sources:

| Symbol | Field | Source | Notes |
|---|---|---|---|
| `t` | sim-clock seconds | bridge | |
| `lat,lon` | GPS position | iNav GPS (`--sim=xp`) via `MSP_RAW_GPS` | |
| `h_asl` | GPS altitude ASL | `MSP_RAW_GPS` | |
| `h_agl` | AGL = `h_asl − DEM(lat,lon)` | GPS + **CC-side DEM** (Terrain lives in the CC) | exact host-side |
| `φ,θ,ψ` | roll, pitch, heading | `MSP2_INAV_..._POSE` (0.1°) | |
| `GS` | groundspeed | `MSP_RAW_GPS` | wind-contaminated proxy for airspeed |
| `χ` | ground course (track) | `MSP_RAW_GPS` groundCourse | `χ ≠ ψ` in wind (crab) |
| `VS` | vertical speed | `MSP_ALTITUDE` vario / d`h`/d`t` | |
| `nav_state,wp` | iNav NAV FSM state, active WP | `MSP_NAV_STATUS` | 5=WP-enroute, 3/4=HOLD, 9=LAND, 10=LANDED |
| `IAS` | indicated airspeed | **JSBSim `[flt]` log only** | `MSP2_INAV_AIR_SPEED`=0 in SITL (no pitot) |

**Ground-truth 3D track** for ring scoring comes from the JSBSim `[flt]` container log (full path, incl. a
`NaN`/crash flag), not MSP — MSP position is the same GPS, but the `[flt]` line also carries airspeed and the
crash flag. Airspeed and any true-attitude scoring therefore read the `[flt]` log.

**Rules that keep predicates honest:**
- **Position-based predicates are wind-robust** (`range`, cross-track `XTE`, coverage); **heading-based are
  not** — use track `χ` for geometry, reserve heading `ψ` only where *pointing* physically matters (sensor
  line-of-sight, strike run-in axis).
- **Coverage is a geometric surrogate, never a pixel claim**: a sensor swath modeled from `AGL × gimbal FOV`
  swept along the ground track. The sensor model is stored in the mission so `swath(alt)` is explicit.
- **Airspeed source is load-bearing**: assert on `[flt]` IAS; if a check falls back to `GS`, the result must
  record the wind caveat — never silently conflate the two.

Derived per point/leg: `range(P,t)`, `bearing(P,t)`, cross-track `XTE(leg,t)`, along-track `ATE`, unwrapped
orbit angle `Θ(C,t)`, turn rate `ψ̇`, closure rate `d range/dt`.

---

## 2. Shared skeleton (every mission, every type)

Generalises today's `takeoff / waypoints / land / success / abort` into a phased envelope with a typed
**task** slot. Backward-compatible: a bare `waypoints` list is sugar for one `transit` + one `p2p` task, so
the 10 existing missions migrate mechanically.

```yaml
id: eddh-platzrunde
name: "PPL Nav — Cessna 172, EDDH circuit"
aircraft: c172                          # plugin dir key: aircraft/models/<name>

envelope:                               # global, enforced ∀t (§5); never-violated is part of PASS
  v_min_ms: 14        v_max_ms: 62      # IAS band; default k_stall·Vs .. Vne per aircraft
  bank_max_deg: 45    pitch_max_deg: 30
  alt_floor_agl_m: 20 alt_ceil_agl_m: 900
  on_nan_abort: true  divergence_guards: true

takeoff: { airport: EDDH, runway: "05", rotate_agl_m: 50 }

transit:                                # INGRESS to the task area (a p2p leg set)
  capture_radius_m: 150
  legs:
    - { lat: 53.6240, lon: 9.9754, alt_agl_m: 120, speed_tgt_ms: 20, speed_tol_ms: 4, alt_tol_m: 30 }

tasks:                                  # THE payload — 1..n typed tasks (§3), ordered or concurrent
  - type: overwatch
    id: tower-stare
    target: { lat: 53.6300, lon: 9.9900 }
    standoff_r_m: 600
    alt_agl_m: 300  alt_tol_m: 40
    dwell_s: 120    min_laps: 3   orbit_dir: cw
    verify: { annulus_pct: 20, gimbal_los_tol_deg: 25 }

egress:                                 # from task area to landing (same shape as transit)
  capture_radius_m: 150
  legs: []

land: { airport: EDDH, runway: "05", touchdown_tol_m: 150, touchdown_gs_max_ms: 30 }

timing: { t_max_total_s: 600, tot_s: null, tot_window_s: null }
abort:  { timeout_s: 600, on_nan: true, on_envelope_violation: true }
```

Every leg / task may carry phase-local `speed_tgt_ms/speed_tol_ms` and `alt_agl_m/alt_tol_m`, so verification
is **phase-local, not endpoint-only** — this is what catches over-climb and stall-departure that endpoint
checks miss.

---

## 3. Geometry primitives & predicate library

Tasks compose these; the runner evaluates the named predicates. Nothing per-type is hard-coded in the runner.

**Geometry:** `Point{lat,lon,alt_agl?}` · `Leg[Point…]` · `Corridor{leg,half_width_m}` ·
`Polygon[Point…]` · `Orbit{center,radius_m,dir}` · `Racetrack{p1,p2|center+axis_hdg+length,radius_m}` ·
`Sector{center,radius_m,bearing_lo,bearing_hi}` · `MovingTarget{p0,(track_hdg,speed_ms)|timed[Point],t0}`.

**Predicates** (parameterised, referenced by `verify`):

| Predicate | Definition (measurable) |
|---|---|
| `CAPTURE(P,r)` | ∃t: `range(P,t) ≤ r` |
| `CAPTURE_BY(P,r,t_max)` | ∃t≤t_max: `range(P,t) ≤ r` |
| `CORRIDOR(leg,W)` | ∀t∈phase: `|XTE(leg,t)| ≤ W` |
| `ALT_BAND(a,tol)` | ∀t∈phase: `|h_agl−a| ≤ tol` |
| `SPEED_BAND(v,tol)` | ∀t∈phase: `|IAS−v| ≤ tol` (IAS from `[flt]`; else GS + caveat) |
| `DWELL(C,r_min,r_max,T)` | ∃ contiguous [ta,tb], tb−ta≥T, ∀t: `r_min ≤ range(C,t) ≤ r_max` |
| `LAPS(C,N)` | unwrapped `|ΔΘ(C)| ≥ 2πN` within an in-band window |
| `COVERAGE(poly,swath,X)` | grid poly; `covered = cells within swath/2 of track`; `|covered|/|poly| ≥ X` |
| `TIME_WINDOW(evt,[lo,hi])` | `lo ≤ t(evt) ≤ hi` |
| `LOS(P,tol)` | ∀t∈dwell: `angle(bearing(P,t), track±90) ≤ tol` (target inside gimbal envelope) |
| `NO_GAP(pred,g_max)` | every interval where `pred` is false lasts ≤ `g_max` |
| `RING_TRACK(frac)` | flew the dense directed-ring corridor in sequence ≥ `frac` (§4) |

**Anti-cheese trilogy** for any dwell/orbit/patrol: **annulus + contiguity + laps**. A bare `range ≤ r`
scores a fly-through as an orbit; `DWELL` (annulus, held contiguously) + `LAPS≥n` forces a real closed orbit.

---

## 4. Ring track — the geometric corridor scorer (exists today)

`ringtrack.py` builds a dense **directed** ring track from iNav's own nav-math and scores the flown path:
climb corridor at `nav_fw_climb_angle`, straight enroute legs joined at the coordinated turn radius
`R=V²/(g·tan(bank))`, approach glideslope at the landing runway — ≥100 rings, radius default 150 m. A ring is
a hoop whose plane normal is the flight tangent; "passed" = **forward pierce** (backward doesn't count) with
in-plane offset (cross-track + vertical) `< radius`. `score()` returns `(threaded, inorder)`; PASS needs the
**in-sequence** fraction ≥ `ring_pass_frac`. This is the `RING_TRACK` predicate and is the backbone of the
`p2p`, `transit`, `egress`, and approach geometry. It expresses 3D corridor adherence + directionality +
in-order completion + altitude profile — but **not** loiter, airspeed, attitude, or touchdown quality (those
need the time/`[flt]` predicates above).

---

## 5. Task taxonomy

Each row: the `verify` predicates that define success, and how the task **compiles to iNav commands**.
`†` marks compilation that needs the CC's `uploadMission` extended to emit non-WAYPOINT actions (§6) — the
iNav wire already supports them, the CC just doesn't emit them yet.

| Type | Purpose | Verify (core) | Compiles to |
|---|---|---|---|
| `p2p` | A→B via waypoints | `RING_TRACK` + `CAPTURE(wp_i,r)` in order + per-leg `ALT_BAND`/`SPEED_BAND` | WAYPOINT legs |
| `overwatch` | orbit a fixed target at standoff | `DWELL(annulus,T)` + `LAPS≥n` + `ALT_BAND` + `LOS` | WAYPOINT at orbit entry + `JUMP` loop †, or POSHOLD via RC |
| `recon_area` | sweep an area for coverage | `COVERAGE(poly,swath,X%)` + lane `spacing≤S` + `ALT_BAND` | boustrophedon WAYPOINT lanes |
| `intercept` | reach static/moving target in radius+time | `CAPTURE_BY(Tgt(t),R,t_max)` + closing-rate + target-on-plan | WAYPOINT to intercept pt (moving: staged) |
| `cap_patrol` | hold racetrack/orbit for a duration | `LAPS(N)` or on-station `DWELL(T)` + `CORRIDOR(track,W)` + bands | racetrack WAYPOINTs + `JUMP` loop † |
| `area_search` | grid/expanding-square/sector | `COVERAGE(X%)` + pattern-track match (Fréchet ≤ W) | generated pattern WAYPOINTs |
| `escort` | station-keep on a moving friendly | frac of samples in `[r_min,r_max]`+rel-sector ≥ X + `NO_GAP` | staged WAYPOINTs tracking the entity |
| `relay_station` | continuous presence over a point | `DWELL(0,r,T)` + `NO_GAP(in-radius,g_max)` | `HOLD_TIME(p1=T)` † or `JUMP` orbit † |
| `perimeter` | patrol a closed boundary | `CORRIDOR(boundary,W)` + `COVERAGE` per lap + `LAPS(N)` | boundary WAYPOINTs + `JUMP` † |
| `strike_pass` | timed run over a point on an axis | `CAPTURE(rel_pt,R)` + `TIME_WINDOW(TOT)` + run-in `χ±tol` (+dive `VS`/θ) | run-in WAYPOINT + `SET_HEAD` † + egress |
| `tf_ingress` | low-level terrain-following ingress | `CORRIDOR(W)` + **tight** `ALT_BAND(agl)` | WAYPOINT legs, low `alt_rel` |
| `sar_ladder` | SAR expanding-square/ladder | pattern-track match + `COVERAGE(X%)` + leg-growth monotone | generated pattern WAYPOINTs |

`overwatch`, `relay_station`, `cap_patrol`, `perimeter` are the loiter/orbit family — they need **time-based**
scoring (dwell, laps) that ring geometry cannot express, and they need the `HOLD_TIME`/`JUMP` compilation.
Everything else is expressible with today's WAYPOINT-only path + geometry predicates + `[flt]` airspeed.

---

## 6. iNav constraints that bound the format (hard, from the firmware)

- **≤ 15 commandable waypoints** (`NAV_MAX_WAYPOINTS`). The *ring track* can be arbitrarily dense (host-side
  scoring), but the *route iNav flies* is ≤15 WPs. `JUMP` loops reuse WPs, so orbits/patrols cost few slots.
- **Waypoint altitude datum = home-relative** (`GEO_ALT_RELATIVE`): `alt_rel = ground(wp)+alt_agl−takeoff_elev`.
  iNav never sees AGL or MSL. The DEM is entirely host-side (CC).
- **Safehome ≤ 650 m from the arming point** (`safehome_max_distance` max 65000 cm). The autoland runway
  threshold must be within 650 m of takeoff → a circuit **lands on its departure runway / near-field**.
- **No airspeed over MSP in SITL** (`MSP2_INAV_AIR_SPEED`=0, no pitot) → airspeed predicates read the `[flt]`
  log. **Crab**: use `groundCourse` for track, `yaw` for pointing.
- **F-16 at `FB_TIME_SCALE=1` only** — its 1 kHz FLCS trips `SYSTEM_OVERLOADED` (arming block) above 1× under
  load; c172/sgs233 tolerate higher.
- **Per-aircraft nav math** (`aircraft/models/*/inav.diff`) sets climb/dive/bank/ref-airspeed/loiter-radius →
  bounds ring spacing and how tight a leg join / orbit can be (c172 loiter 140 m, f16 250 m, sgs233 tight).
- **The CC currently emits only `NAV_WP_ACTION_WAYPOINT`.** To compile the loiter/orbit/heading tasks, extend
  `uploadMission` to emit `HOLD_TIME`(p1=seconds), `JUMP`(p1=target,p2=count), `SET_POI`, `SET_HEAD`(p1=cdeg),
  `LAND`, `RTH` — the wire (`MSP_SET_WP` with nonzero action/p1/p2/p3) already supports them.

---

## 7. What to build (implementation plan)

1. **`mission.py` / `mission.ts` resolver**: extend to resolve the phased skeleton + typed tasks (each task's
   geometry AGL→home-relative exactly like waypoints today). Keep `p2p`-sugar back-compat.
2. **Task→iNav compiler**: each task emits its WAYPOINT/HOLD_TIME/JUMP/SET_POI/SET_HEAD/LAND sequence within
   the ≤15-WP budget; extend `CC.uploadMission` to encode non-WAYPOINT actions + p1/p2/p3.
3. **Predicate evaluator**: one module implementing §3's predicates over `S` (stream) + the `[flt]` log
   (airspeed/attitude/NaN). Every task's `verify` is a predicate composition, checked continuously; the
   run **aborts on the first violation** (fail-fast) and reports the failing phase/task/predicate + value.
4. **`ringtrack`**: already implements `RING_TRACK`; add per-leg altitude/geometry it already computes to the
   report; keep it as the corridor backbone.
5. **Moving/entity targets**: a scripted target path (`MovingTarget`) evaluated in lockstep with the flown
   track for `intercept`/`escort`; assert the target stayed on its own plan so an entity glitch can't
   false-pass.
6. **Result**: structured per-phase / per-task / per-predicate pass-fail with measured values, plus the
   envelope-violation log — the whole picture, not a single boolean.

Current `missions/*.json` are `p2p` special cases; the generalisation is strictly additive.

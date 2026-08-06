# Client and server — a role, not a process

> Owner, 2026-08-05: *„ich denke wir brauchen Clients wasm/native/gym und einen Welt/Mod-Server. Gym
> könnte Teil des Servers sein und ohne Latenz auskommen."* · *„Gym und Test-Runner sind ja quasi
> dasselbe und wären serverseitig richtig."*

Spec-first. Nothing built.

## Spec

### 0. What ARMA and DayZ decided differently

| | Model | Cost |
|---|---|---|
| **ARMA 3** | distributed *locality* — each object simulated by one machine, ownership migrates | desync and cheating: whoever simulates an object decides its truth |
| **DayZ** (Enfusion) | **server-authoritative** — server holds loot, AI, positions; client sends input and predicts | latency, and it is the one they moved *to* |

Mods run on both sides in both engines, signed, with the server publishing the required list.

**The lesson is `CLAUDE.md` principle 3 spoken over a wire:** whoever simulates knows everything; whoever
participates knows only what it perceives. ARMA's distributed variant is the one its authors abandoned.

### 1. The prize: anti-cheat stops being a discipline

Today the boundary is a compiler guarantee — `FBUnitRegistry` reachable by six files, `FBFdm`'s private
ctor, `FBSystemHealth`'s single friend. Strong, but it holds because the code is arranged so.

**Draw the client/server line and it becomes a transport property: a client cannot see what the server
did not send.** That is the strongest form this tree has ever had available, and it costs nothing extra
if the line is drawn before the renderer grows against it.

### 2. Server is a role

| Deployment | Server | Latency |
|---|---|---|
| **`fb-gym` / a harness** | **is** the server, no viewer attached | none |
| single player | in the same process | none |
| watching, later multiplayer | remote | real |

One code path, three arrangements — the same thing Unreal does, and the reason single player costs
nothing there.

**`fb-gym` and the test runner are the same object.** A harness is a server that produces a verdict
instead of a picture; the gym is a server that produces telemetry instead of a picture. Both hold world
truth, neither has a viewer. This also explains a rule that until now had to be stated separately: the
gym may never reach a model ([`mods.md`](mods.md) §2.1) because **it is the truth, it does not query it.**

### 3. The tension with principle 2, and its resolution

`CLAUDE.md` principle 2 says *„JSBSim läuft IM Client … ein Prozess, ein Adressraum."* Server authority
appears to move simulation out of the client.

It does not, because *server* here is a role. In single player the server is in-process and the property
principle 2 protects — no telemetry boundary between physics and picture — is untouched. What changes is
that the boundary **exists as a type** even when both sides live in one address space, so it can later be
cut without a rewrite.

### 4. Data delivery

> Owner: *„eine Idee ist auch, dass der Tile-Server zum Outshine-Server wird und alle Daten liefert
> inclusive den `mods/`."*

The tile loader is already async, cached and fronted by nginx; mods are static files and simpler than
tiles. Two gains: mods stream instead of living in one 13.5 MB `gpu.data` (four titles are currently one
download for four titles), and a mod becomes replaceable without rebuilding the WASM binary.

Two conditions:

- **The gym never needs a server for data.** It is the test runner: filesystem, offline, deterministic.
  Two paths to the same mod data — filesystem for gym and native, HTTP for the browser — exactly as
  tiles work today.
- **A mission is complete before spawn.** Non-blocking to load, blocking before the first tick. Otherwise
  arrival order becomes a coupling to speed, which principle 5 calls a bug.

Consequence worth noting: this makes `fb-sim` nearly redundant — principle 4's two containers could
become one. That is a convenience question (live-mounting `sim/web` while developing), not an
architectural one, and it is not decided here.

### 4.1 The world runs 24/7, and the client attaches

> Owner, 2026-08-05: *„was ich aber gut fände, auch im Single Player, dass die Welt immer 24/7 läuft und
> man mit seinem Client jederzeit connecten kann."*

Same server shape as §2 — the gym is a server without a viewer, and a persistent world is that server
without an end. **This is also the 2026 deliverable**: you do not launch a mission, you tune in. A world
that runs whether or not anyone is watching is literally a stream.

**Cost when nobody watches is already answered** by the observation rule
([`persistent-world.md`](persistent-world.md) §4): 24/7 does not mean ten thousand entities ticking at
100 Hz forever. Coarse world state advances; detail materialises on observation. That is Ocean's job.

**Determinism is the real hazard, and it has one rule:**

> **The world advances in ticks, never in seconds. Wall-clock decides *when* a tick happens, never
> *what* it computes.**

Otherwise the pace produces the result, which principle 5 calls a bug outright. With it, a world that has
been running for three weeks is still fully reconstructible from `(seed, tick count, transaction log)` —
exactly the persistence model already chosen.

Connecting at any time is then the view type §1 needs: the client attaches and receives the current view,
never the world.

### 5. What must not be built shut

Nothing needs building now. But nothing may be built that assumes:

- the renderer can read world state directly rather than a delivered view
- a mission is a filesystem path in every client
- the simulation and the picture share a clock
- the world stops when no client is attached, or advances in wall-clock seconds rather than ticks

## State

Nothing of the client/server LINE is built. `fb-gym`, `gpu_native` and wasm all link one library and
read the filesystem.

§4.1's rule, however, has now been tested, because the browser already streams one thing from a server
— the ground — and it broke exactly the way this section predicts. Its `/elev` cache held ONE global
value with no key, so the answer a client got was the FETCH ARRIVAL ORDER: `mods/f22 c01m05` briefed
both strike aircraft's targets with the flight leader's spawn ground (777.06 m for 510.93 / 442.26 m),
released 1.6 s late and dropped 344 m short of the run `fb-gym` flew from the same file. The pace
produced the result. The repair is §4's own condition applied to ground truth — the unit of transport
is the TILE (4.8 km, twenty seconds of flight) and not the point, so the datum is there before the tick
that needs it ([`world/terrain.md`](world/terrain.md) §3.3). Same file, both clients, now: `c01m05`
456 event lines with 4 differing (a 1e-12-degree numerical zero), `c01m07` 670 lines over 415.5 mission
seconds with none.

## Gaps

- **No view type exists.** §1's prize needs one — what a client receives, as opposed to what the world
  holds. Without it the line is a diagram.
- **Prediction is unaddressed.** A remote client that cannot predict is unplayable, and prediction is
  where DayZ spends its complexity. Out of scope until multiplayer is, but it is what makes §2's
  „one code path" claim optimistic.

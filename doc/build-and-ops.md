# Build, gates and operation

Recipes live in the Makefile, not in agents' heads. This file says which targets exist, what counts as
a proof, and what is special about this machine.

## Spec

What a proof is, and what has to hold before a change counts as verified.

| Gate | Must hold |
|---|---|
| Warnings = errors | all targets clean under `-Wall -Wextra -Wpedantic` |
| `nm` gate | `build/fb-gym` contains zero Dawn/WebGPU symbols |
| Harnesses | all seven test binaries rc=0 |
| Frame proof | build-effective changes need a rendered frame **or** a numerical measurement |
| Regression | telemetry of all `sim/missions/*.fbm` byte-compared; every deviation justified individually, every verdict change separately |
| Determinism | `--threads 1/2/4` × repetitions produce a single signature |
| WASM | `make -C sim wasm` builds and the app boots in the browser — the one client used daily |
| Model deltas | `make -C sim verify-models` green: every copy deviates from the pinned upstream by EXACTLY the entries in `sim/assets/MODEL-DELTAS.md` |
| vendor read-only | `sim/vendor/jsbsim` is never modified; only the COPY changes, and then as a named delta |

Two rules about how measuring is done at all: accepted properties of the vanilla JSBSim F-16 are the
truth and not defects (`CLAUDE.md` principle 5), and measurements run through the **mission control
loop** (telemetry), never through single observations.

Recipes live in the Makefile, not in agents' heads.

## State

All gates exist and are runnable today; the delta gate is the newest and was proven in four failure
directions (see the detail below and [`journal.md`](journal.md)).

## Gaps

| Thing | Where it is tracked |
|---|---|
| The mission control loop effectively runs on `const`/`swiss` elevation because `payerne-full` crashes under `--elev tiles` | [`clients/clients.md`](clients/clients.md) |
| The delta entry format is untested for a multi-file delta or a new file (diff against `/dev/null`) | [`aircraft/stores.md`](modules/stores.md) |

## Knowledge

Targets, gates and host facts in full.

### Make targets

Every project carries its own Makefile.

#### `sim/`

| Target | Result |
|---|---|
| `core-lib` | `build/libfbcore.a` — the simulator as a library |
| `gym` | `build/fb-gym` — headless, GPU-free |
| `native` | `build/gpu_native` — reference renderer / frame oracle |
| `wasm` | `web/gpu.js` + `web/gpu.wasm` — **depends on the `worker` target and always builds both** |
| `worker` | `web/fbtileworker.js` + `.wasm` — callable on its own |
| `image`, `up` | container build and start |
| `test-monitor` | `fb-test-hard-landing`, `fb-test-loc-departure`, `fb-test-nan-divergence` |
| `test-fdm` | `fb-test-two-fdm` — two coexisting FDM instances |
| `test-corner` | `fb-test-corner-speed` — measures the model's corner speed/g/turn rate |
| `test-missile` | `fb-test-missile-airframe` |
| `test-gun` | `fb-test-gun` — dispersion, time of flight, funnel geometry, lead solution, ammunition consumption |
| `test-weather` | `fb-test-weather` — the FBWX mirror against the committed `/wx` fixture |
| `test-mig29` | `fb-test-mig29-envelope` — the MiG-29 deck against every anchor of its spec §8 |
| `test-air` | `fb-test-air-envelope` **and its verdict**: builds, flies the ten catalogue decks, then runs `tools/fb_test.py` against `sim/test/modules/air/envelope.json` |
| `verify-models` | the delta check: `assets/aircraft` against the pinned submodule + `assets/MODEL-DELTAS.md` |
| `verify-layers` | `sim/src` + `sim/test` are a stack; every `#include` points down it |
| `verify-guards` | the compile-time guarantees, proven by trying to break them |
| `verify-trees` | `doc/`, `sim/src/` and `sim/test/` carry the same directory tree ([`testing.md`](testing.md) §3.1) |
| `verify-tests` | every declaration under `sim/test/` against its band; the harness binaries must be built |

If the tile worker is missing, the WASM app hangs silently at startup (404 in the worker). Hence the
fixed dependency instead of two separately memorised targets.

#### `tiles/`

`build` | `image` | `run`

### Gates

A change counts as verified only once it passes these checks.

| Gate | Check |
|---|---|
| **Warnings = errors** | all targets clean under `-Wall -Wextra -Wpedantic` |
| **`nm` gate** | `build/fb-gym` contains 0 Dawn/WebGPU symbols |
| **Harnesses** | the nine self-judging test binaries rc=0. `air-envelope` no longer judges itself — its verdict is `make -C sim test-air` |
| **Declarations** | `make -C sim verify-tests`: every `tier: A` declaration under `sim/test/` inside its band ([`testing.md`](testing.md)) |
| **Tree congruence** | `make -C sim verify-trees`, and the orphan count it prints is read |
| **Frame proof** | build-effective changes need a rendered frame **or** a numerical measurement |
| **Regression** | telemetry of all `sim/missions/*.fbm` byte-compared; every deviation justified individually, every verdict change separately |
| **Determinism** | `--threads 1/2/4` × repetitions produce a single signature |
| **WASM** | `make -C sim wasm` builds and the app boots in the browser — the only client that is used daily; a broken boot is more expensive than any other defect |
| **Model deltas** | `make -C sim verify-models` green: every copy under `sim/assets/aircraft` deviates from the pinned upstream by EXACTLY the entries in `sim/assets/MODEL-DELTAS.md` |
| **Layer + count gate** | `make -C sim verify-layers` green, and the numbers it PRINTS are read: 6 registry readers, 1 antenna-cue poster, 1 simulation-loop driver |
| **Guarantee gate** | `make -C sim verify-guards`: eight two-line translation units against the real headers, six of which must FAIL to compile ([`conventions.md`](conventions.md), "A rule nobody can forget to obey") |
| **vendor read-only** | `sim/vendor/jsbsim` is never modified — engine as well as models. At most the COPY is changed, and then as a named delta entry |

#### The delta gate in detail

`verify-models` computes, per file, the canonical unified diff (`difflib`, 3 lines of context) between
upstream and copy and compares it character by character with the diff block of the matching entry. It
fails in **four** directions, all measured:

| Case | Message |
|---|---|
| unexplained deviation in a copy | `UNEXPLAINED difference from upstream` + the block that is missing |
| declared delta that the copy does not (any longer) carry | `declares a delta that is NOT present in the copy` |
| entry present, but the diff does not match | `the declared delta does not match the actual difference` |
| model under `assets/aircraft` that the provenance table does not name | `is not declared in ... ('## Herkunft')` |

A delta block is **generated, not typed** — `python3 tools/verify_models.py --emit` prints the finished
entry skeleton. In a unified diff whitespace carries meaning (context lines carry their leading space),
so copying it by hand is a source of errors without benefit.

The target is deliberately NOT a prerequisite of the build targets: it only has something to say when a
file under `assets/aircraft` or in the submodule changes, and putting a Python interpreter on the
critical path of every C++ compile for that would be the wrong trade.

### The committed measurement tools

Not build targets, and deliberately so — they are analysis, not product.

| Tool | What it measures |
|---|---|
| `sim/tools/fb_duel_report.py` | both sides' `eng_*` debriefing plus the EMCON timeline out of one duel run |
| `sim/tools/fb_tournament.py` | the pilot-variant tournament over `variants-*.txt` |
| `sim/tools/capture_cloud_proofs.sh` | the **cloud frame-proof set** and the recipe that makes it reproducible: every shot holds the camera still for 180 frames and keeps only the last, so the tile streamer is at `pending=0` and a second run writes the same bytes (measured: 12/12 sha256-identical). Needs `fb-tiles` on :8081 |
| `sim/tools/fb_campaign_verify.py` | the **campaign layer's instrument** ([`missions/campaign.md`](missions/campaign.md) §5). `fingerprint DIR --exit N` = one run's SHA-256 over all `telemetry*.csv` + the normalised `events.log` + the exit code (normalisation = `wallS`/`speedup` and the absolute path in `telemetry=`, nothing else); `campaign DIR` = the campaign fingerprint over that tree; `determinism FBC` drives 3 × `--threads 1/2/4` and expects ONE hash; `replay FBC --ref DIR` re-runs every step STANDALONE from the previous step's state file and compares per-mission fingerprints. Exit 0 = every comparison held. **It never guesses the environment**: the elevation source is read from the reference tree's `campaign-summary.txt`, `--elev` is an override, and a tree without that record is refused — a fingerprint compares two runs over the SAME ground or nothing |
| `sim/tools/fb_bfm_sweep.py` | the **16-approach BFM/gun sweep** the close-combat roll law is measured against (`pilot.md` §5.7.2/§5.7.3): 8 pursuer geometries × straight/turning defender, printing kills, departures and the pooled roll-rate statistics. Read the departure count and the rate statistics, not the kill count — a ~1 m spawn perturbation flips a cell |

### Measurement discipline

- Accepted properties of the vanilla JSBSim F-16 are the truth, not defects (CLAUDE.md, principle 5).
- Measurements run through the **mission control loop** (telemetry), not through single observations.
- Target GPU capabilities: `doc/webgl-webgpu-report.txt`.

### The mission control loop

The way of working for everything that concerns pilot AI or system behaviour:

```
define mission  →  simulate headless  →  analyse telemetry mechanically  →  correction  →  loop
```

Format `.fbm`, line-based, zero-dependency — [`doc/missions/INDEX.md`](missions/INDEX.md). Parsed by
`core/FBMissionFile.h` (a pure text→`FBMission` function, no file I/O — that is the app's job).

A SEQUENCE of missions is a campaign, `sim/campaigns/*.fbc`, run with `fb-gym --campaign FILE --out DIR`
([`missions/campaign.md`](missions/campaign.md)): the same loop N times, with destroyed units, destroyed
ground targets and expended stores carried between the steps through a text state file. It writes
`DIR/NN-<mission>/` per step (the ordinary per-run files plus `campaign-state.txt`) and
`campaign.log` + `campaign-summary.txt` at the root; its exit code is the worst step's. Any single step
is an ordinary run again with `fb-gym --mission FILE --state DIR/NN-…/campaign-state.txt`, which is what
makes a campaign debuggable and is measured every round.

Termination → exit codes:

| Result | Exit |
|---|---|
| SUCCESS | 0 |
| FAIL | 1 |
| CRASH | 2 |
| TIMEOUT | 3 |

**The exit code is not always the verdict.** A fight has no waypoint objective; such missions end in a
timeout on purpose, and the verdict is in the events and the telemetry. Where this applies, the header
comment of the respective `.fbm` file says so — and it is binding, because it carries the reading rule.

Output per run in `--out/`:

| File | Content |
|---|---|
| `telemetry.csv` | 10 Hz, fixed column count. New sources are **always appended at the end**, so that no measured column loses its position. |
| `telemetry_<callsign>.csv` | per further unit |
| `events.log` | `t=SEC EVENT key=val`, greppable |

### Host and operation (this machine)

No hidden agent memory — all operational knowledge is here.

| Item | State |
|---|---|
| emsdk | `~/Git/emsdk` |
| `nproc` shim | `~/.local/bin` (macOS has no nproc) |
| Containers | podman VM first (`podman machine start`), then `tiles/up.sh` (:8081) and `sim/up.sh` (:8080) |
| Live mount | fb-sim mounts `sim/web` live — `make wasm` takes effect on refresh |
| WASM artefacts | gitignored |
| Native builds | need `sim/vendor/.compat-headers` (gitignored, host-local) |
| Git | commit mail is the GitHub noreply alias; push via SSH insteadOf |
| `timeout(1)` | **does not exist on macOS** — do not build it into scripts |

The mission files are copied from `sim/missions/` to `sim/web/missions/` during the WASM build; the copy
is gitignored. A hand-kept second copy would be a source of errors, and once was.

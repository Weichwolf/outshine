# Mission data — the avionics bus: validity, commands, brief

**Source of this file:** the former `doc/mission-format.md` (split in the Phase-3 mirror rebuild), section "Der
Avionik-Bus — Gültigkeit, Kommandos, Brief". Translated 1:1 from the German original; no revision of
content.

This file is the **mission-author's view** of the bus: which validity states a run can produce, how a
command is accepted or rejected, and what a `brief_*` line does. The implementation contract of the
bus itself is in [`../core.md`](../core.md); the real jet's command vocabulary is in
[`../modules/f16/controls-commands.md`](../modules/f16/controls-commands.md).

---

## Spec

### Blocks and three-state validity

**The shared state is a set of typed BLOCKS**, no longer a flat field list (`core/FBState.h` +
`core/FBAvionicsBlocks.h`). Every block has EXACTLY ONE writer (the source system) and a head
`{StampS, Status}` (`core/FBBlockStatus.h`) with **three** states — the semantics of a multiplex-bus
jet, not its addresses:

| Status | Meaning | who produces it today |
|---|---|---|
| `invalid` (0) | the numbers mean nothing: never written, or the source system is off/failed | `set radalt off` (CARA without power), radar/datalink switched off, nav without steerpoint |
| `valid` (1) | updated by its own writer at time `StampS` | normal operation |
| `held` (2) | DELIBERATELY frozen: last good values, last timestamp, no new computation | radar picture between two sweeps, datalink between two net cycles, BFM estimate beyond its extrapolation window, CRUS computed fields (TTG) with the gear extended ([`../modules/f16/controls-commands.md`](../modules/f16/controls-commands.md)) |

`held` is not a nicety but documented behaviour: the real jet FREEZES several cruise computation
fields with the gear extended instead of invalidating them. Consumers distinguish it: the HUD keeps
drawing a held value (it is valid, only old) and replaces an invalid one with dashes (`R----`,
`B---.-`, `---:--`); `systems/FBWarningSystem` reports a warning whose source is invalid as
**inhibited** instead of as "no warning"; `pilot/FBPilot` does not act on an altitude it cannot
measure (no gear retraction, no flare, no BFM ground pull without a valid radar altimeter).

Every block status appears in `telemetry.csv` as its own column (`blk_platform`, `blk_env`,
`blk_airdata`, `blk_radalt`, `blk_nav`, `blk_cruise`, `blk_firecontrol`, `blk_ufc`, `blk_stores`,
`blk_airframe`, `blk_warn`, `blk_radar`, `blk_datalink`, `blk_bfm` — and, at the very end of the line
instead of inside that group, `blk_rwr` and `blk_cmds`: their blocks arrived when the group already
stood in every measured telemetry.csv, and inserting them would have shifted every column to the
right; values 0/1/2 as above) — plus `warn_active` and `warn_inhibited` as bitmasks (`1` = ALOW,
`2` = BINGO, `4` = gear unsafe).

### Commands: the ONLY way from the pilot to a box

A command is `{target, proposed value}`, the acknowledgement `{outcome, reason}` — the documented DED
pattern propose → commit/reject. Two latency classes: **HOTAS** (switch/button, 0.5 s, usable while
manoeuvring) and **DED** (field entry, 4 s, head down) — a DED entry is rejected above 1.5 g, so that
no AI types in steerpoints during a turning fight.

Outcomes: `accepted`, `clamped` (taken, but a documented system ceiling rules — e.g. BNGO above
6,070 lb), `inhibited` (taken, effect blocked — e.g. ALOW without a radar altimeter), `rejected`.

Rejection reasons are the catalogue from
[`../modules/f16/controls-commands.md`](../modules/f16/controls-commands.md) §6 plus two OWN model
decisions: `out_of_range` (the sources document NO range check — FlightBox rejects and says so instead
of silently clamping), `channel_busy` (hand/head are already occupied) and `depleted` (the magazine
behind the box is empty — the only rejection a defensive set produces in the middle of being shot at).

The command stream is observable: `events.log` carries `cmd CMD_ISSUE` / `CMD_ACK` / `CMD_REJECT`
(with target, value, class, outcome, reason, measured latency), `telemetry.csv` the nine columns
`cmd_issued`, `cmd_accepted`, `cmd_rejected`, `cmd_clamped`, `cmd_inhibited`, `cmd_pending`,
`cmd_last`, `cmd_last_outcome`, `cmd_last_reason`.

### The BRIEF (`brief_*` lines): what the pilot enters IN FLIGHT himself

Normal `set` lines set the aircraft up in the spawn window (before the first pilot tick, see
[`syntax.md`](syntax.md)). A `brief_*` line sets up NOTHING — it tells the pilot what he should enter
over the command path after take-off, in the latency class of his control action, with the risk of
being rejected. Without a `brief_*` line the pilot operates nothing at all (he does not type in numbers
nobody gave him).

| Line | Class | Effect |
|---|---|---|
| `set brief_alow_ft <ft>` | DED | enter the ALOW floor (0…50,000 ft, otherwise `out_of_range`) |
| `set brief_bingo_lbs <lb>` | DED | enter the BNGO threshold (0…20,000 lb; above 6,070 lb → `clamped`) |
| `set brief_master_arm arm\|sim` | HOTAS | set master arm |
| `set brief_weapon gun\|aim9\|aim120` | HOTAS | weapon selection — today `rejected/not_implemented` |
| `set brief_chaff_s <t>` | HOTAS | throw countermeasures (CMS forward, selected program); repeatable |
| `set brief_release_s <t>` | HOTAS | when the pilot pickles (sim seconds); repeatable — see [`weapons.md`](weapons.md) |
| `set radalt on\|off` | (spawn) | CARA power; `off` makes the radar-altitude block `invalid` for the whole run |

`mods/f16/src/missions/cmd-avionics.fbm` runs exactly these cases in one run (acceptance, clamping, effect
inhibition, "not implemented", channel busy, manoeuvre lock) and additionally switches the radar
altimeter off — the reference run for the command stream AND the validity states.

## State

| Item | State |
|---|---|
| Block bus | built; sixteen blocks, three-state validity, one writer each, all statuses in telemetry |
| Command bus | built; two latency classes, four outcomes, the rejection catalogue plus three own reasons |
| Brief lines | built for ALOW, BINGO, master arm, chaff, release; `brief_weapon` is accepted by the parser and answered `rejected/not_implemented` |
| Reference run | `missions/cmd-avionics.fbm` covers all outcomes and both validity anomalies |

## Gaps

| Gap | Detail |
|---|---|
| `brief_weapon` is not implemented | the line exists and is deliberately answered `rejected/not_implemented` rather than silently ignored |
| `blk_rwr`/`blk_cmds` sit outside their group | column order is append-only, so the two later blocks stand at the end of the line instead of with the other `blk_*` |
| Only two latency classes | HOTAS 0.5 s and DED 4 s; the source distinguishes more control actions than the model does |
| The manoeuvre lock is a single threshold | DED entries are rejected above 1.5 g; there is no gradation |

## Knowledge

- **Why three validity states and not two.** `held` is documented behaviour of the real jet, which
  freezes several cruise fields with the gear extended instead of invalidating them. With only two
  states, a held value would look either fresh (wrong: it is old) or meaningless (wrong: it is the
  last good value). Every consumer therefore has to answer both questions separately — and the HUD,
  the warning system and the pilot each answer them differently.
- **Why the pilot may only reach a box through the bus.** Latency, channel occupancy and the manoeuvre
  lock are properties of the human operating the jet; a direct call would remove all three. The bus is
  also the only place where a rejection can exist at all — and a rejection is information the pilot
  must handle.
- **Why `out_of_range` is our own reason.** The sources document no range check for the DED fields.
  Silently clamping would fake a system property; rejecting and saying so keeps the model honest about
  where it goes beyond its source.
- **Why a brief is mission data and not a setup.** A `set` line configures the aircraft before the
  first pilot tick — it cannot be rejected, because no operator was involved. A `brief_*` line is an
  instruction to the pilot, executed later, through the same command path a human uses, and therefore
  subject to latency and rejection.

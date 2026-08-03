# The connected air defence — the net above the single site

**Subject:** `C22`/`C23`/`C24` — **what happens BETWEEN ground positions.** The cue that aims a fire
unit's antenna, the layered belt a mission can declare and a judge can read, the fire-control authority
that decides who shoots, what the defence becomes when the net is taken from it, and the minimum
jamming model that makes that last question measurable.

**Status: BUILT.** The five capabilities of §0 are built and measured; the numbers are in §State and the
eight proof missions are `sim/missions/net-*.fbm`.

**Delimitation, and it is the whole reason this file exists separately.**
[`modules/ground/module.md`](modules/ground/module.md) specifies **one position**: two antennas, an
envelope, a five-state engagement machine, nine catalogue rows. It is deliberately complete about the
position and deliberately silent about everything between positions — its own
[`catalogue.md`](modules/ground/catalogue.md) says so in one line for the `p18` row:

> *"It cues nobody. A `p18` beside an `sa3` does not make the `sa3` smarter … It is a target and a
> warning, not a network."*

That sentence is correct, bounded, and it is the gap this file is the home of. **Nothing here changes a
contract of that file**; where the two meet, the interface is named in §Gaps as a declared dependency.

**Why it is outside the `sim/src/` mirror.** Like [`duels.md`](duels.md) (a PAIRING) and
[`formation.md`](formation.md) (a FLIGHT), a net is not a directory: it cuts through `core/` (one value
type), `sensors/` (the channel), `units/` (what a node publishes), `missions/` (the declaration and the
judge) and `modules/ground/` (the subscriber). Putting it in `sensors.md` would bury the doctrine half;
putting it in `modules/ground/` would put it inside the file it is the sequel to. It is the **third**
deliberate exception to the mirror rule, and [`INDEX.md`](INDEX.md) names it as one.

| Source class | What it is |
|---|---|
| **Requirement sources** | the five campaigns whose subject is the net and not the position: [`campaigns/o1-bekaa-1982.md`](campaigns/o1-bekaa-1982.md), [`campaigns/w3-desert-storm.md`](campaigns/w3-desert-storm.md), [`campaigns/w4-allied-force.md`](campaigns/w4-allied-force.md), [`campaigns/o5-airfield-defence.md`](campaigns/o5-airfield-defence.md), [`campaigns/o3-yom-kippur-1973.md`](campaigns/o3-yom-kippur-1973.md) |
| **FlightBox sources** | [`sensors.md`](sensors.md) §1 (the perception boundary) and §3 (the cooperative net), [`formation.md`](formation.md) §§2/5 (a reported POINT and its correlation gate), [`modules/mig29/datalink-gci.md`](modules/mig29/datalink-gci.md) §2.2 (a cue is typed in, with latency), [`missions/verdict.md`](missions/verdict.md) (what a judge may measure), [`core.md`](core.md) §§6.1/8.3 |

Marking: `[SET]` = a FlightBox setting with its one-sentence reason · `[DERIVED]` = computed from a named
relation · `[MESS]` = measured in this tree · `[TODO]` = open.

---

## Spec

### 0. The five capabilities, and the mission question each one unblocks

Every row names a question that is **unanswerable today** — not unanswered, unanswerable: the mechanism
that would decide it does not exist, so a run produces a number that says nothing.

| # | Capability | The question that stays unanswerable without it | Where it is asked |
|---|---|---|---|
| 1 | **Early warning cues a fire unit** | *Is killing an early-warning radar worth a sortie?* Today the answer is structurally **no**: an EW set cues nobody, so a mission with a `p18` plus an `sa3` is arithmetically identical to one with the `sa3` alone. That is a model artefact wearing a finding's clothes | `w3-01` (the opening move of the whole war as one aircraft), `w4-04`/`w4-05`, `o1-01`, `o2-01` |
| 2 | **Layered belts and sectors, declared and judged** | *Does the altitude that escapes the AAA put you in the SAM?* The 15,000 ft floor, the low ingress and the "under the belt" route are the same one decision seen from three campaigns, and today a run can report a kill and a loss but never **where in the layer cake** either happened | `w4-01`…`w4-10` (the floor), `o1-07` (`under-the-belt`), `o3-06` (`inside-the-umbrella`, and there the belt is OURS), `w3-10` |
| 3 | **Doctrine and control** | *What does a defence look like when the net is taken from it* — and its dual, *how many rounds does an uncoordinated defence spend on one aircraft?* The saturation arithmetic every SEAD package rests on is currently unopposed, because no site knows any other exists | `o1-02`/`o1-10`, `o5-04`, `w3-08`, `w3-09` (Package Q failure mode 3) |
| 4 | **Communications jamming (the minimum)** | *How much of the Israeli result was the jamming?* The present stand-in deletes the controller **at spawn**; the anchor removed him **mid-intercept**. [`campaigns/o1-bekaa-1982.md`](campaigns/o1-bekaa-1982.md) §Knowledge 4 already tabulates the two as different failure modes — blindness against **confident** blindness — and calls the second the more dangerous one | `o1-02`, `o1-10`; `C13` is named there as "the one thing in the set with no substitute at all" |
| 5 | **What the net means to the attacking pilot** | *Can an attacker tell a system from a transmitter?* `w4-05` asks it directly (one real emitter, three decoys, a receiver that measures power and never range). Without a stated boundary the question decays into "did we accidentally give him a threat-ring display" | `w4-05`, `w4-04`, `w3-04` |

### 1. The contract

| Contract | Acceptance / measurement anchor |
|---|---|
| **A net is a SIMULATED CONNECTION** — it can be out of range, be late, be stale, be cut and be jammed | the link is `sensors/FBDatalinkSystem` **as built**, with its 1 Hz cycle, its three-cycle hold, its `Invalid`-on-silence rule and its age. A path that always and immediately tells the truth is the same cheat as a radar that sees through mountains |
| **The net moves an ANTENNA. It never creates a TRACK** | acceptance: a cued site whose own `Radar` id is failed produces **zero** `site TRACK` lines however perfect the cue. The cue enters through the command bus as antenna azimuth + elevation, exactly as `set brief_gci` does on the MiG-29 |
| A report carries a **POINT**, never an identity, never a team, never a type | `FBNetReport` has no id field and no team field — the same absence that makes `FBFlightReport` safe ([`formation.md`](formation.md) §2). A site cannot learn "hostile" from the net, because nothing in the tree can say it (`FBIffReply` has no such value) |
| A report can only carry what the **sender's own sensor measured** | the point is reconstructed from the node's own `FBRadarContact` — anonymous, aged, range-gated — and it travels with **the sender's look age**, not the reception time. That is the `FBMissileUplink` rule verbatim ([`sensors.md`](sensors.md) §1.2) |
| **The registry reader list does not grow** | the link is the EXISTING reader (`sensors/FBDatalinkSystem.cpp`), configured by three setters and one new test **inside its own loop**. No new class walks the registry. Measured by the includer tuple in `tools/verify_layers.py`, not by the gate's printed number (§Gaps, collision 2) |
| Doctrine is **mission data**; capability stays catalogue data | the `net` block declares the node, the link kind, the members, their sectors and their fallback. Nothing in it names a performance figure |
| A belt is **declared geometry**, judged, and invisible to every unit | `zone` lines reach `FBMissionMonitor`'s private copy and the owner's telemetry, and **no module, no pilot and no sensor** ever sees one. Enforced by a new `RESTRICTED` entry — a narrowing, not a widening |
| Killing the net uses the **existing** damage register | `FBSystemId::Datalink` already exists. A node whose terminal is `Failed` stops transmitting → its block goes `Invalid` at every subscriber → the members fall back. **No new type, no new friend, no new id** — the same sentence [`modules/ground/module.md`](modules/ground/module.md) §8 makes for the position |
| Jamming denies the **link and nothing else** | no sensor, no radar, no seeker, no warning receiver changes. §6 lists what is left out, exhaustively |
| A mission that declares no `net`, no `zone` and no jammer behaves **byte-identically** | the conservation rule of `C2`/`C12`: nothing new is written unless something new is declared. All stock `.fbm` telemetry byte-identical, `events.log` identical modulo `wallS`/`speedup` |
| Nothing about the net is random | no die: the walk is the registry order, the cue is geometry, the correlation is the flight's existing gate, jamming is a distance test. One fingerprint over `--threads 1/2/4` × 3 repeats |

---

### 2. The link — the one thing that must be able to fail

**Design decision: the net is `sensors/FBDatalinkSystem`, not a new system.** Everything a net needs
that thing already is: a periodic broadcast of one's own state, a range test, a hold, an age, a
three-state block, and a faction filter that makes a net *one side's* net by construction.

| Design | Verdict |
|---|---|
| **A: three setters + one test on `FBDatalinkSystem`**, all defaulting to today's behaviour exactly | **RECOMMENDED.** The tree's own rule for parameters is "no forced empty derivation for numbers" ([`architecture.md`](architecture.md)). Two of the three are flags and one is a range; a derivation would buy nothing and would have to re-walk the registry — which is exactly the seventh reader nobody wants |
| B: `sensors/FBNetLinkSystem : FBDatalinkSystem` with an `FBState` block of its own | **deferred, with the case for it stated.** It becomes right the day an **aircraft** joins a control net (`C6`): a jet's Datalink block already carries Link-16, and a GCI feed would collide with it. Until then a ground site has no other use for that block, and a second block is a second thing to keep valid |

**What changes on the base class, and each defaults to the built behaviour:**

| Hook | Default = today | Why the net needs it |
|---|---|---|
| `SetCarriesTerminal(kinds)` | `Aircraft` only | `FBDatalinkSystem::Cycle` skips every unit that is not `FBUnitKind::Aircraft` (a store carries no terminal). **A ground site is `Ground`, so the class as built cannot hear another ground unit at all** — the flat collision of §Gaps 3 |
| `SetLinkMode(radio\|wire)` | `radio` | `RadioHorizonM = 1.23·(√h₁+√h₂)` nm is **zero metres for two antennas at ground level**. A buried cable is not subject to a radio horizon, and a real sector net ran on one — §Gaps 1 |
| `SetNetPeriodS(s)` / `SetHoldCycles(n)` | 1.0 s / 3.0 | a ground reporting cycle is not a fighter's PPLI rate, and no source gives one. The mission declares it, `[SET]` per mission, rather than a constant declaring it for every scenario |
| the jam test (§6) | no unit declares a jammer, so the branch is never taken | one distance test inside the loop that already walks the registry |

**The two staleness terms, and neither is invented:**

```
total age of a cue  =  TgtLookAgeS          (the NODE's own contact age when it reported)
                    +  AgeS                  (how long ago the message was heard)
```

The first is the node's radar's own `LookAgeS`; the second is `FBDatalinkTrack::AgeS`, computed the way
it already is. **A cue is therefore always old, and how old is derived from two measurements rather than
from a latency constant.** The receiver publishes both, so a mission can read which half hurt.

**`wire` versus `radio` is a doctrine lever with a real trade**, and it is the mission's choice:

| | `wire` | `radio <rangeM>` |
|---|---|---|
| Reach | unlimited; no horizon test | `min(rangeM, RadioHorizonM)` — for two ground sites, effectively zero unless a mast height is declared |
| Jammable (§6) | **no** | **yes** |
| Dies with | either endpoint (unit destroyed, or `FBSystemId::Datalink` failed) | the same, plus range and jamming |
| Historically | the buried cable between a battalion and its sector post — and the reason a VHF EW set plus a cable was what could not be jammed | the mobile case, and the one Bekaa's anchor attacked |

**The mast height problem, stated rather than fudged:** a `radio` link between two ground units needs a
declared antenna height or it has no reach at all. `net link radio <rangeM> [mast <m>]` — `mast` `[SET]`
per mission, fed into the existing horizon formula as the node's antenna height. A mission that declares
neither gets a link that does not work, and the event log says why (`net LOST reason=horizon`).

---

### 3. The cue — from the node's own echo to the member's antenna

**The rule, and it is the whole perception argument in one line:**

> **The net delivers a DIRECTION TO LOOK. The member's own radar must still detect, must still firm the
> track over `kHitsToFirm` looks, and must still pass its own envelope test. No track is ever created
> from a message.**

The chain, with the boundary each step already carries:

```
node's own radar  ──▶ FBRadarContact (anonymous, aged, gated)
                  ──▶ FBNetReport published in FBUnitSignature::Net   [the barrier]
                  ──▶ member's link hears it as FBDatalinkTrack::Net    [range, hold, age]
                  ──▶ member's fire control TYPES it: two bus commands  [latency, rejectable]
                  ──▶ the member's own search volume is RE-CENTRED
                  ──▶ the member's own radar looks, and may find nothing
```

**What the cue does to the volume — and it is two existing precedents, not a new mechanism:**

| Quantity | Set from the cue | Precedent |
|---|---|---|
| `AzCenterDeg` of the search volume | the bearing from **this member's** position to the cued point | the MiG-29's `ZONE` switch: a discrete azimuth third, entered by hand |
| `ElCenterDeg` of the search volume | `atan2(cued alt − own alt, planar range)` | **verbatim** the documented MiG-29 range-angle entry ([`modules/mig29/datalink-gci.md`](modules/mig29/datalink-gci.md) §2.2), and the same quantity the F-16's `fcr_slew_el` carries. [`sensors.md`](sensors.md) §4.2: *"putting the antenna on the wrong altitude band is the classic way to fly past a target one could easily have seen"* |

**Nothing else.** Not the range gate (that is the set's own power), not the frame time, not a track file
entry, not a mode change. A cue that points at something outside the member's own reach produces a
slewed antenna and no contact, and that is a correct outcome rather than a failure.

**Why the frame time is deliberately NOT shortened.** A narrower sector would let a set complete a sweep
sooner, and `FrameS` follows the volume in this tree. But whether a given set can sector-scan at all is a
**property of the hardware** — a mechanically rotating parabolic cannot — and that is a per-row catalogue
fact this file does not own. Named, sized (one boolean column plus one `[DERIVED]` relation
`CuedFrameS = SearchFrameS × cued azimuth width / full azimuth width`) and **deferred to the catalogue's
own file**.

**The cue costs time to act on.** Two entries over the command bus, one per decision tick, each charged
its own latency — the identical construction the GCI chain uses, and the only measured analogue in the
tree: **8.0 s from the controller's call to a radiating radar** [MESS, `mig29-intercept.fbm`]. A cue
superseded while still being entered is **abandoned**, not queued (`net CUE_SUPERSEDED`), for the reason
that file already states: typing yesterday's numbers is worse than typing none.

**The EMCON interaction, which is the second thing a cue is for.** A position under
`set emcon hold` is `Dark` until its own passive receiver hears an airborne emitter. A cue is the
**second legal wake-up**, and it is legal for the same reason the first one is: it is a *received signal*
with a range, an age and a sender that can be killed — not a timer and not a registry read. This is
`w4-04`'s and `o1`'s whole shape: an air defence that stays silent until somebody else tells it where to
point. **It is an interface change to [`modules/ground/module.md`](modules/ground/module.md) §Spec 5 and
is booked as a declared dependency in §Gaps, not decided here.**

---

### 4. Belts, sectors and zones — declaring a layering and judging it

Three separate things are needed and they must not be confused:

| Thing | What it is | Who reads it |
|---|---|---|
| **The belt** | N `unit` blocks with positions and catalogue rows. There is no container object — a belt IS its sites | the world, through sensors, as before |
| **The sector** | a member's declared arc of responsibility, `<centreDeg> <halfDeg>` from its own position | the member's own fire control: a cue whose bearing falls outside is ignored (`net CUE_OUT_OF_SECTOR`) |
| **The zone** | a named cylinder, pure geometry, `lat lon radiusM altMinM altMaxM` | **only** the judge and the telemetry writer. No module, no pilot, no sensor |

**The layering falls out of the altitude bands and needs no new concept.** Three zones over one patch of
ground express the whole tactical argument of Bekaa, the first night over Baghdad and Allied Force:

```
zone flak     33.90 35.95  8000     0  1500     # under the SAMs, above nothing
zone sa6      33.90 35.95 22000   100  7000     # the mobile layer
zone sa2      33.90 35.95 30000   450 25000     # the strategic layer
```

A route under 1 500 m is inside `flak` and outside nothing; a climb to 5 000 m leaves `flak` and enters
`sa6`; 20 000 m is in `sa2` alone. **The judge measures which of those the aircraft actually flew, in
seconds.** That is the difference between "he was shot down" and "he was shot down where the doctrine
said he would be".

**How the verdict shows a break-through against a detour:**

| Question | Read from |
|---|---|
| Did he go through? | `zone_<name>_s` > 0 for the belt zones, and the strike objective met |
| Did he go round? | every belt zone at 0 s, and the strike objective met — the same SUCCESS, a different **route cost**, visible as mission duration and fuel |
| Where in the cake did he die? | the zone membership at the tick of `UNIT_RESULT` |
| Did the floor pay? | the two runs of `w4-01`/`w4-10` differ in the `wp` altitudes; the measured quantity is the **trade**: seconds in `flak` against seconds in `sa6` |

**One new objective kind, and one only:**

```
objective avoid zone <name> [exposure <s>]      # fulfilled when cumulative dwell ≤ s (default 0)
```

It is admissible under [`missions/verdict.md`](missions/verdict.md)'s own test — *a judge measures what
the aircraft DID* — because it is a position against a declared cylinder, the identical currency
`identify`'s planar range already uses. **Roster cost: nothing.** It asks about the declaring unit's own
sample, like `waypoints`. It is a **deferred** objective (a zone can still be entered), so it joins
`HasDeferredObjective()` and moves nothing for a mission that does not declare one. `FBObjectiveCovers`
returns false for it, like every non-`kill` kind.

**No `penetrate` kind.** "He broke through" is `kill unit X` plus a non-zero dwell — two facts that
already exist. A kind whose check is the conjunction of two existing checks earns nothing, which is the
argument [`missions/verdict.md`](missions/verdict.md) already makes against `escort`.

**A zone is not derived from the sites, and that is a decision:**

| Design | Verdict |
|---|---|
| **A: the mission declares the cylinder** | **RECOMMENDED.** A mission is then reproducible from its own text — the `C0` determinism rule — and a corrected catalogue envelope cannot silently move an old verdict |
| B: the judge derives the cylinder from each site's catalogue envelope | **rejected.** It puts `kSiteCatalogue` inside `FBMissionMonitor`, and it makes a run's verdict a function of a data file that is expected to change as sourcing improves |

The price of A is paid openly: the author writes the geometry twice (once as `spawn`, once as `zone`)
and **the runner does not police the agreement** — the same non-policing the `time`/`wx` pair already
declares ([`missions/syntax.md`](missions/syntax.md)).

---

### 5. Doctrine and control — who may shoot, and what is left when the node dies

**Three net states per member, and the third is the conservation rule:**

| State | Entered when | Cue | Weapons control |
|---|---|---|---|
| `Netted` | a report from the declared control node is younger than `hold × period` | drives azimuth + elevation | **the node's transmitted WCS** |
| `Silent` | no report for that long — node destroyed, terminal failed, out of range, or jammed | none; the volume returns to the mount's own default | **the member's declared `autonomy` value** |
| `Unnetted` | the mission declared no net for this unit | none | as [`modules/ground/module.md`](modules/ground/module.md) specifies today, unchanged |

The member **cannot tell the four `Silent` causes apart** — its block is `Invalid` either way, which is
the class's own vocabulary for "no picture, not an empty picture". The true cause is written to
`events.log` for the analyst (`net LOST reason=node_dead|terminal|horizon|jammed`). **The judge knows;
the unit does not.**

**Weapons control state — three values, one gate:**

| WCS | Effect on the launch decision |
|---|---|
| `free` | launch against any firm track that passes the envelope test — **today's behaviour exactly** |
| `tight` | launch only against a track that **correlates** with a live cue |
| `hold` | never launch. The set still searches, still tracks, still radiates — which is what makes it a decoy that costs the attacker a weapon |

The gate sits **in front of** the existing launch decision and adds no launch path. `tight` needs a
correlation, and the correlation is **not invented**: it is [`formation.md`](formation.md) §5.2's gate
verbatim — `1 000 m + age · 300 m/s` in three dimensions, plus its unambiguity rule (if the second-best
echo is within twice the distance of the best, the correlation is a coin toss and nothing is claimed).
One rule, two consumers, and the flight already measured it.

**Sector responsibility, and why it is declared rather than assigned:**

| Design | Verdict |
|---|---|
| **A: each member declares a sector; the node publishes the picture; a member acts on cues inside its own sector** | **RECOMMENDED.** No addressing field, no assignment message, no matching. The node owns exactly two things — **the picture and the WCS** — and both are things it measured or was told to publish. The seam between two sectors becomes a real, flyable feature of the defence, and a bad sector plan costs rounds |
| B: the node matches targets to members and addresses an assignment | **deferred, and its price named.** It needs a recipient field on the report, a member-readiness word, and a minimum-cost matching. The matching itself is free (`pilot/FBFlightPicture` already does exactly this over a flight, and a fire control is an `FBPilot`), but the addressing is a new payload shape. It becomes worth it the day a mission wants to measure *fire distribution* rather than *sector doctrine* |

**Under A, two sites whose sectors both contain the target both engage it.** That is not a defect — it is
the measurable cost of the sector plan, and it is the arithmetic every SEAD package rests on
(rounds expended per aircraft engaged). `w3-09`'s saturation question and `o1-08`/`o1-09`'s piecemeal
pair finally have a defender-side quantity.

**The node's death is the campaign's core experiment, and it is three lines of mission text:**

| Run | The one changed line | What it measures |
|---|---|---|
| baseline | — | the netted defence: cued, WCS from the node |
| node killed | the attacker's `objective kill unit <node>` | detection time, rounds expended, engagements per aircraft — **all after** the node dies |
| node jammed | `set jam_comm_m` on one attacker (§6) | the same, without a bomb, and **mid-run** |

And the fallback is the doctrine, not a constant:

| `autonomy` | The defence it produces | The campaign that wants it |
|---|---|---|
| `hold` | batteries wait for orders that never come: silent, intact, useless. **Confidently blind** | `o1-02`, `o5-04` — the anchor's own description of a force cut off from control |
| `free` | every battery engages everything it sees: three sites on one aircraft, rounds gone, the fourth aircraft unopposed | `o1-10`, `w3-09` |

**Both are historically real doctrines and FlightBox refuses to prefer one.** The measured difference
between them is this file's single most valuable output.

---

### 6. Jamming — the minimum, and everything it is not

`C13` is empty and O1's decisive mechanism is **communications** jamming, not radar jamming. The minimum
that makes that one question measurable is therefore **a link that can be denied**, and nothing else.

**The model, complete:**

| Element | Definition |
|---|---|
| Declaration | `set jam_comm_m <rangeM>` on any flying unit. `0` (the default) = not a jammer |
| Publication | `FBUnitSignature::CommJamM` — one float, published at the barrier beside `RcsM2` and the chaff clouds |
| Effect | a link **receiver** within `rangeM` of any unit of **another team** whose `CommJamM > 0` receives nothing this cycle; its block goes `Invalid` |
| Determinism | a distance test. No die, no probability, no ramp |
| Selectivity | other teams only — *"selective airborne communications jamming"* is the anchor's own phrase, and a jammer that killed its own side's Link-16 would poison every existing formation measurement rather than model anything |
| Which end | the **receiver**. Barrage jamming swamps a receiver's front end; denying the transmitter would model a different physical thing and is not what the phrase means |

**What is deliberately left out — the list is the specification:**

| Not modelled | Consequence, stated |
|---|---|
| Radar jamming of any kind: noise, deception, range-gate pull-off, angle-of-jam | a jammed net still sees. An `sa6` under this jammer detects, tracks, illuminates and kills exactly as before — **it only loses the picture from next door.** The N019's documented AOJ chain stays unmodelled ([`sensors.md`](sensors.md) §4.9) |
| J/S ratio, transmit power, frequency, band, burn-through range | `rangeM` is the single power measure, exactly as `FBEmitterSignature::RangeM` is for a radar. One number, `[SET]` per mission, and the mission says what it means |
| Any effect on radar, RWR, IRST, the eye or a seeker | five channels untouched; every existing measurement stands |
| Self-protection versus stand-off versus escort jamming | one radius, one geometry |
| Jamming **detection**, and therefore home-on-jam | the jammer publishes **no** `FBEmitterSignature`: it is inaudible. Publishing one would need a beam window and a kind, and the RWR would then report a threat that cannot shoot. Named in Gaps |
| Partial degradation | the link is up or down. The three-state block validity is the model's own vocabulary and it has no "half" |
| A jammer that is a **unit type** | it is a property of any unit, like a cross-section — so an F-16 can be declared as a stand-in for a 707 or an EF-111 without `C7` |

**What it buys, precisely:** `o1-02` stops being a stand-in. The controller is removed **during** the run,
on geometry, at the moment the jammer closes — which is the case
[`campaigns/o1-bekaa-1982.md`](campaigns/o1-bekaa-1982.md) §Knowledge 4 says is the interesting one and
the present mechanism cannot produce. `C13` does **not** close: it splits, and the radar half stays open.

---

### 7. What the attacking pilot may see — and nothing beyond

| May see | Because |
|---|---|
| Each emitter individually: bearing, mode, estimated kind, received power, age | unchanged. That is what a warning receiver is |
| `SurfaceEarlyWarning` distinct from `SurfaceFireControl` | a waveform property, and the enum value already exists in the `C1` contract. It says *what kind of box*, never *whose net* |
| The **timing**: an EW sweep, then N seconds later a fire-control set on a different bearing that comes up already pointing at him | an emergent consequence of the cue latency, measured by the RWR's own timestamps. **FlightBox gives him the sequence; the conclusion is his own work** |

| May NOT see, and the refusal is structural | Why |
|---|---|
| A count of sites, a net membership, a topology, a "you are being handed off" bit | there is no field for it and none is added. `FBRwrThreat` gains **nothing** in this round |
| Which zone he is in, or that a zone exists | `core/FBZone.h` is `RESTRICTED` to `missions/` and `core/FBMissionMonitor` — a **new** gate entry, i.e. a narrowing. A mission's declared belt is judge data, and a pilot who could read it would know where the SAMs are without a sensor |
| An emitter-location capability | that is HARM (`C8`), a weapon and a mode, and it is not in this file |
| Any behaviour change at all, today | nothing consumes the threat picture ([`modules/ground/module.md`](modules/ground/module.md) G11, and the same `D3` precedent the eye set). A channel is a measurement; a behaviour is its own round with its own measurement |

**The `w4-05` decoy question, answered by construction:** a decoy is a site with `rounds 0` and a small
range gate. It radiates, it is heard, and the receiver measures **power, never range**. Under a net it
gains one more property — a decoy that is *not* on the net never cues anybody and never comes up on a
cue — so a patient attacker can separate the two **by their behaviour in time**, not by a symbol. That is
the honest form of the question and no new channel produces it.

---

### 8. Mission grammar

**Mission scope**, before the first `unit` block, like `wx` and `time`:

```
zone flak 33.9000 35.9500 8000 0 1500          # name lat lon radiusM altMinM altMaxM

net iads                                        # one net per name; several nets may coexist
  control soc_north                             # the node's callsign; optional
  link wire                                     # wire | radio <rangeM> [mast <m>]
  period 4.0                                    # net cycle, seconds          [SET] per mission
  hold 3                                        # cycles before Silent        [SET] per mission
  wcs tight                                     # the node's transmitted state: free | tight | hold
  member sam_north sector 090 60 autonomy hold
  member sam_east  sector 150 45 autonomy free
  member aaa_west                               # no sector = all-round; no autonomy = hold
```

**Actor scope**, one key, and it is on the *attacker*:

```
set jam_comm_m 60000                            # 0 = not a jammer (the default)
```

**Why the net block and not `set` keys on the site:** the `C1` contract declares **six** `set` keys and
says a seventh is a defect. Membership, sector, fallback and link kind are properties **of the net**, not
of the position — a battery does not know it is in a net until somebody puts it in one. Declaring them
in the net block therefore leaves that contract at six and keeps one net's doctrine on one screen.

**The one interface consequence, named rather than smuggled:** the runner must configure a member's link
slot from the net block. The existing path for mission data → module is `FBModule::ApplySetup`, so the
runner **generates** four reserved keys per member (`net_link`, `net_period_s`, `net_sector`,
`net_autonomy`) from the block. They are runner-generated, never author-written, and they belong to this
file. That the site module must answer them is the declared dependency of §Gaps 4.

Parse rules: a `member` naming a unit that does not exist is a parse error (the `objective` rule); a
`control` that is not also a `member` is a parse error; a `zone` with `altMin ≥ altMax` or `radius ≤ 0`
is a parse error; a unit in two nets is a parse error. **An unknown token is a parse error, not a
best-effort read** — the `time` precedent.

---

### 9. Observable

| Channel | Content |
|---|---|
| Telemetry, source `net` (appended last) | `net_state` (Netted/Silent/Unnetted ordinal), `net_age_s` (message age), `net_cue` (0/1), `net_cue_brg`, `net_cue_rng_m`, `net_cue_age_s` (**the sender's look age**, the second staleness term), `net_wcs` (ordinal), `net_in_sector` (0/1), `net_correlated` (0/1) |
| Telemetry, source `zone` (appended last) | one `zone_<name>_in` (0/1) and one `zone_<name>_s` (cumulative dwell) per declared zone, per unit |
| Events, `net` | `JOIN` / `LOST` (with `reason=node_dead\|terminal\|horizon\|jammed`) · `CUE` (bearing, range, alt, look age) · `CUE_SUPERSEDED` · `CUE_OUT_OF_SECTOR` · `AUTONOMOUS` (with the declared fallback) · `WCS` (change) · `CORRELATE` / `CORRELATE_AMBIGUOUS` |
| Events, `zone` | `ENTER` / `EXIT` (with the dwell so far) |
| Reused unchanged | `datalink TRACK_GAINED` / `TRACK_LOST`, every `site *` event of the `C1` contract, `damage *`, `UNIT_RESULT` |

---

### 10. Acceptance criteria

Measured, not argued:

| # | Criterion | Measurement |
|---|---|---|
| 1 | **The gate did not widen** | the `units/FBUnitRegistry.h` includer tuple in `tools/verify_layers.py` still has **six** entries, byte-compared before and after. (The gate's *printout* counts headers and will read **3** because `core/FBZone.h` is added — see §Gaps collision 2) |
| 2 | Existing missions untouched | all `telemetry*.csv` of all committed `.fbm` byte-identical to the pre-round binary; `events.log` identical modulo `wallS`/`speedup` |
| 3 | **The cue is worth a measurable time** | one geometry, two runs differing only in the `net` block: time from target entering the member's reach to its first firm track. The uncued run must be **later or never**, and the mission must be built so the uncued default volume does not happen to cover the target |
| 4 | **The cue cannot invent a track** | the same cued run with the member's `Radar` id failed by `damage`: **zero** `site TRACK` lines, and `net_cue` = 1 throughout |
| 5 | **The cue can be wrong** | a target that turns hard after the node's report: the member slews to the reported point, finds nothing, and the log shows `net CUE` with a look age and no `site TRACK` |
| 6 | **Killing the node changes the defence** | baseline against `kill unit <node>` at a declared time: detection time, engagements per attacker and rounds expended all move, and every member logs `net AUTONOMOUS` |
| 7 | **The two doctrines differ** | the same file twice, `autonomy hold` against `autonomy free`: rounds expended and aircraft engaged differ; `hold` produces zero launches after the node dies |
| 8 | **Jamming denies the link and nothing else** | jammed run against node-killed run: identical `net` collapse, and the jammed run's `site` detection/track/launch lines against a single aircraft are **byte-identical** to the unjammed baseline's for that aircraft |
| 9 | **A `wire` net is not jammable** | the same jammed geometry with `link wire`: zero `net LOST` lines |
| 10 | **The belt is visible in the verdict** | one route through and one route around the same belt: both SUCCESS, `zone_*_s` non-zero against zero, and `objective avoid zone` FAIL against SUCCESS |
| 11 | **Nothing leaks to the pilot** | two runs identical except the `zone` lines: the aircraft's telemetry byte-identical on every column that is not a `zone_*` column |
| 12 | Determinism | one fingerprint over `--threads 1/2/4` × 3 repeats |

---

### 11. The anti-cheat argument, in full

**The seventh-reader question, answered: there is no seventh reader.**

| Candidate path | Verdict |
|---|---|
| A "net" class that walks the registry to find its members | **forbidden.** It is the registry read the whole architecture exists to prevent. Members are found by **hearing them transmit**, exactly as the datalink finds its participants today |
| A derivation of `FBDatalinkSystem` that overrides `Cycle` | would need `.Units()` and therefore the include — **a seventh reader**, and it is why the design is setters instead (§2, design A) |
| The jam test | lives **inside** `FBDatalinkSystem::Cycle`, in the loop that already walks the registry. One more field read from a published signature, like `DatalinkXmt` and `RcsM2` |
| The node's report | **published**, not read. Writing one's own signature is not a registry access; the barrier makes it visible, as it does for chaff and the afterburner bit |
| The judge's zone test | `FBMissionMonitor` is allowed the truth by construction — it is one of the two incorruptible judges. What is new is that `core/FBZone.h` is **RESTRICTED** so nothing else can reach it |

**If a future round does need a seventh, the price is declared in advance the way the eye's was**
([`sensors.md`](sensors.md) §1.2, §9.2): a slot joins the list only by being a **sensor with modelled
limits**, and the entry is a reviewable diff in the gate. This round asks for none.

**The second half of the boundary — the payload:**

| Type | Carries identity? | Why |
|---|---|---|
| `FBNetReport` (new, `core/`) | **no.** `Reporting` bit · target lat/lon/alt · `TgtLookAgeS` · `Wcs` ordinal | it is built from an `FBRadarContact`, which has none. There is no field an identity could be put in — the same absence that makes `FBFlightReport` safe |
| `FBDatalinkTrack::Net` (the carrier) | the **sender's** callsign and team, as today | a cooperative net is one's own faction; the sender gives its own identity away. It says nothing about the target |
| `FBUnitSignature::CommJamM` | **no** | a scalar |

**And the one sentence that carries requirement 1's constraint:** the net can tell a battery *where to
look*; it can never tell it *what is there*. Everything the battery ends up believing was measured by
its own radar, firmed over its own looks, and gated by its own envelope.

---

## State

**BUILT.** `sim/src/` grows two value headers (`core/FBNetReport.h`, `core/FBZone.h`), four setters plus
one test on the existing link (`sensors/FBDatalinkSystem`), four short steps on the existing engagement
machine (`modules/ground/FBSiteFireControl`), one objective kind, two mission scopes and one published
scalar. **No new class walks the registry, and no new class was needed at all.**

> **PRE-FIX, and three rows of the table below rest on rounds that used to destroy themselves.** The
> ground-launch fix of 2026-07-29 ([`modules/ground/module.md`](modules/ground/module.md) §4.1) moved the
> bytes of **`net-cue`, `net-belt-high` and `net-jam-wire`** — three of the ten missions out of 160 that
> changed at all. Everything those rows say about the LINK (join, cue, sector, WCS, autonomy, `net LOST`,
> the mast arithmetic) is upstream of the launch and stands; everything they say about what happened
> AFTER a `site LAUNCH` — a `viper FAIL`, a mission verdict, a launch count that depends on rounds
> surviving — was measured on a binary whose SAM rounds nosed into the ground within 1.6 s and is
> **not re-measured**. **TODO**, and marked per row. `net-cue-unnetted`, `net-blind-cue`, `net-belt-low`,
> `net-jam-late` and `net-jam-start` are byte-identical and their rows are untouched.

| Measured | Number |
| **A `net` block now works on an AIRCRAFT** (2026-08-03) | `modules/FBAirNet.h` answers the runner-generated `net_*` keys for both airframe families identically. On a fighter **the net IS the Link-16 terminal it already carries** — the node's `wcs` arrives on the same block the flight's PPLI does, and the jet's own nearest ANONYMOUS echo goes back the same way as an `FBNetReport`. `net_sector` and `autonomy tight` are REFUSED with a reason (a sector of responsibility belongs to a position in the ground; `tight` needs target addressing this tree has none of). **No existing mission is affected**: all 78 `net` blocks in the tree are ground-only, and the first two friendly nets are new files (`missions/map-friendly-net.fbm`, `missions/map-emcon-gap.fbm`) |
|---|---|
| Existing missions untouched | **336/336** `telemetry*.csv` byte-identical and **112/112** `events.log` identical modulo `wallS`/`speedup`/path, against the pre-round binary |
| Determinism, all 120 missions | **371/371** telemetry and **120/120** events identical over `--threads 1/2/4`, exit codes identical |
| Perception boundary | `verify-layers`: *"3 restricted header(s) respected, **6 registry reader(s) inside the perception boundary**"* — the tuple is unchanged, and a seventh reader added anywhere is **rejected** (counter-checked: `systems/FBSystemSlots.h -> units/FBUnitRegistry.h is not on that header's includer list`, rc=1) |
| The zone gate is a NARROWING | `core/FBZone.h` is `RESTRICTED` to an EMPTY outside-includer list. Counter-checked: a `#include "FBZone.h"` in `pilot/FBPilot.h` is rejected, rc=1 |
| **The cue is worth everything** (`net-cue` vs `net-cue-unnetted`) | one geometry, the `net` block the only difference. Netted: `net JOIN` t=3.9 s, `net CUE` t=8.0 s, `site RADIATE` t=8.0 s, `site TRACK` t=145.1 s, **2** `site LAUNCH` (t=172.0 / 198.9), viper FAIL at t=201.8 **[pre-fix — the second half of this run is where `monitor KO unit=sam_3m9_1 reason=CFIT` used to appear at t=172.8; TODO re-measure]**. Unnetted: **0** `net`, **0** `site RADIATE`, **0** `site TRACK`, **0** `site LAUNCH`, viper SUCCESS. That is criterion 3 in its strongest form — not *later*, **never** |
| **The cue cannot invent a track** (`net-blind-cue`) | one Mk 82 at **52.32 m**, **2 086.81 J/m²**: `Radar` **FAILED**, `FireControl`/`Structure`/`Stores` only degraded. `net_cue` = 1 from t=8.1 s to the end of the run with **no further transition**, and **ZERO** `site TRACK` lines. Criterion 4 |
| **Where in the layer cake** (`net-belt-low` vs `net-belt-high`) | identical route, altitude the only difference. Low (1 200 m ASL, 259 m over the position): `zone_flak_s` = **34.5 s**, `zone_sambelt_s` = **0.0 s**, **54** `gun BURST`, **0** `site LAUNCH`, `avoid zone flak` LOST. High (5 000 m): `zone_flak_s` = **0.0 s**, `zone_sambelt_s` = **320.0 s**, **0** `gun BURST`, **1** `site LAUNCH`, `avoid zone flak` MET, SUCCESS. The altitude that escapes the AAA does put you in the SAM, and the verdict says which storey. **[`net-belt-high` is pre-fix: the round now flies, so the SUCCESS verdict is the quantity to re-check; TODO]** |
| **Blind against confidently blind** (`net-jam-late` vs `net-jam-start`) | jammed **mid-run**: `net JOIN` t=3.9, `site RADIATE` t=8.0, `net LOST reason=jammed` t=128.0 (distM 21 027 against reachM 23 341 — not the horizon), `net AUTONOMOUS fallback=hold`, `site TRACK` t=143.1, **0** launches. Jammed **from t=0**: **0** `net JOIN`, **0** `net CUE`, **0** `site RADIATE`, **0** `site TRACK`, **0** launches. Same nil result, opposite cost: the mid-run loss bought the attacker the battery's position on his RWR |
| **Jamming denies the link and nothing else** | `net-jam-late` against `net-jam-wire`, one line apart: the `site TRACK` line is **byte-identical** in both — `t=143.1 site TRACK unit=sam brgDeg=206.713 rangeM=21977.2 closureMs=223.135 altM=6000 reactionS=26`. The runs diverge at the WCS gate alone (t=169.2: `net WCS state=hold effect="launch inhibited"` against `site STATE to=ENGAGE`) |
| **A wire is not jammable** | `net-jam-wire`, the same jammer on the same geometry: **0** `net LOST`, 2 launches. Criterion 9. **[pre-fix; the criterion is about the LINK and stands — what the two rounds then did is TODO]** |
| The mast is what makes a ground radio link exist | 21.0 km between two 8 m masts gives `reachM` **23 340.7 m** by the 4/3-earth rule. With the pre-round ASL formula the same pair would have read 2×1.23·√(2 208 ft) = **114 km** — a number about the map's absolute height and not about the two antennas |

**The seven runner-generated `set` keys**, not four (§8 named four and could not carry the node's own
identity or its transmitted WCS): `net_link` · `net_period_s` · `net_hold` · `net_sector` ·
`net_autonomy` · and exactly one of `net_control` (a member: whom it subscribes to) or `net_wcs` (the
node: what it transmits). They are generated by the PARSER rather than by the runner, so every client
— `fb-gym`, `gpu_native` and the browser — sees the same configured net from the same file. The site
module's six AUTHOR-facing keys are untouched.

What already existed and was consumed **unchanged**:

| Piece | Where | Used for |
|---|---|---|
| the cooperative net with cycle, hold, age, faction filter, three-state block | `sensors/FBDatalinkSystem` | the whole link |
| a reported **point** and its correlation gate (`1 000 m + age·300 m/s`, plus ambiguity) | [`formation.md`](formation.md) §5.2 | the `tight` WCS |
| a signature published at the barrier | `units/FBSimUnit::PublishPose` | the report and the jam scalar |
| the terminal as a damageable system | `core/FBSystemHealth::Datalink` (id 9) | killing the net with **no new id** |
| a cue that is typed, latency-charged, rejectable and supersedable | `FBCommandBus` (`RadarSlewAz`/`RadarSlewEl`, 0.5 s each, entered one after the other) | the cue's cost |
| a judge with its own private plan copy and pose-only measurement | `core/FBMissionMonitor` | the zone dwell and `avoid zone` |
| deferred objectives, `FBObjectiveCovers`, the conservation argument | `C12`, [`missions/verdict.md`](missions/verdict.md) | `avoid zone` costs the roster nothing |

---

## Gaps

### The honest headline

**The channel is built and its two ends exist; what the tree still cannot do is measure the cue as a
DETECTION advantage.** With no terrain masking (`C4`) a 50 km acquisition set finds anything inside
50 km whatever its elevation window, so "the cue found what my own set could not" has no geometry in
this tree. What the cue IS measurably worth here is the **wake-up of a silent position** (`net-cue`
against `net-cue-unnetted`: everything against nothing) and the **fire-control authority** — and both of
those are doctrine rather than detection. That limit is `C4`'s, not this file's, and it is stated rather
than papered over with an invented clutter floor.

`C6` is still open: no *aircraft* rides the net, so O1's jamming reaches a battery and not a fighter.

And one asymmetry must be said out loud, because it compounds the one `C1` already declared: `C1` gives
the ground the ability to shoot back before the air can shoot first; **this file makes the ground
better at it.** The counterweight (HARM, terrain, a SEAD element that suppresses) is `C8` and `C4`, and
neither is in this round.

### New gaps, booked here

| ID | Gap | Home |
|---|---|---|
| `C22` | **CLOSED.** A cue between positions, sector responsibility, fire-control authority and a control node that can be killed, jammed or fall out of range mid-run. Measured in §State | this file, §§2/3/5 |
| `C23` | **CLOSED.** `zone` declares the belt, `objective avoid zone` judges it, `zone_<name>_in`/`_s` measure it — and `core/FBZone.h` is RESTRICTED so no unit can read one | this file, §4 |
| `C24` | **CLOSED.** `set jam_comm_m` denies a link on geometry, mid-run, with no die. The radar half of `C13` stays wholly open | this file, §6 |

### Existing gaps this file touches

| ID | What changes | What does not |
|---|---|---|
| `C13` | **splits.** `C24` (comms) is specified here; the radar half — noise, deception, AOJ, burn-through, home-on-jam — stays wholly open | no sensor gains an ECM term |
| `C6` | the **ground** half of "a controller that can change or vanish during a run" is specified here: a node that can be killed, jammed, or fall out of range mid-run | the **airborne** half is untouched. `set brief_gci` is still static text, and a MiG-29 still cannot subscribe to anything. §2 design B is what closes it |
| `C18` | the ground-to-ground half of "no radio between units" is specified here | there is still **no voice net**, no call volume, and nothing that saturates under 80 % of the calls — Package Q's third failure mode remains a telemetry read |
| `C19` | the vocabulary `free`/`tight`/`hold` is defined here and gated here **for ground units** | aircraft have no ROE state; W5 and O2 stay blocked |
| `C1` | gains a second legal wake-up cue (§3) and four runner-generated `set` keys (§8) | its six author-facing keys, its five-state machine, its two beams and its nine rows are untouched |

### Found while BUILDING (new, and measured)

| # | Gap | Detail |
|---|---|---|
| B1 | **An inert ground target and a falling bomb radiate a FIGHTER radar** | `FBGroundModule` and `FBStoreModule` leave their `Radar()` slot at `Powered_ = true`, and `FBSimUnit::PublishPose` publishes `Radar().Emission()` unconditionally — so a `target_soft` and a released Mk 82 both put an `AirborneFireControl` beam on the air. It is PRE-EXISTING and visible in the committed baseline (`sam-radar-kill.fbm`, `t=41.1 rwr THREAT_NEW unit=ew … kind=fire-control elDeg=40.56` — that is the bomb). It did not bite before because nothing listened: the datalink walked aircraft only and no ground unit had an ESM that acted on a bearing. It bites now, and it is why `net-blind-cue.fbm` needs `set alert cold` rather than `emcon hold` alone. **NOT fixed in this round**: the fix (`Radar_.SetPowered(false)` in both modules) changes committed `events.log` files and belongs to whichever round owns `modules/ground`/`weapons`, with its own conservation argument |
| B2 | **The cue cannot be measured as a DETECTION advantage** | with no terrain masking (`C4`) a 50 km acquisition set finds anything inside 50 km whatever its elevation window, and every long-range search set in the catalogue is heard by an ESM at twice its own range — so a fire unit inside a node's cue range is ALWAYS inside that node's own emission. §10 criterion 3 is therefore satisfied in its wake-up form (`site RADIATE` at t=8.0 s against never) and not in its detection-time form. The number that would close it is `C4`'s |
| B3 | **`node_dead` is not distinguishable from `terminal` at the receiver** | §5 names four `net LOST` reasons; the built set is three (`jammed`, `terminal`, `horizon`). A node killed outright and a node whose terminal was shot away are the same silence on the air, and telling them apart would mean reading the sender's health register — which is the registry read the whole architecture exists to prevent. Named rather than faked |

### Named, quantified, refused for a reason

| # | Gap | Detail |
|---|---|---|
| N1 | **A cue is stale, never wrong** | the tree has no measurement error and no track confusion ([`sensors.md`](sensors.md) gaps 4/5). So a report is always *the truth, late* — never the truth about the wrong aircraft, never a bearing off by two degrees. Reproducing that needs a die or an invented bias, and both are refused everywhere else in this tree |
| N2 | **No terrain between two nodes** (`C4`) | a `wire` is a straight line through a mountain and so is a `radio` link above its horizon. The horizon formula is a spherical-earth rule with no DEM |
| N3 | **The jammer is inaudible** (§6) | no `FBEmitterSignature`, therefore no bearing to it, therefore no home-on-jam and no "jam strobe". The defence experiences a dead link and cannot even point at the cause |
| N4 | **No fire distribution** | design A means two sites whose sectors overlap both engage. That is the specified behaviour, not an approximation of assignment — B (§5) is the thing that would model assignment and it is deferred with its price |
| N5 | **No peer deconfliction tier** | between `Netted` and `Silent` there could be a third state in which members that still hear each other coordinate without a node. Deferred deliberately: two tiers already answer "what happens when the net dies", and a third multiplies the states before the first has been measured |
| N6 | **One report per node per cycle** | a node reports **one** point — its own most significant contact. A mission that wants more throughput declares more nodes, the same rule `C1` uses for launchers. The consequence is a genuine capacity limit, which is arguably a feature (`w3-09`), but it is a `[SET]` shape and not a sourced one |
| N7 | **No sourced timing anywhere in the net** | reporting cycle, hold, entry latency and mast height are all `[SET]` or inherited. The one measured analogue in the tree is the MiG-29's 8.0 s call→radiating chain, and it is an aircraft's number being borrowed by a battery |
| N8 | **The zone is authored twice** | `spawn` places a site, `zone` declares the geometry, and nothing checks that they agree. A mission can declare a belt where there is none |
| N8a | **Only a JUDGED unit gets zone telemetry** | the dwell lives in `FBMissionMonitor`, and a unit with neither waypoints nor objectives carries no monitor. §9's "per unit" is in practice "per judged unit" — which is every unit whose route a belt is a statement about, and no ground position |
| N9 | **No campaign-scope net state** | `C0` carries units, ground targets and stores across steps. A node killed on night one is alive on night two, and "the IADS was rolled back over 78 days" — Allied Force's actual subject — is not expressible |
| N10 | **The attacker gains nothing** | the pilot AI consumes no threat picture, no zone, no RWR-derived route change. Every question in §0 is answered by *measuring* an AI that flies its briefed route into the belt. That is the `D3`/G11 precedent and it is deliberate, but it means the first results describe geometry more than tactics |

### Collisions with the existing tree, found while writing this

Four, and the first three are the places the tree is actively in the way.

| # | Collision | Detail | Resolution proposed |
|---|---|---|---|
| 1 | **The radio horizon zeroes a ground-to-ground link** | `FBDatalinkSystem::RadioHorizonM = 1.23·(√h₁[ft]+√h₂[ft]) nm` — for two antennas at ground level this is **exactly 0 m**, so the base class's own reach rule makes a ground net impossible before any doctrine is discussed | `SetLinkMode(wire\|radio)` plus an optional declared mast height. `wire` skips the horizon test and is unjammable; that is a modelling *statement*, not a waiver, and §2 makes it |
| 2 | **The gate's printout counts HEADERS, not includers** | `verify_layers.py` prints `f"{len(RESTRICTED)} restricted header(s) respected"`, and `RESTRICTED` has **two** entries today — the run prints *"2 restricted header(s) respected"*. The six that matters is the length of the `units/FBUnitRegistry.h` includer tuple. **Any acceptance criterion phrased on the printout is fragile**, and adding `core/FBZone.h` makes it read **3** while the boundary is unchanged | acceptance criteria phrase themselves on the **tuple** (§10 criterion 1). The `C1` contract's criterion 1 is written on the printout and would have to be re-phrased; it is that file's line to change, not this one's |
| 3 | **`FBDatalinkSystem` cannot hear a ground unit** | `Cycle` skips every unit whose kind is not `FBUnitKind::Aircraft`, before the ordinal, deliberately (a store carries no terminal). A site is `Ground`, so **the class as built cannot carry a ground net at all** | `SetCarriesTerminal(kinds)`, default `Aircraft` only — one flag, byte-identical for every existing mission, and the same shape the N019's three hooks used |
| 4 | **Four runner-generated `set` keys land on a module that declares six** | the net block must reach the member's link slot, and the only mission-data→module path is `ApplySetup`. The `C1` contract says its key set is exactly six and a seventh is a defect | the four keys are **generated**, not author-written, and they are declared here (§8). It is an interface change to [`modules/ground/module.md`](modules/ground/module.md) and belongs to whichever round builds second. The alternative — a `CueLink()` accessor on `FBModule` for every module in the tree — is heavier and is rejected here |

---

## Knowledge

### 1. Why a message may not become a track — the derivation, not the slogan

The perception boundary is *"a pilot sees other units exclusively through simulated sensors"*
([`sensors.md`](sensors.md) §1.1). A net is the first mechanism in the tree that could quietly break it,
because a message is not a sensor and yet it arrives full of geometry. Three properties keep it honest,
and each of them already exists:

1. **The content is bounded by its origin.** A report is built from an `FBRadarContact`. That type has
   range, bearing, az, el, closure, a sensor-owned track number — **and no id, no team, no type**. What
   cannot be put into a report is not a rule; it is a missing field.
2. **The receiver's use is bounded by the command bus.** The cue does not reach `FBState` as a contact;
   it reaches the *antenna* as two commands, with latency, rejectable, supersedable. The receiving unit's
   own track file is written by its own radar and by nothing else.
3. **The truth is bounded by the sender's own age.** The report carries `TgtLookAgeS`, which is the
   node's radar's own age at the moment it reported. A receiver that acts on a 12-second-old cue is
   acting on a 12-second-old measurement, and both terms are in the telemetry.

The counterfactual is worth stating because it is the design that would have been easy: a net that wrote
`FBRadarContact`s into a member's block would make every battery see everything the best radar in the
country sees, instantly, through terrain, with no age. **That is the same cheat as reading the registry,
committed one layer higher**, and nothing in `verify_layers.py` would catch it — which is exactly why the
prohibition is written here as a contract line with an acceptance criterion (§10 criterion 4) rather than
left to taste.

### 2. What the cue is worth, in the model's own currency

Two mechanisms, both already in the tree, and the arithmetic is theirs:

**(a) Elevation.** A fire-control set's search window is narrow — the Fan Song's beam is ~7°
([`modules/ground/catalogue.md`](modules/ground/catalogue.md)). At 30 km, a ±5° window centred on the
horizon covers 0…2 600 m; an attacker at 6 000 m is 11.3° up and **outside it**. Aiming the window is
therefore the difference between detection and none, and the aiming equation is the one the MiG-29's
manual gives for the same act:

```
ElCenterDeg = atan2(cued alt − own alt, planar range)      [DOC, mig29 datalink-gci.md §2.2]
```

**(b) Azimuth.** A volume with `AzHalfDeg < 180` misses whatever is behind it. Re-centring on the cued
bearing is the `ZONE` switch, and it costs one bus command.

**What it is NOT worth, and the number is the reason:** shortening the frame time in proportion to a
narrowed sector would turn a `p18`'s 6.0 s revolution into 0.5 s over a 30° sector, i.e. a firm track in
1.0 s instead of 12 s — a factor of twelve, granted to a rotating antenna that physically cannot sector
scan. **The mechanism is refused until the catalogue declares which sets can.**

### 3. Where the campaigns actually disagree with the position-only model

| Anchor statement | What a position-only model produces | What the net changes |
|---|---|---|
| O1: the RPVs made the SAM radars radiate; the emissions were relayed to E-2Cs and analysed | baiting works (`emcon hold` is a receiver), but the relay and the analysis have no representation | still none — **that is an ELINT loop, not a defence net**, and it is not in this file. Named so nobody reads §6 as covering it |
| O1: the 707s "cut them off from ground control" | nothing to cut | `C24`: a link denied on geometry, mid-run |
| W3: "Kari", a centralised, French-built integrated air-defence system | a set of unconnected emitters, and the campaign file says so | a node, sectors, a WCS — **but the Kari architecture itself is not sourced beyond the name** ([`campaigns/w3-desert-storm.md`](campaigns/w3-desert-storm.md) §Knowledge 2), so the mission declares a plausible one and labels it `[SET]` |
| W3: the mission commander took ~80 % of all calls | no channel to saturate | still none. §N6's one-report-per-cycle is a *capacity*, not a voice net (`C18`) |
| W4: dispersed, radars mostly not emitting, extensive decoys | expressible per position | expressible as a **posture**: a net whose members are `Dark` and are woken only by a node, and decoys that never are |
| W4: SA-6 shoot-and-scoot cycle times | `set scoot_s`, and the campaign says the timings are unsourced | unchanged — and a scooting member drops off the net for exactly as long, which is a second observable of the same act |
| O3: >200 batteries shielding the crossing; the umbrella is **ours** | a striker's discipline metric with an inert umbrella | a `zone` the striker must stay inside — `objective avoid zone` inverted by declaring the *outside*. The friendly-umbrella case needs no separate mechanism, which is the requirement O3 exists to state |
| O5: the airfield's own point defence | absent | a net whose node is the airfield's own radar, and whose death is the attacker's first objective |

### 4. Rejected designs, with the reason each was rejected

| Design | Why not |
|---|---|
| A net object that holds pointers to its members | it is the registry read by another name, one layer up, and no gate would catch it |
| A cue that writes a contact into the member's `FBState::Radar` | §Knowledge 1's counterfactual. It is the cheat this whole file is shaped to prevent |
| Deriving zones from the sites' catalogue envelopes | puts `kSiteCatalogue` in the judge and makes an old verdict move when a data file is corrected |
| A `penetrate` objective kind | its check is `kill` ∧ dwell — two existing checks. The same argument that refuses `escort` |
| Per-target assignment from the node (§5 design B) | needs a recipient field and a member-readiness word; deferred until a mission wants to measure fire distribution rather than sector doctrine |
| A jammer as a unit **type** | it would need `C7`. As a published scalar, any airframe can stand in for a 707 or an EF-111 today |
| A jammer that is audible on the RWR | needs a beam window and a kind; the receiver would then report a threat that cannot shoot, and home-on-jam would follow. Refused as scope, not as physics |
| Radar jamming in the same round | the anchor's decisive mechanism was the **communications** link. Modelling J/S, burn-through and AOJ is a subsystem, and doing it badly here would poison five measured sensor chains |
| A net that carries a track list rather than one point | more capacity than any source describes, and it deletes `w3-09`'s saturation question by construction |

### 5. Where the numbers live, and which of them are settings

| Kind of number | Home | Status |
|---|---|---|
| envelopes, bands, reaction times, channels | [`modules/ground/catalogue.md`](modules/ground/catalogue.md) | not this file's, and not touched |
| net cycle, hold, mast height, jam radius, sector arcs, zone geometry | the `.fbm` | **all `[SET]` per mission**, each with a logged value — the same honesty `set reaction_s` buys the position |
| the cue's entry latency | this file | `[SET]` at the GCI chain's per-entry cost, whose only anchor is [MESS] 8.0 s call→radiating |
| the correlation gate | [`formation.md`](formation.md) §5.2 | reused verbatim: `1 000 m + age·300 m/s`, plus the ambiguity rule |
| the elevation aiming equation | [`modules/mig29/datalink-gci.md`](modules/mig29/datalink-gci.md) §2.2 | `[DOC]`, with the manual's own worked example |
| the horizon | `FBDatalinkSystem::RadioHorizonM` | `[DERIVED]`, the 4/3-earth rule |

**No number in this file is sourced from a document about a real air-defence network.** The doctrine
vocabulary (`free`/`tight`/`hold`, sector responsibility, a control node) is common terminology; the
timings are not. That is the same sourcing asymmetry
[`modules/ground/catalogue.md`](modules/ground/catalogue.md) declares for the hardware, and it is
declared again rather than hidden. The [T1] material that would fix it is the same material named
unread there — TRADOC's *Worldwide Equipment Guide*, the FM 44 series, and the CIA reading room's
Soviet air-defence assessments — plus the two reading-room documents
[`campaigns/PROGRESS.md`](campaigns/PROGRESS.md) already calls the highest-value unread sources in the
directory.

---

## Related

| Place | Relationship |
|---|---|
| [`modules/ground/module.md`](modules/ground/module.md) | the position this file connects. Its five-state machine is the subscriber; its `emcon hold` gains a second cue; its `set` keys stay six |
| [`modules/ground/catalogue.md`](modules/ground/catalogue.md) | the nine rows, and the `p18` line that says a search radar cues nobody — the sentence this file is the answer to |
| [`sensors.md`](sensors.md) | the perception boundary, the cooperative net, the emitter signature, and the RWR that must gain nothing here |
| [`formation.md`](formation.md) | the reported POINT, its correlation gate and its ambiguity rule — one mechanism, two consumers |
| [`missions/verdict.md`](missions/verdict.md) | what a judge may measure, and why `avoid zone` is admissible under it |
| [`missions/syntax.md`](missions/syntax.md) | the two scopes a `net`/`zone` block joins |
| [`campaigns/INDEX.md`](campaigns/INDEX.md) | `C22`/`C23`/`C24`, and the five campaigns whose subject is the net |
| [`core.md`](core.md) | `FBSystemHealth::Datalink`, `FBEmitterSignature`, `FBUnitSignature` — the value layer this contract extends by one type and one scalar |

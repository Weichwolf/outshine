# Campaigns — the ten scenario specifications

**Status: TEN of ten BUILT (O4, O1, O5, O2, W5, W3 and W4 on 2026-07-29; O3, W1 and W2 on 2026-07-30).**
`sim/missions/o4-*.fbm`,
`sim/missions/o1-*.fbm`, `sim/missions/o5-*.fbm`, `sim/missions/o2-*.fbm`, `sim/missions/w5-*.fbm`,
`sim/missions/w3-*.fbm`, `sim/missions/w4-*.fbm`, `sim/missions/o3-*.fbm`, `sim/missions/w1-*.fbm` and
`sim/missions/w2-*.fbm` with their ten `.fbc` files
exist, run, replay and are measured ([`o4-gaf-mig29g-dact.md`](o4-gaf-mig29g-dact.md) §State,
[`o1-bekaa-1982.md`](o1-bekaa-1982.md) §State, [`o5-airfield-defence.md`](o5-airfield-defence.md)
§State, [`o2-pvo-intercept.md`](o2-pvo-intercept.md) §State,
[`w5-baltic-qra.md`](w5-baltic-qra.md) §State, [`w3-desert-storm.md`](w3-desert-storm.md) §State,
[`w4-allied-force.md`](w4-allied-force.md) §State,
[`o3-yom-kippur-1973.md`](o3-yom-kippur-1973.md) §State,
[`w1-red-flag.md`](w1-red-flag.md) §State, [`w2-osirak.md`](w2-osirak.md) §State). **The directory is
complete.** It exists because the missions must not be invented: a campaign without a cited anchor is a mood,
and a mood cannot be measured.

### What O4 established, and what the other nine inherit from it

O4 was built first because it is the only campaign in which BOTH FlightBox airframes really flew against
each other and because its spec listed 8 of 10 missions as runnable. It came out at **10 of 10 runnable,
9 of 10 answerable**, and it left five rules behind that are properties of the machinery rather than of
the Baltic:

| Rule | Why it exists |
|---|---|
| **A controlled variant must not share a callsign with its control** | the store carry is keyed by callsign, so "the same file plus one `wx` line" would also inherit the sibling's expenditure and stop being a control. Carry chains and controlled pairs must not overlap, and the `.fbc` header has to say where each one lives |
| **The carry belongs where the campaign has a QUESTION about it** | O4 puts it on one three-sortie range period and nowhere else, and that one chain inverted a whole engagement (see its §State). A carry sprinkled over every mission destroys the experiments and demonstrates nothing extra |
| **Narrow `carry` when the scenario says so, and pay the price out loud** | O4 drops `units` because a DACT kill is a call and not a lost airframe — and therefore says plainly that it shows no attrition arc. O1 and O5, whose subject IS attrition, must not narrow it |
| **Every result that has two possible causes gets a control run** | O4's weather sortie changes cloud AND wind; the attribution was measured with a wind-only control rather than argued, and the answer was that the cloud contributed exactly zero |
| **A blocked mission is re-checked against the tree, not trusted** | both missions O4's spec called blocked had been overtaken by other rounds. One now decides; one runs and still cannot answer, and it is kept at full size so that the hole has a number (6 of 184 telemetry columns) |

The sixth is not a rule but a warning: **`tools/fb_campaign_verify.py replay` is not optional per
campaign.** O4's first replay was 9 of 10 DIVERGED, and the cause was a real hole in the campaign layer
that `viper-attrition` could not expose ([`../missions/campaign.md`](../missions/campaign.md), "the clock
was missing from the replay half").

### What O1 added, and the other eight inherit it too

O1 was built second, took all five rules unchanged and passed both criteria on the first attempt (9 runs
one fingerprint, 10/10 replays). It contributed three constraints that are properties of the machinery
rather than of the Bekaa:

| Rule | Why it exists |
|---|---|
| **A chain step cannot also be a controlled variant, so budget the ten slots before writing a file** | the carry is keyed by callsign; a mission that inherits a predecessor's state differs from any sibling in two things at once. Ten slots hold one baseline + N single-lever variants + one controlled pair + one chain, and O1 had to DROP a spec mission (piecemeal/massed) to fit. It says which one and why, in the `.fbc` header |
| **Two engagements in one file must be separated in TIME, not only in space** | `FBMissionRunner` ends the whole run at the first flight-monitor K.O. (`FirstFlightKo`). O1's first strike sortie ended at t = 237.0 s with the strikers 130 km short of their targets, because a fighter duel 90 km away produced a crash. Its fix — no fighters in the SEAD pair, and the strikers of the big sortie already past the CAP line — is the shape every mixed package needs |
| **Re-check the SPEC's own mechanism, not just its blockers** | O1's spec said the "confidently blind" case needed a controller that could be silenced mid-run (`C6`). It never did: `set brief_gci <atS> …` has always carried its own time, so a TRUNCATED brief is exactly that experiment. The capability was read, not built |

And one warning of its own: **a campaign is the first thing that fires a subsystem in anger.** O1 is the
first campaign in which ground SAMs shoot, and it found that the 2K12's and S-125's rounds reach the
ground at their own launcher's coordinates seconds after release — pre-existing, visible in the
committed `net-cue.fbm`, and enough to make four closed gaps (`C1`/`C22`/`C23`/`C24`) unable to move any
outcome. Expect the eight remaining campaigns to find one of these each.

**FIXED 2026-07-29, and the fix is the second half of that lesson: the finding was three defects on
FlightBox's side of the seam and cost no deck change** — an initial condition run against JSBSim's own
default ground, a motor cold for 0.55 s, and a `GatherS` that no line of code read
([`../modules/ground/module.md`](../modules/ground/module.md) §4.1). O1's own §State says which of its
numbers that supersedes. **And underneath the fixed defect three more became visible** (a caged MANPADS
seeker, a V-750 that cannot fly its own pitch-over, one gain set shared across three orders of magnitude
of missile mass) — so the corrected expectation for the remaining eight campaigns is not *one* finding
each but *a stack*: the first defect hides the next.

### What O5 added, and the other seven inherit it too

O5 was built third, took all eight rules unchanged and passed both criteria on the first attempt (9 runs
one fingerprint, 10/10 replays). It contributed three constraints, and unlike O4's and O1's they are
about what a campaign may CLAIM rather than about how it is laid out:

| Rule | Why it exists |
|---|---|
| **If the campaign attacks the ground objects, the ground callsigns must be per-mission too** | the carry drops a destroyed unit by callsign. O1 could share one belt set across its seven controlled variants because its belt was never hit in them; **O5's field is attacked in every sortie**, so a shared set would leak sortie 01's damage into sortie 02 and destroy the experiment. The rule is: a controlled variant's cast is disjoint in EVERY unit it can lose, not only in its aircraft |
| **Two defensive layers cannot be measured in one file, and the campaign must say which one it measured** | a battery has no IFF interrogator and an airfield is the one geometry where the friendly aircraft is the nearest firm track (MEASURED: the belt's first three launches go east at its own MiG-29s). Plus `FirstFlightKo`: a mission-killed F-16 falls from 5 000 m in ~122 s, so a defender may kill at most **88 s** before a 34 s bomb's release point and still leave the strike observable. O5 splits its ten slots by layer and states the split in every header |
| **A "closed" gap is closed for the reason it names, not for the campaign** | `C7` is booked CLOSED-with-`ALPHA`-rows, and four rows are `ACCEPTED` — as **flight models**. O5 wanted the F-15C as an escort and measured that no catalogue row can fire a weapon at all (`FBAirModule` composes no fire control). Re-checking a blocker means re-checking the CAPABILITY the mission needs, not the gap's status line |

And one warning of its own, which is O1's sharpened: **expect the defect to be in the seam you did not
look at.** O5 found three, and none of them is in the subsystem the campaign was written about — one is
in the pilot's phase machine, one in the air catalogue's composition, one in a coordinate frame inside
the GCI entry chain that every eastern campaign depends on.

### What O2 added, and the other six inherit it too

O2 was built fourth, took all eleven rules unchanged and passed both criteria on the first attempt (9
runs one fingerprint, 10/10 replays). Its three contributions are about **what a measurement is worth**
rather than about layout or claims:

| Rule | Why it exists |
|---|---|
| **A campaign result that contradicts three earlier ones is a different QUANTITY until proved otherwise** | O1 and O5 measured the controller at nothing, three times. O2 measures him at the entire intercept — and the difference is not the theatre. In all three earlier files the MiG's radar was already `illum` at spawn, so deleting the brief left it badly AIMED; O2 flies the documented power-up state (`n019_emission off`), where the brief's third entry is the only thing that turns the radar on at all. The deciding line is `set n019_emission`, not `set brief_gci`, and a campaign that had not looked would have published a contradiction |
| **A "read the source" item is worth a round of its own, and the blocked path is not the only path** | `PROGRESS.md` had carried two CIA documents as *"the highest-value unread source in the directory"* since run 1, marked "not retrieved this pass". They are Akamai-blocked on `cia.gov` and completely available on the Wayback Machine. Reading them moved O2's doctrine half from [T4] to [T1], supplied the corridor unit that sortie 09 and 10 fly, and — the sharpest of the four — gave the campaign's own result its proportion: FlightBox times a **cockpit** loop at 8.0 s against an anchor whose **ground** loop took 10–15 minutes |
| **A defect that is in the way must be measured on BOTH sides of its threshold** | O5 found the GCI world/body elevation frame and measured it where it bites (0 contacts in 700 s). O2 needed the same chain and measured it where it does NOT: the error is exactly `st.pitch`, `FBAutopilot` holds a 5.36…5.89° climb pitch band, the RAD bar is ±6.0°, and the margin is **0.11 to 0.64 degrees**. The bite condition — `|pitch| + |target body-frame offset| > 6.0°` — is a number the fix can be tested against, which a single bitten case was not |

And one warning of its own, which is the third rule turned around: **the outcome axis of an air-to-air
campaign is not ordered by the quality of its inputs.** O2's baseline put two R-27R inside the fuze at
4.85 m and 4.75 m and did not kill; its *degraded* sibling put one in at 2.48 m and did. The deciding
variable is the damage ZONE, so any campaign that reads "kills" as a lever's effect is reading a
warhead's arrival geometry. Read the detection times.

### What W5 added, and the other five inherit it too

W5 was built fifth — **the first campaign in which the F-16 flies**, and the only one of the ten whose
success condition contains no weapon. It took all fourteen rules unchanged, budgeted O2's three-file
rule *before* writing the first file, and passed both criteria on the first attempt. Its two
contributions are about what a campaign may conclude from an ABSENCE:

| Rule | Why it exists |
|---|---|
| **A gap that a campaign is "blocked on" is a claim about a CAPABILITY, and the capability may live in a row rather than in a subsystem** | W5's own headline said it *"cannot do the identification"* because there was no eye. The eye was built — and it was still not the deciding thing. What made the task performable is that the SUBJECT is a transport: 29.3 m of span against a fighter's 9.14, identified at 1 086 m where a fighter is not recognised at 1 600. The row (`an26`, a MOVER, which needs no generated deck and so is untouched by `C7`'s `ALPHA` verdict) was the blocker, not the sensor. **Re-check what the mission needs, then ask which layer supplies it** |
| **A "nothing happened" result must name WHICH of the two nothings it is: restrained, or absent** | `w5-07` arms two F-16s, switches the master arm on, paints them with a fighter radar for 300 s and measures zero releases — which looks like discipline and is not. `eng_state` is `idle` for all 3 001 rows: a pilot on `route` never enters the engagement machine at all. The distinction is one telemetry column and it inverts the meaning of the whole sortie, and it is the same shape as rule 11 one level down |

And one warning of its own: **the task with no weapon in its success condition is the one the pilot AI
abandons.** `FBPilot.cpp:1040` classifies *inside 5.0 nm and never shot* as an ABORT, which is the
definition of an identification pass — measured, the interceptor turns away at t = 5.1 s. Every pass in
all ten W5 files is a route a human wrote, and any campaign whose subject is presence rather than
combat should expect to author its geometry rather than fly it.

### What W3 added, and the other four inherit it too

W3 was built sixth — **the first campaign whose opponent is a SYSTEM rather than an aircraft**, and the
first to be built entirely out of capabilities that did not exist when its own spec was written. It took
all sixteen rules unchanged and passed both criteria on the first attempt. Its two contributions are
about **the value of a lever being a property of the topology it is pulled in**:

| Rule | Why it exists |
|---|---|
| **A lever's value is a property of the STRUCTURE it acts on, and a campaign that measures it once has measured one structure** | W3 killed the same early-warning radar twice. Against a net with ONE node it was worth the entire strike (0 launches against 10; 2 of 2 strikers reaching release against 0 of 2). Against a net with a second node it was worth **9 of 25 cue messages and nothing else** — same file, same bomb, run as campaign step and standalone, 30 of 58 telemetry files byte-identical and not one trajectory column moved. This is rule 11 one layer down, and it is the direct qualification of O5's *"one Mk-84 on the P-18 costs the missile layer two nights"*: **O5's field had one node** |
| **A campaign must state which of its subject's failure modes it can STAGE, before it reports what it measured** | W3's anchor names three, and W3 can stage exactly one. The other two are not "hard" — one has no channel to saturate (`C18`) and one is blocked *twice*, by `C5` and by a pilot branch that is unreachable. Writing that table first is what stopped the campaign from reporting a substitute as an equivalent |

And one warning of its own, which only a large mission can produce: **density is a mechanism.** A
proximity fuze is resolved against every published pose except the launcher's, with no team test — the
same boundary that makes a seeker blind to identity — and at 24 aircraft that becomes an attrition
channel. In W3's capstone **one of the three aircraft lost was killed by its own side's missile**
(11.74 m against a 13.8 m fuze, a MiG-29 of the other Red flight). No 2v2 campaign in this set could
have seen it.

### What W4 added, and the other three inherit it too

W4 was built seventh — **the campaign whose subject is finding rather than killing**, and the first
whose central number came out of the collision between two of its own anchor's facts. It took all
eighteen rules unchanged and passed both criteria on the first attempt. Its two contributions are about
**a countermeasure that is cheaper than the doctrine it replaces**, and about naming the arena's own
absence in the file rather than in the report:

| Rule | Why it exists |
|---|---|
| **Total emission discipline and a decoy buy the same survival, and only one of them also keeps the fight — so a campaign that measures "going dark" has measured the EXPENSIVE option** | one geometry, one lever, three ways: with the node radiating the suppression pair kills it in 66.5 s; with everything on `emcon hold` the defence keeps every position and gives up **6 rounds, 2 firm tracks and every cue message**; with three `p18` decoys in front of it the defence keeps the node AND fires **7 launches against 4** while both AGM-88 die on a tin shed. And **none of the three moves the strike** — the same two strikers reach the same aim points at the same ticks with the same `aimErrM` in all three files. This is W3's *"emission discipline is worth the position and nothing else"* one level out, and the level out is where the alternative appears |
| **An absence that decides a result belongs in the mission file, priced, not in the report as a caveat** | W4 is `C4`'s acceptance test and it cannot mask anything, so every W4 header carries the disclosure AND its consequence for that file's claim, and the capstone states that it stages **three** of its own four antagonists. The campaign then measured what the missing half would have cost: two runs under `--elev tiles` over the real Kosovo (ground 547.88 m against a 0 m plane) move `predErrM` by 11.6 m and produce **zero masks**, because masking is a computation and not data. An unbuildable thing that has a number attached is a gap; one that does not is a mood |

And one warning of its own, and it is the sharpest thing the campaign found: **two anchor facts can be
mutually exclusive, and a campaign that flies both without measuring the overlap publishes a fiction.**
NATO's 15 000 ft floor and the F-16CJ's AGM-88 are both [T3]/[T4] facts of this operation. Measured on
a nine-point ladder, an AGM-88 launched at 20 km holds a P-18 up to **4 150 m** and loses it at
**4 200 m** — because the last fresh look of every failing shot is at exactly **15.00°**, the radar's
own published elevation limit. **15 000 ft = 4 572 m.** Every W4 Weasel therefore flies 1 572 m below
the floor the rest of its package obeys, and the campaign says so in the header of every file that does
it.

### What O3 added, and the other two inherit it too

O3 was built eighth — **the campaign that had ZERO runnable missions until the day it was flown**, and
the only one of the ten whose surface-to-air umbrella belongs to its own side. It took all twenty rules
unchanged and passed both criteria on the first attempt. Its two contributions are about **what an
absence of identification does to a force that owns the missiles**, and about the carry:

| Rule | Why it exists |
|---|---|
| **A friendly weapon system with no identification is not a degraded shield, it is a NEGATIVE one — and the measurement has to count rounds by INTENDED TARGET, not by outcome** | O5 measured a belt firing east at its own MiG-29s and W3 a proximity fuze killing a MiG-29 of the other Red flight. O3 is the file where that is the SUBJECT, and at campaign scale the number is total: **28 surface-to-air launches, 28 of 28 aimed at its own aircraft, 0 ever aimed at an enemy**, one own MiG-29 destroyed and one enemy F-16's avionics destroyed by a round fired at a MiG-29. The mechanism is two facts, both re-checkable: `FBSiteFireControl` has no IFF path at all, and every SAM row has `Channels = 1` — so a friend inside the envelope does not degrade the engagement, **it deletes it**. Counting kills would have reported "harmless"; counting `sms LAUNCH_SOLUTION … tgtLat/tgtLon` against both sides' telemetry reported the truth |
| **`carry stores` can be the most decisive thing in a campaign, and whether it is depends on whether the carried quantity has MASS** | W3's carried node moved 28 of 58 telemetry files in bookkeeping columns and no trajectory. O3 carries one unexpended FAB-500 per aircraft into a 25-unit capstone and **0 of 48 telemetry files are byte-identical**: the store is 499 kg on ONE inboard pylon, so it changes the mass AND the lateral balance, `aimAcrossM` goes **+48.3 → −39.2 m** (an 87.5 m swing that FLIPS SIGN, larger than the store's own 68.4 m kill radius), and the two aircraft the campaign loses are different aircraft with a different cause. **One unexpended bomb is worth one aircraft and 238 seconds of run** |

And one warning of its own, and it is about reading a campaign's own arena rather than its levers:
**check the mission spawn altitudes against the elevation source you are about to fingerprint.** Every
O3 striker is at 300 m ASL because a 6 km rangefinder caps this airframe's level-bombing altitude at
2.0–2.2 km — and `fb-gym`'s OWN default is `--elev swiss` when the baked DEM is on disk, which validates
an explicit spawn altitude against the resolved ground and FAILS the mission before it flies. `--elev
const` is not a comparability convention for this campaign, it is a precondition, and every command in
its record passes it explicitly.

### What W1 added, and the last one inherits it too

W1 was built ninth — **the only campaign whose contract is that its rungs disagree**, and therefore the
one the saturation gate had to be pointed at. It took all twenty-two rules unchanged and passed both
determinism criteria on the first attempt. Its two contributions are about **what a measurement is worth
when the gate refuses the arena it was taken on**, and about the carry running backwards:

| Rule | Why it exists |
|---|---|
| **An arena's informativeness is a property of the SEAT the lever is pulled in and of the FORCE RATIO, and a campaign that reports a ladder must report which of its rungs could have gone differently** | W1 flew the nine declared doctrine levers on all ten of its own rungs, on both seats, 180 runs. **The gate REFUSES the arena: 2 informative geometries against a required 3, in both seats.** The movers concentrate on opposite ends — Blue's on the two rungs with one aircraft a side (4 and 5 of 9), Red's on the two multi-ship rungs (4 of 9 each) — which is `duels.md`'s tournament finding (the early launch is an outcome band on the MiG and nothing on the F-16) reproduced on hand-authored geometries. And the three rungs that DECIDE anything are exactly the three at 1v1 and 2v1: **a FlightBox air-to-air result above two aircraft a side is a fixed point**, so the eight earlier campaigns' 4v4-and-larger capstones are outcome-blind by construction and only their ground halves grade. `doctrine-evolution.md` had measured *"a 2v2 is MORE saturated than a 1v1"* on generated cells; W1 is the campaign that pays for it |
| **The carry can run BACKWARDS — removing a unit is an anchor's own mechanism, not an approximation of one — and its price must be attributed one fact at a time** | O4 dropped `units` because a DACT kill is a call. W1's spec lists *"Range control / kill removal"* as **"not a unit — a campaign-layer function (`C0`)"**, so the layer's own deletion IS the documented procedure and W1 carries all three facts on purpose. Measured: 7 `campaign CARRY` lines (one drop + six store lines) are worth **one F-16** — standalone the same file loses two Blue aircraft, in campaign one — and **1 of 17** common telemetry files stays byte-identical. Attributed with `--carry` one fact at a time: `units` alone → 1 loss, `stores` alone → 1 loss, neither → 2. **Either half alone is sufficient**, which no single comparison could have told apart |

And one warning of its own, and it is the cheapest defect in the set to trip over: **a combat phase can
be silently unarmed by an absent navigation waypoint.** `FBF16FireControl` invalidates its entire block
when `state.Nav` is unreadable and `FBNavSystem` publishes nothing without a steerpoint, so a `set task
bfm` jet with no `wp` line has no gun solution, no DLZ and no missile gate — measured as `blk_firecontrol`
= 0 for 3,001 rows, 0 shots, and 14.8 s of unbroken lock from 7.5 km to 185 m. There is no rejection and
no log line. Four committed `bfm-*` missions are in that state today.

### What W2 added, and it closes the directory

W2 was built **tenth and last** — the only campaign in the set whose antagonist is not an opponent but a
quantity, and the only one whose central result is a **subtraction**. It took all twenty-four rules
unchanged and passed both determinism criteria on the first attempt. Its two contributions are about
**what a campaign owes a value its own sources disagree on**, and about a hole in the layer below it:

| Rule | Why it exists |
|---|---|
| **A [DISPUTED] anchor value is not settled by picking one — it is put to the tree in BOTH halves, and the answer may be that neither is flyable** | W2's ingress altitude is 30 m [T4] or 240 m [T4], a factor of eight, and its own spec says the difference decides whether the raid is a terrain-following problem. Both were flown. Over the campaign's `--elev const` plane the 240 m leg holds to **0.85 m over 300 km** and the 30 m leg holds too — and turns out not to be a terrain problem at all but a **fuzing** one (`armMarginS` **0.486 s** of the Mk-84's own 2.0 s arming time). Over the ground the raid actually crossed, **neither is flyable**: under `--elev tiles` the mission fails before tick one (`spawn altitude is below ground, altM=240 groundM=487.48`) and the route's real ground reaches **1,599.22 m**. A campaign that had preferred one value would have published a number about an arena instead of an answer about a dispute |
| **The carry carries three facts and a campaign may need a fourth — and when it does, the honest move is to say so rather than to widen the contract** | [`../missions/campaign.md`](../missions/campaign.md) refuses **fuel** as a carried fact, with a stated and correct reason (*"carrying fuel across a landing models the GROUND time, which does not exist"*). W2 is the campaign whose antagonist IS fuel, so the refusal lands on its subject: every W2 fuel number is a **within-sortie** number and the layer above the missions is structurally blind to the thing the campaign is about. Nothing was widened. What the carry CAN do it did, and it did it sharply: one `action=drop` line, and **removing the escort that would have died killed the one that would have lived** (standalone 1 of 2 alive, in campaign 0 of 1) while **8 of 29** telemetry files stayed byte-identical — *exactly the eight bombs* |

And one warning of its own, which only a campaign about a quantity could produce: **the biggest lever
in a campaign is not always the one the campaign is named after.** W2 was built for the tanks, and the
tanks are worth +45.6 % clean and +50.5 % under the war load. **The ingress altitude is worth 43.2 % of
the range on its own** — the raid's own defining tactical choice is the most expensive thing in it, and
neither the spec nor the anchor's sources say so anywhere.

### The re-verification of 2026-07-30, and what it says about the two instruments

Before W2 was built, both determinism criteria were re-run on **all nine** earlier campaigns under the
branch-order change of `b433950` — **81 campaign runs, 90 standalone replays, 0 divergences**, every new
fingerprint written into its own §State beside the old one. Three findings belong to the whole
directory:

- **Eight of nine fingerprints moved; W5's did not, byte for byte.** The reason is W5's own published
  property — zero rounds expended over ten sorties, and no jet that ever enters `Defend`, which is the
  state the re-ordered branch owns.
- **Two step EXITS moved, and the commit that caused them had claimed none did**: `o3-07-top-cover`
  1 → 3 and `w4-10-allied-force` 3 → 2. Both were already traced in [`../pilot.md`](../pilot.md) §7.4b;
  what had not happened was the booking. Both campaigns' §State now carry it.
- **The campaign fingerprint and the stock-mission regression are different instruments.** `pilot.md`
  §7.4b names five W1 files as movers; the campaign fingerprint moved **eight of ten**. `fb_regress.sh`
  runs missions standalone and **unclocked**, and nine of ten W1 files declare no `time`; the campaign
  clock alone moves 2 and 7 telemetry columns (`blk_env`, `vis_*`) and **zero trajectory columns**.
  **A mission list from one cannot predict the other.**

This directory is **step 4 of the owner goal** and its specification comes first, per
[`../conventions.md`](../conventions.md)'s spec-first rule: *change the Spec of the topic file first;
if a round cannot say what the contract becomes, it is not ready to start.*

---

## The ten campaigns

Five in which the **F-16** flies, five in which the **MiG-29** flies. Ten missions each.

| # | Campaign | Anchor | The hook |
|---|---|---|---|
| **W1** | [Red Flag / Nellis](w1-red-flag.md) **— BUILT** | the USAF aggressor enterprise, 1975– ([T1] since 2026-07-30: the 403-blocked 414th CTS fact sheet was read through the Wayback Machine) | the training ladder — and the inversion that at Nellis the "MiG-29" is an F-16 pretending, while here it is the real module. **Flown 2026-07-30, and it is the campaign that put the saturation gate to its own ten missions: the gate REFUSES the arena** (10 geometries, **2** informative against a required 3, modal share 55.6–100 %, movers 0–5 of 9), because the three rungs that decide anything are the three at 1v1 and 2v1 — **a FlightBox air-to-air result above two aircraft a side is a fixed point, and only the ground half of a package grades**. The anchor's own ten-mission rationale is now [T1] verbatim; the trigger fires at **0.978 × Rtr** on both airframes and is blind to a Raero 1.26× longer; the DCA rung's CAP is worth **11 of 184 telemetry columns and zero metres** (its control run reproduces the attack's 101.05 m miss to five decimals); total emission discipline costs Blue every shot and Red **10 launches for 0 arrivals**; a radar-less wingman is never even ASSIGNED a target; the aggressors' 18 s shot lead turns **four Blue triggers into one**; and kill removal plus a spent magazine is worth **one F-16** |
| **W2** | [Osirak 1981](w2-osirak.md) **— BUILT** | Operation Opera, 7 June 1981 | reach, not combat: 1,600 km, a 30 m ingress and tanks that ran dry — the campaign whose spec called itself *"the one FlightBox is furthest from being able to fly"*. **Flown 2026-07-30, the day after half its blocker closed, and it is the only campaign of the ten whose central result is a SUBTRACTION:** with the anchor's own load, tanks and ingress altitude the combat radius is **874.4 km against the 982.9 km the raid needed — 108.5 km short**, with zero reserve and an air start; **the ingress altitude alone costs 43.2 % of the range**, more than the tanks give back and more than the bombs take away; the tanks are worth **+50.5 %** and dropping them when dry another **7.1 %**, with the externals dry at 675.8 km against the anchor's "about 1,000 km"; **neither half of the [DISPUTED] 30 m/240 m ingress is flyable over the ground the raid crossed** (`--elev tiles`: `spawn altitude is below ground, altM=240 groundM=487.48`) and over a plane the 30 m variant is a FUZING problem (0.486 s of a 2.0 s arming time); a hardened dome needs **17.7 m** and the level laydown delivers a **6.36–50.83 m** band, so the capstone kills it with **5 of 8**; four anti-aircraft guns firing 39 rounds are worth **7 of 184 columns and zero metres**; and a radar blind spot is not expressible because **there is no channel from a radar to a fighter to be blind in** |
| **W3** | [Desert Storm, the first nights](w3-desert-storm.md) **— BUILT** | 17 January 1991 + Package Q, 19 January | a package against an integrated air defence, and the three named ways Package Q came apart. **Flown 2026-07-29, the first campaign whose opponent is a SYSTEM:** its spec called five of ten blocked and every blocker had closed; of Package Q's three failure modes FlightBox can stage **one**, and the campaign says which and why. **A suppression element is worth one striker's release and the target — and it is worth that even when its missile falls 10 km short**, because a battery with no IFF and no threat priority empties its magazine at whatever is nearest; **emission discipline is worth the position and nothing else** (dark at 57.4 % of the round's flight, the HARM lands 214 m short, the crew comes back and shoots at the departing Weasels); the same bomb on the same radar is worth the whole strike against a one-node net and **36 % of the cue traffic** against a two-node one; and at 24 aircraft **Red killed one of its own with an R-27R** |
| **W4** | [Allied Force 1999](w4-allied-force.md) **— BUILT** | 24 March – 10 June 1999 | mountains, cloud and an air defence that refuses to emit — the campaign the weather hook was built for. **Flown 2026-07-29, and its two anchor facts turned out to be mutually exclusive:** an AGM-88 holds a P-18 from 4 150 m and loses it at 4 200 m, at the radar's own published **+15.00°** elevation limit, so the **15 000 ft floor sits above the ceiling of its own SEAD weapon**; **a decoy is the same catalogue row as the node it protects** and three of them absorb the whole suppression element while the belt fires 7 rounds instead of 4; **total silence keeps the positions and gives up the entire engagement**, and neither policy moves the strike by one metre; the weather that is MEASURED is the **wind** (5.014 m of bomb miss per knot, and 0 of 6 kills where calm gives 3 of 6), and the deck is scenery except for **one** line of sight closed to 8.0e-13; and the carried node is worth **25 % of the cue traffic and nothing else** |
| **W5** | [Baltic Air Policing / QRA](w5-baltic-qra.md) **— BUILT** | NATO air policing, 30 March 2004– | **identification as the task**, and the sharpest anti-cheat test in the set. **Flown 2026-07-29, the first campaign in which the F-16 flies:** the task turned out to be blocked on a CAST ROW and not on a sensor — a transport is named by eye at **1 086 m** and a bomber at **2 049 m** where a fighter is not recognised at 1 600; the anti-cheat pair is **byte-identical in 6 of 6 telemetry files with 1 differing log line of 75**, and its control run moves **5 of 184 columns and zero metres**; **zero rounds expended over ten sorties**; and the one task with no weapon in its success condition is the exact geometry `FBPilot` calls an ABORT |
| **O1** | [Bekaa 1982, the Syrian side](o1-bekaa-1982.md) **— BUILT** | Operation Mole Cricket 19, 9 June 1982 | the canonical defeat, reframed as a measurable question: **what in the doctrine moves the outcome, and what is left when nothing does**. **Flown 2026-07-29:** the baseline reproduces the rout; ONE lever (launch at 1.4 × Rtr) inverts it; the controller is worth everything on a 45° entry and nothing head-on; the warning receiver, the belt, the net and ~~the anchor's own jamming~~ move mechanisms and no outcome. **AMENDED 2026-07-29:** the belt's nullity was a defect, not a doctrine — the ground-launch fix gave the rounds a trajectory, and the jamming lever now costs the belt 8 launches, 7 detonations and 2 positions. See the file's §"The ground half, re-measured" |
| **O2** | [PVO intercept exercise](o2-pvo-intercept.md) **— BUILT** | Soviet GCI doctrine + the MiG-29's own guidance panel (**doctrine half raised to [T1] 2026-07-29**: the two CIA reading-room documents were read) | ground control in its pure form, and identity that always costs surprise. **Flown 2026-07-29:** the loop is **11.0 s and splits 8.0 + 3.0**; the controller is worth the ENTIRE intercept on an aircraft that starts silent, which is why three earlier campaigns measured him at nothing; a wrong azimuth third deletes the intercept outright and a wrong altitude band deletes it for one aircraft of two, recovered 28 s late by a dead-band accident; a late commit buys 45.3 s of the target's silence and costs 77 % of the detection range; and the faction swap is **byte-identical in 5 of 5 telemetry files with 1 differing log line of 53** |
| **O3** | [Yom Kippur 1973](o3-yom-kippur-1973.md) **— BUILT** | the opening strikes, 6 October 1973 | ground attack under a **friendly** SAM umbrella — the only campaign in the set where the missiles are ours, and until 2026-07-30 the only one with **zero** runnable missions. **Flown 2026-07-30, the day its blocker closed:** the friendly umbrella fires **28 rounds, 28 of 28 at its own aircraft**, kills one MiG-29 and never once aims at an enemy — and its only effect on the enemy is a round fired at a MiG-29 that failed ten of an F-16's systems; **weapons hold costs nothing and gives up everything**; a hardened position is unreachable by a factor of 5.2 and **a 66 m miss on one is not even a recorded event**; this airframe's cross-track error is **48.3 m + 33.9 m per degree of dog-leg**, so the longest usable final is under 12 km and the largest admissible turn inside it is **0.59°** — the anchor's own 220-aircraft coordinated approach is not flyable here; the top cover engages 43 s after the bombs are down, hits nothing with 8 rounds and ends 103 km away; and **the defender's lateness is a CLOSURE fact, not a detection one** (Blue has the 300 m strikers at 45.16 nm at t = 3.9 s and cannot shoot until 122 s after the last bomb lands) |
| **O4** | [GAF MiG-29G DACT](o4-gaf-mig29g-dact.md) **— BUILT** | JG 73 Laage, 1991–2003 | the one campaign in which **both** FlightBox airframes really flew against each other — and the cheapest to build, because half of it is already measured. **Flown 2026-07-29:** the ten-mile claim holds at ten miles, trades at five and loses at two |
| **O5** | [Airfield defence](o5-airfield-defence.md) **— BUILT** | Batajnica 24–26 March 1999, with Iraq 1991 as the parallel | a defender whose success is something **not happening**. **Flown 2026-07-29:** the vocabulary turned out to be the easy half — a pair already on station denies half a two-ship and nothing else comes close; the controller is worth 6 s; **one bomb on the field's P-18 costs its missile layer every launch for two nights**; and the campaign's own most important parameter, the scramble, **cannot be declared at all** |

---

## How to read a campaign file

Every file carries the tree's four sections, mapped onto a campaign:

| Section | Content here |
|---|---|
| `## Spec` | the anchor in one table · the campaign contract · **the ten missions** · **the cast** · what must be true before mission 1 can fly |
| `## State` | what is built (today: nothing) and which existing FlightBox pieces the campaign would consume unchanged |
| `## Gaps` | **what FlightBox cannot do for this campaign**, with the shared gap IDs — plus one "honest headline" naming the single worst hole |
| `## Knowledge` | the anchor **with its sources**, the disputes left standing, where the sourcing is thin, and the derivations |

### Four rules that hold in all ten files

1. **Every number carries a source and a confidence tier.** Nothing is averaged, smoothed or inferred
   into existence. Where two sources disagree the file says so and carries **both**.
2. **Where the flying jet does not match the historical operation, the file says so in its own
   header** and treats the anchor as a **scenario archetype** — *the situation is the anchor, not the
   serial number.* It also states the **direction** of the substitution: for O1 and O3 the MiG-29
   makes the defender materially **stronger** than history, which means a FlightBox result that still
   goes badly says something stronger than the record did, and one that goes well says nothing.
3. **Ground targets appear in every mission of every campaign**, not only the strike ones — including
   the pure air-to-air rides. A fight with nothing on the ground has no reason to be where it is.
4. **Each mission states exactly one tactical question**, and a run whose telemetry cannot answer that
   question is a badly built mission rather than a result.

### Confidence tiers (identical in all ten files)

| Tag | Meaning |
|---|---|
| **[T1]** | official / government / service document (NATO, USAF, CIA reading room, service histories) |
| **[T2]** | manufacturer or service publication — here chiefly the two DCS manuals via `doc/modules/mig29/` |
| **[T3]** | established literature and specialist press (RAND, Osprey, Air & Space Forces Magazine, Smithsonian) |
| **[T4]** | encyclopaedic / community consensus (Wikipedia, enthusiast press, forums) |
| **[DISPUTED]** | sources conflict; both values carried, neither preferred |
| **[SET]** | a FlightBox setting chosen in this directory, not sourced |
| **[DERIVED]** | computed from a named relation |

**The honest overall picture of the sourcing:** the western campaigns rest largely on [T3]/[T4] with
some [T1] (NATO and AFHSD pages for Allied Force, a Marine Corps study for 1973); the eastern
campaigns are markedly thinner, and O2's doctrine half is the thinnest thing in the directory — the
[T1] material that would fix it (two CIA reading-room documents) is identified and **unread**. That is
the same sourcing asymmetry `doc/modules/mig29/` already declares for the aircraft itself, and it is
declared again rather than hidden.

---

## What is buildable today

**50 of the 100 missions when this table was written; all 100 are now BUILT and every row is re-counted
against the tree rather than against the spec.** Per campaign:

| Campaign | Runnable today | Blocked | The first pair to build |
|---|---:|---:|---|
| O4 GAF DACT | **10 — BUILT** | 0 (1 runs but cannot answer) | done: ten `.fbm` + one `.fbc`, both determinism criteria measured |
| O1 Bekaa | **10 — BUILT** | 0 (the ground half runs and cannot decide — see its §Gaps row 1) | done: ten `.fbm` + one `.fbc`, both determinism criteria measured on the first attempt |
| O2 PVO | **10 — BUILT** | 0 (the spec's mission 9 folded into the chain, named in the `.fbc` header) | done: ten `.fbm` + one `.fbc`, both determinism criteria measured on the first attempt |
| O5 Airfield defence | **10 — BUILT** | 0 (2 spec missions dropped with reasons in the `.fbc` header) | done: ten `.fbm` + one `.fbc`, both determinism criteria measured on the first attempt |
| W3 Desert Storm | **10 — BUILT** | 0 (the spec called 5 blocked; all 5 blockers had closed, and 1 spec mission was dropped for `C15` with its slot named in the `.fbc` header) | done: ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt, the replay run after the FIRST mission |
| W4 Allied Force | **10 — BUILT** | 0 (the spec called 6 blocked; 4 of those blockers had closed and 2 spec missions were dropped with their reasons in the `.fbc` header) | done: ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt, the replay run after the FIRST mission |
| **W1 Red Flag** | **10 — BUILT** | 0 | done: ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt, the replay run after the FIRST mission. **The spec called four runnable and six blocked; six of the six blockers had closed** (`C2` `C12` `C9`, and `C1` was never needed — this campaign's Red threat is aircraft). **No spec mission was dropped, folded or added**: the ten rungs are the anchor's own [T1] number and dropping one would break the ladder. It flies **no catalogue row at all**, which is how it got around `A15` |
| **W2 Osirak** | **10 — BUILT** | 0 | done: ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt. **The spec called four runnable and its own headline called it "the campaign FlightBox is furthest from being able to fly"; six of the six named blockers had closed or half-closed** (`C5` half — tank yes, boom no; `C8` `C1` `C22` `C2` `C12` `C0` fully; plus the pilot's BINGO branch becoming reachable the same day). One spec mission was dropped (`pair-pop`, `C10`) and one is new in its slot (`w2-05-tanks`). It flies **no catalogue row at all** |
| W5 Baltic QRA | **10 — BUILT** | 0 (the spec called 6 blocked; 4 of those blockers had closed and the other 2 were re-scoped in their headers) | done: ten `.fbm` + one `.fbc`, both determinism criteria measured on the first attempt, and the replay run after the FIRST mission |
| **O3 Yom Kippur** | **10 — BUILT** | 0 | done: ten `.fbm` + one `.fbc`, both determinism criteria on the first attempt, the replay run after the FIRST mission. **The spec called ZERO runnable and the count is now ten**; 3 spec missions were dropped with their reasons in the `.fbc` header, 1 was folded into two others, and 4 are new. It flies **no catalogue row at all** — the cast did not stop it, the substitution did, and it is declared in all eleven files |

Four of the ten "first pairs" are **one-line experiments**: `o1-01/02`, `w3-07/08`, `w4-01/02`,
`w5-02/03` (**all four now built**). That is the pattern this directory was written to
produce.

---

## The aggregated cast — the build order for units

Every type named in any campaign's cast table, with the number of campaigns that need it. **This is
the input to the next build step.** Ordered by frequency, then by how much the campaign loses without
it.

| Type / class | Campaigns | Exists | Note |
|---|---:|---|---|
| **F-16C** | 10 | **yes** (`f16`) | flown by W1–W5, opposed by O1–O5 |
| **MiG-29 (9-12)** | 10 | **yes** (`mig29`) | flown by O1–O5, opposed by W1, W3, W4, W5 |
| **`target_soft`** | 10 | **yes** | vehicle parks, radar vans, artillery positions, coastal sites, range pits |
| **`target_hard`** | 10 | **yes** | bunkers, shelters, bridges, a reactor dome, a runway |
| **Ground radar / EW / GCI site as an EMITTER** | 7 | **no — specified** | today `target_soft` that does not radiate; needed by W2, W3, W4, W5, O1, O2, O5. Row `p18` in [`../modules/ground/catalogue.md`](../modules/ground/catalogue.md). **As a row it only radiates and dies; what makes it worth attacking is `C22`** ([`../air-defence-network.md`](../air-defence-network.md)) |
| **AAA / short-range and man-portable air defence** | 6 | **no — specified** | the reason for every altitude decision in W2, W3, W4, O3, O5, W1. Rows `zsu23` `zu23` `sa7` `sa18` |
| **SAM battery, fixed (SA-2 / SA-3 class)** | 5 | **no — specified** | W3, W4, O1, O3, O5. Rows `sa2` `sa3` |
| **SAM battery, mobile (SA-6 / SA-8 class)** | 5 | **no — specified** | W3, W4, O1, O3 — and in O3 it is **ours**, which the design must allow (it does: a site declares its own `team` like any unit). Rows `sa6` `sa8`; mobility is expressed in TIME (`set scoot_s`), not in space — `C14` stays open |
| **AEW aircraft (E-3 / E-2C class)** | 4 | **no — specified** | W1, W3, W4, O1. Rows `e3` `e2c` in [`../modules/air/catalogue.md`](../modules/air/catalogue.md). **The most dangerous row in that catalogue**: it sees 400 km and tells somebody, so the ground net's rule applies verbatim — *the cue moves an antenna, it never creates a track* — and its one price is a second comms slot on the receiving fighter ([`../modules/air/module.md`](../modules/air/module.md) §Spec 7) |
| **F-15 class** | 4 | **row built, and it cannot shoot** | escort in W2/W3, opposition in O1/O5 — the aircraft that historically shot down every MiG in O5's two anchors. Row `f15c`, tier **T4**, `ACCEPTED` as a flight model. **MEASURED by O5 2026-07-29: `modules/air/FBAirModule` composes no fire control, so `FBState::FireControl` is never written and none of `FBPilot`'s three employment gates ever opens.** An `f15c` with four AIM-120 designates at 18.64 nm, holds a firm lock for 28 s down to 8.8 nm and never presses. O5 flies F-16s for the escort and says so |
| **Period Soviet types (MiG-21 / MiG-23 / MiG-25 / MiG-17 / Su-7 / Su-20)** | 3 | **no — specified** | W3, O1, O3 (+W2). **Five rows on one class**, not one module family: `mig21` `mig23` `mig25` `mig17` `su7` `su22` — and their differences are exactly the five decisive quantities (the MiG-21's sourced ±30°×±10° radar field is a quarter of an F-16's sky; the MiG-25's sourced **+4.5 g** limit means it loses every turning fight it enters) |
| **Tanker (KC-135 class) + a boom** | 3 | **no — specified, minus the boom** | W1, W3, W4. Row `kc135`, a kinematic mover with no weapon — **and `C5` is untouched, so it cannot give fuel.** It is also O1's Boeing 707 ECM aircraft for free: same airframe family, `set jam_comm_m` (`C24`), **zero new rows** |
| **Cruise missile / one-way vehicle** | 3 | **no** | W3, O5, O2 — already on the roadmap as R7 |
| **Runway / airfield as a STATEFUL, closable object** | 3 | **no** | W1, W3, O5 |
| **Large subject aircraft (bomber, ELINT, transport)** | 2 | **no — specified** | W5, O2 — the actual subject of a real intercept, and a completely different intercept geometry from a fighter. Rows `tu95` `an26`; the Il-20 ELINT maps onto `an26` because an ELINT aircraft **receives** and FlightBox has no ESM emission to model. Their decisive quantity is their **span** against the eye's resolution law (a Tu-95 is recognised at ~7× a MiG-21's range, [DERIVED]) — and the eye is the one sensor whose per-row input the catalogue sources completely |
| **Anti-radiation shooter (F-4G / F-16CJ with HARM)** | 2 | **module yes, weapon no** | W3, W4 |
| **Jammer aircraft (EF-111 / Boeing 707 class)** | 2 | **no — specified, and it never needed an airframe** | W3, O1 — in O1 it is the **decisive** mechanism of the whole battle. `C24` makes comms jamming a published **scalar on any unit** (`set jam_comm_m`), so an F-16 stands in for the 707 without `C7`; the radar-jamming half still has no representation. Row `ef111` exists only because its **flight profile** differs in kind (supersonic, swing-wing, at strike speed), which is precisely what a mover row carries |
| **Moving ground column** | 2 | **no** | W4, O3 |
| **Ships** | 2 | **no** | W5, W3 |
| **Helicopter (Mi-8 / AH-64 class)** | 2 | **no — specified** | W3, O3. Rows `mi8` `ah64`, movers (a rotorcraft is outside the deck recipe's fixed-wing set entirely). **The one thing they do that decides is free**: at 240 km/h = 66.7 m/s their radial rate sits inside the Doppler notch of every radar in the tree at any aspect but nearly head-on, so a helicopter is a visual and infrared target and not a radar one, without a line written for it |
| **Radar decoy (ground, emitting, not lethal)** | 1 | **yes, and it cost nothing — MEASURED by W4 2026-07-29** | W4 — named by the anchor as a decisive Serbian measure. `cast.md` costed it at *"the `p18` row with `rounds 0` and a small range gate"* and **both halves were wrong**: a `p18` already has `Channels 0`/`RoundsDefault 0`, so `rounds 0` is unnecessary; and a small range gate is not declarable and not needed, because what makes the decoy work is that it is IDENTICAL to the node — the receiver's power law is `1 − (r/2R)²`, so at equal gates the loudest admissible symbol is the NEAREST one. Measured: two AGM-88 into one decoy at 0.019 m and 4.75 m, the node and both batteries intact, `net CUE` 26 against 4, belt expenditure 7 launches against 4. The limits are the seeker's binary `arm_class` sort and the audibility floor `alt/tan(15°)` = **17.1 km at 15 000 ft** |
| **RPV / expendable decoy air vehicle** | 1 | **no** | O1 — the operation's opening move |

**Reading of that table.** After the two flyable jets and the two ground-target kinds — all of which
exist — the next four rows are the same thing four times: **something on the ground that emits and
shoots.** Nothing else in the list appears in more than five campaigns. The unit build order is
therefore not a list of aircraft; it is one system.

**And the rows below them are the same thing eighteen times.** `C7` was specified on 2026-07-28 the way
`C1` was: **not** as a list of airframe projects but as **one parametric class with eighteen catalogue
rows** ([`../modules/air/`](../modules/air/INDEX.md)). Ten rows fly a JSBSim deck generated from one
recipe against eight published anchors; eight move kinematically, because their manoeuvre decides
nothing and their drag polar was never published. The pilot is staffled in five tiers rather than built
once. **What that does NOT do is make the opponents equal** — a generated deck carries the linear
aerodynamic range and stops at the α limiter, so a catalogue fighter is most faithful in the BVR arena
and least faithful in a knife fight, which is the same staggered scale
[`../vision.md`](../vision.md) already declares, one level down.

**And O3 is where the `ALPHA` verdict cost the most.** Five of its cast rows (`mig17` `su7` `su22`
`mig21` `mig23`) are the actual force of 6 October 1973, and because an `ALPHA` row may not answer a
campaign question O3 flies the MiG-29 in BOTH of the anchor's two layers — the low fighter-bomber and
the top cover — which erases the layer difference the anchor's own sentence is about. Its files say so
eleven times.

---

## The aggregated capability gaps — the build order for the engine

The shared catalogue used by all ten files. Ordered by **blocking degree**: first how many campaigns
it *blocks* (a mission cannot run or cannot be read), then how many it *degrades*.

**Home files.** Five of these got one in the foundation round (2026-07-28), and four of the five are now
CLOSED: `C2`, `C12`, `C3` and `C0` were specified and then built, each against its own file's Spec.
`C1` was specified and then BUILT on 2026-07-28, and is now CLOSED: the contract, the nine sourced
catalogue rows and the rest of the cast live in [`../modules/ground/`](../modules/ground/INDEX.md). It
stays open in this table until it is measured against its own ten acceptance criteria.

**The net above the positions.** `C1` specifies **one** position and is deliberately silent about what
happens between positions — its own catalogue says a search radar "cues nobody … a target and a warning,
not a network". That layer is [`../air-defence-network.md`](../air-defence-network.md), specified
2026-07-28: the cue that aims a fire unit's antenna, the declared and judged belt, the fire-control
authority and what remains when the net is taken away, plus the bounded jamming model that makes the
last question measurable. It booked `C22`/`C23`/`C24` below and split `C13`; all three are CLOSED as of
2026-07-28 and the radar half of `C13` stays open.

**The air half.** Both of those files end on the same sentence — *the ground can shoot back long before
the air can shoot first*, and *the counterweight is `C8` and `C4`*. That counterweight is
[`../air-to-ground.md`](../air-to-ground.md), specified 2026-07-28: the anti-radiation weapon, the five
other stores, what a fighter radar honestly does against dirt, the difference between suppressed and
destroyed, and the pre-existing defect (a bunker and a falling bomb both radiate a fighter radar) that
has to die first. It gives `C8` a home and books `C25`/`C26`/`C27`. **Spec only, nothing built** — and
its own headline is that the air gains a weapon which needs no sight, not sight: after it, a position is
still *heard* and not *seen*.

**The cast in the air.** Those three files together give the ground half of every campaign a threat, a
net and a way to be beaten down. The other half of every cast table is aircraft, and it is
[`../modules/air/`](../modules/air/INDEX.md), specified 2026-07-28: the third level below the module
(one parametric class, eighteen catalogue rows), the two-part test that decides whether a row flies on
JSBSim, the recipe that generates the decks that do, five pilot tiers instead of one, and — the sharpest
line in it — **the early-warning aircraft**, which is where a perception boundary falls by accident and
where `../air-defence-network.md`'s rule (*the cue moves an antenna, it never creates a track*) has to
carry with the sender airborne. It gives `C7` a home. **Spec only, nothing built**, and its own headline
is that presence is not parity: it also carries the acceptance test that says when a campaign result is
publishable at all.

| ID | Gap | Blocks | Degrades | Verdict |
|---|---|---:|---:|---|
| ~~`C1`~~ | **CLOSED 2026-07-28.** Nine air-defence positions, one class, one catalogue: `modules/ground/FBSiteModule` + `core/FBSite.h`. They find with a rotating acquisition set, track with a second antenna that radiates AT THE SAME TIME (`FBUnitSignature` now carries two beams), gate on the four envelope numbers plus the round's own endurance, wait out a sourced reaction time and fire a doctrinal SALVO out of a finite MAGAZINE that has to be reloaded — through the fire-control state machine on the command bus, never a scripted release. `emcon hold` comes up on the position's own passive receiver, `scoot_s` puts it dark again. Eight proof missions, `sam-*.fbm`. Home: [`../modules/ground/`](../modules/ground/INDEX.md). **The layer above it is `C22`** ([`../air-defence-network.md`](../air-defence-network.md)) | — | — | six campaigns get their threat; what they still do NOT get is a way to shoot FIRST — **now specified** as `C8`/`C25`/`C26`/`C27` in [`../air-to-ground.md`](../air-to-ground.md) and still unbuilt — and no pilot reaction to a SAM (`C1`'s own G11) |
| ~~`C12`~~ | **CLOSED 2026-07-28.** The vocabulary is eight kinds: `identify`, `protect`, `no_fire` and `deny release` are built beside the original four — [`../missions/verdict.md`](../missions/verdict.md), grammar in [`../missions/syntax.md`](../missions/syntax.md). What stays refused (a general `deny`, `escort`, time windows, target value) is listed there with a reason each | — | — | W5/O2 can now declare the identification pass and the weapons hold, O5 the denial. O5's timing half remains a telemetry read, as its own spec says |
| `C7` | **BUILT 2026-07-28 — and STILL OPEN, because the gate it wrote for itself is not passed: `make -C sim test-air` puts 10 of 10 generated decks OUTSIDE their §7.1 bands, so every row is `ALPHA` and none is campaign-admissible. The presence half is done (18 rows, 10 decks, 8 movers, 5 tiers, the early-warning boundary, both attribution instruments); the FIDELITY half is measured and named. Was:** Only two flyable modules today; every other aircraft in every cast list is absent. The contract is **one parametric class with eighteen catalogue rows** — `modules/air/FBAirModule` + `core/FBAircraft.h` — home [`../modules/air/`](../modules/air/INDEX.md). **The central decision:** a row flies on a GENERATED JSBSim deck iff *(its own manoeuvre decides an outcome)* ∧ *(its envelope is published)* — ten fighters get one, eight large or rotary aircraft get a kinematic mover, and the test never splits a row because fighter data IS envelope data. The deck comes from ONE recipe against eight anchors with a closed-form drag inversion, and its acceptance bands are **derived from the MiG-29 deck's own measured misses** rather than chosen. Pilot is **staffled in five tiers**, and a tier is a declared task set plus the hooks a row's own MEASURED deck supplies. Cost to the rest of the tree: 7 store rows, 7 generated missile decks, 6 gun rows, 2 `set` keys, 1 sensor derivation — and **zero** new seeker kinds, emitter kinds or health ids | **1** (O3's period force) | 9 | blocks nothing outright because substitutions exist — but every substitution changes the answer, and each is declared in its mission header. **What the spec adds beyond presence:** an acceptance test that separates *"he lost as a MiG-21"* from *"he lost as a coarse deck"* (`band_deck ≤ 0.25 × band_doctrine` on the tournament instrument [`../duels.md`](../duels.md) already runs), so a campaign result is publishable or explicitly is not. **It stays open until every row is measured against its own bands** |
| ~~`C2`~~ | **CLOSED 2026-07-28.** `time <ISO8601 Z>` is mission data, the clock binds all three clients, `FBEphemeris` sits in `core/` and `fb-gym` publishes `FBEnvironmentBlock` — [`../missions/syntax.md`](../missions/syntax.md), [`../clients/clients.md`](../clients/clients.md) | — | — | a night mission can now say so |
| `C6` | **No live controller.** GCI is `set brief_gci`, static text fixed before the run: nothing re-vectors, nothing goes silent mid-intercept, nothing is wrong *halfway through*. **Its GROUND half is specified in [`../air-defence-network.md`](../air-defence-network.md)** — a node that can be killed, jammed or fall out of range mid-run; the AIRBORNE half (a jet subscribing to a controller) is untouched and is that file's §2 design B | **2** (O2's subject, W2-07) | 6 | **MEASURED BY W2 2026-07-30, and it turned out to be the binding constraint where `C4` was expected to be.** `w2-07` asks whether a radar blind spot is expressible: the early-warning node holds the raid from **294.6 km** and sends 12 `net CUE` to the only receiver that exists — a 2.5 km gun 200 km away — while the two fully briefed MiG-29 log `fcr_contacts` **max 0.0 over 700 s**. **There is no channel from a radar to a fighter to be blind IN**, which is a different absence from terrain masking and a larger one. | the difference between "blind" and **"confidently blind"** — see [`o1-bekaa-1982.md`](o1-bekaa-1982.md) §Knowledge 4, the cheapest addition that would raise O1 from a stand-in to the real experiment |
| ~~`C9`~~ | ~~**The MiG-29 module cannot fly `set task attack`**~~ — **CLOSED 2026-07-30, and O3 FLEW ON 2026-07-30: thirty FAB-500 delivered by the OPT director at `aimErrM` 46.4-94.0 m.** The director is built (`core/FBDirector.h`, `modules/mig29/FBMig29Director.*`) as a director and **not** as a release cue: the aircraft picks the moment, the pilot flies an instruction. Proved by the one comparison that could expose a shortcut — same geometry, same store, F-16 CCRP **34.02 m** against MiG-29 OPT **65.65 m** (1.93×, `missions/mig29-opt-low.fbm`). A director that came out *better* than a computer would have been a release cue with Cyrillic labels. Refusal fires as its own case (`mig29-opt-refused.fbm`: the marginal run releases at 102.9 m, the refused run never releases). Byte-identity held — a MiG-29 without an attack task does not move, same column count | 0 | 2 | O3 is no longer zeroed **at the module**; what still blocks it is its cast (the period Soviet types are `ALPHA` and may not answer a campaign question) |
| `C5` | **HALF CLOSED 2026-07-30: the external fuel tank is built, aerial refuelling is NOT.** `tank370` is a catalogue row with `FuelLbs > 0` that owns one of the airframe's own JSBSim tanks while it hangs there, draws external-before-internal on JSBSim's own priority, and takes its mass, its drag and its fuel with it when it goes; `set fuel_int_pct` makes a CLEAN jet declarable for the first time. Home: [`../modules/stores.md`](../modules/stores.md). **The boom is the expensive half and is deliberately unbuilt** | 0 | 3 | **MEASURED BY W2 2026-07-30, and the half that is missing has a number now.** The tanks are worth **+45.6 % of range clean and +50.5 % under the war load**, dropping them when dry another **7.1 %**, and the externals run dry at **675.8 km** against the anchor's *"about 1,000 km"* [T4]. And the raid is still not flyable: **874.4 km of war-load radius against 982.9 km needed, 108.5 km short**, which is exactly the size of hole a tanker fills. What no W2 number can say is what one would have been worth |
| ~~`C3`~~ | **CLOSED 2026-07-28.** `sensors/FBVisualSystem` is built and is the SIXTH registry reader, declared in advance and paid for in five currencies — [`../sensors.md`](../sensors.md) §9, mission switches in [`../missions/sensors.md`](../missions/sensors.md). Recognition is the resolution test it was specified to be: measured, a beam-on F-16 is detected at 3 784 m, recognised at ~950 m and identified at ~590 m, and the name it gains is the module registry key. **`w5-03`/`o2-08` survive by measurement, not by argument**: two runs differing only in the target's `team` produce byte-identical telemetry | — | — | what is NOT closed: nothing consumes the block yet (deliberate, its own round in [`../pilot.md`](../pilot.md)), and the channel contributes nothing at night because nothing in the tree emits light — so W5's and O5's night merges are measured to be eyeless |
| `C8` | **BUILT 2026-07-28, except the rocket pod.** Six rows exist and fly: `agm88` `mk84` `gbu12` `cbu87` `fab250` `fab500`, with both new `FBSeekerKind` values and the FAB release envelope as `FBStoresSystem::Release` check 8. Twelve proof missions (`arm-*`, `mk84-radius`, `mk82-radius`, `cbu87-footprint`, `lgb-*`, `fab-envelope-*`). **STILL OPEN: `hydra70`/`s8`** — the pod is a MAGAZINE on one station (design C, [`../air-to-ground.md`](../air-to-ground.md) §3.5) and neither the field group nor a model was built | **2** (W3/W4 SEAD, O3 stores) | 1 | there is no such thing as a suppression element in the tree. **The contract's own headline:** the anti-radiation seeker is the RWR, so it has a bearing and never a range — which makes the real AGM-88 memory mode unbuildable here and the crew's shutdown a real countermeasure, on a derived law rather than a setting |
| `C25` | **NOT BUILT.** No air-to-ground radar function. `FBRadarSystem` filters `FBUnitKind::Aircraft` ([`../sensors.md`](../sensors.md) gap 3), so no aircraft radar ever finds a ground object; and the fire control samples the elevation provider only at the briefed steerpoint. **Specified 2026-07-28 in [`../air-to-ground.md`](../air-to-ground.md) §4 as RANGING ONLY** — one slant range and one point where the commanded antenna line meets the terrain, never a contact, never a track, never a map. The `Aircraft` filter stays | 0 | **4** | the two radars are very unequal here (APG-68: GM/GMT/DBS/SEA/FTT; N019: **nothing**) and the file states the inequality as a **result**. Two consequences fall out for free: mapping RADIATES, so it wakes an `emcon hold` site, and every `--elev const` mission is byte-identical because a flat plane is a flat plane |
| ~~`C26`~~ | **CLOSED 2026-07-28.** Both halves built: `set emcon react` (the crew goes dark for `scoot_s` on its own health register's next hit — proof `sam-emcon-react.fbm`) and `objective suppress unit\|team [emitting <s>]`, deferred, roster price exactly one bool (`FBUnitObservation::Emitting`) — proof pair `suppress-quiet.fbm` / `suppress-killed.fbm`. `CombatEffective` untouched. **One value beyond the spec:** `set emcon` also takes a briefed emission PLAN (`free <offS> [<onS>]`), without which the escape window is unmeasurable — `scoot_s` needs a launch and `react` needs a hit, so neither can be placed in time | — | — | the honest limit is named with it: the crew reacts to damage, i.e. AFTER the round arrives, so FlightBox's anti-radiation weapon is harder to defeat than the real one |
| ~~`C27`~~ | **CLOSED 2026-07-28.** One gate and one `attack_mode` value: `arm` reads the RWR block like any other instrument and presses when a threat of the declared `arm_class` sits inside the round's own seeker cone. No range is learned because none exists. Proof `arm-pilot-cue.fbm`. Everything above it — a turn onto a threat bearing, a reaction to a launch, weapon selection, re-attack — is still the pilot's own round | 0 | 2 | **the weapon is measurable before the pilot learns anything**, which is why this is a separate ID and a separate round |
| `C15` | **No package coordination** — no time-on-target, no deconfliction, no lead tasking; formation is combat spread only, no rejoin | 0 | **7** | **MEASURED by W2 2026-07-30 at eight-ship scale, and the finding is that it WORKS and nothing maintains it**: the capstone's four pairs are spaced **1,029 m along track = 5.00 s at the commanded 400 kt**, arithmetic the author did in a comment, and the releases come out at **290.8 / 295.8 / 300.8 / 305.9 s** — 5.0 s apart, four times, to the tick. That is what `C15` means. Also the *definition* of a package, and every large-force mission is affected. **MEASURED by W5 2026-07-29 at the two-ship scale:** with three contacts the flight's only free variable is the sort, it ranks by time-to-arrive, and a deliberate split collapses onto the nearest contact in **1.0 s** (`w5-04`); with two subjects 17.8 km apart, one on each member's radar, **both** members report `track=1 free=0 dup=1`, because `FBFlightPicture::Assign` matches every member against the *computing* aircraft's own contact list (`w5-06`) |
| ~~`C0`~~ | **CLOSED 2026-07-28.** The `.fbc` file, the three carried facts and the aggregating runner are built — [`../missions/campaign.md`](../missions/campaign.md), driven by `fb-gym --campaign`. Both determinism criteria measured on both ground bases: 9 runs one campaign fingerprint, and all 4 steps of `sim/campaigns/viper-attrition.fbc` reproduce standalone from their state file plus the ground the run recorded | — | — | a sequence of missions is now a campaign. What is NOT closed: the ten campaigns still have no `.fbm` files, damage and fuel stay refused with a reason, and there is no campaign-scope objective |
| `C4` | **No terrain masking.** The hook (`const FBWorld*`) reaches every sensor slot; the computation does not exist | 0 | 5 | already named as next in [`../roadmap.md`](../roadmap.md) R6. W4 and O1 are its acceptance tests. **W4 FLEW ITS ACCEPTANCE TEST 2026-07-29 and it is a NEGATIVE:** every W4 header carries the disclosure and its consequence for that file's claim, and the capstone states that it stages **three** of its four antagonists. The one thing that CAN be measured today was: two runs of `w4-01` under `--elev tiles` over the real Kosovo (ground 547.88 m against a 0 m plane) move `predErrM` **58.08 → 46.50 m** and produce **zero masks** — the missing half is a computation, not data, so closing `C4` cannot be done by improving the DEM |
| `C14` | **No moving ground units and no ships** | 0 | 4 | W4's armour hunt, O3's column, W5's Baltic. **MEASURED by W4 2026-07-29**: `w4-09` is a hunt for PARKED vehicles and reports an upper bound — 4 of 4 released, `aimErrM` 69–79 m in 20 kt of crosswind and **0 of 6 killed**, against **3 of 6** in the same file with `wx calm`. A column that moved would have moved during the 33 s the bomb is in the air |
| `C18` | **No radio between units.** Only the datalink PPLI and typed GCI entries. The **ground-to-ground** half is specified as `C22`; what stays open is the VOICE net — call volume, a channel that saturates | 0 | 4 | Package Q's third failure mode was a radio net collapsing under 80 % of the calls; there is no such net to collapse |
| ~~`C22`~~ | **CLOSED 2026-07-28.** An early-warning set cues a fire unit: the node publishes a POINT its own antenna measured, the member types it into its own antenna over the command bus (two entries, 0.5 s each) and must still find, firm and gate the target with its own radar. Sector responsibility, a `free`/`tight`/`hold` fire-control authority transmitted by the node, a declared `autonomy` fallback, and a node that can be killed, jammed or fall out of range **mid-run**. Home: [`../air-defence-network.md`](../air-defence-network.md) | — | — | *Is killing an early-warning radar worth a sortie?* MEASURED: the same geometry with and without the cable is `2` `site LAUNCH` against `0`, and `site RADIATE` at t=8.0 s against never (`net-cue.fbm` / `net-cue-unnetted.fbm`). What is still NOT measurable is the cue as a DETECTION advantage — that needs `C4` |
| ~~`C23`~~ | **CLOSED 2026-07-28.** `zone <name> <lat> <lon> <radiusM> <altMinM> <altMaxM>` declares a cylinder, `objective avoid zone <name> [exposure <s>]` judges it, and `zone_<name>_in`/`zone_<name>_s` measure it per judged unit. `core/FBZone.h` is RESTRICTED with an EMPTY outside-includer list — a NARROWING: no module, no pilot and no sensor can name it | — | — | MEASURED (`net-belt-low.fbm` / `net-belt-high.fbm`, identical route, altitude the only difference): low = 34.5 s in `flak` / 0.0 s in `sambelt` / 54 gun bursts / 0 SAM launches; high = 0.0 s / 320.0 s / 0 bursts / 1 launch. The altitude that escapes the AAA does put you in the SAM |
| ~~`C24`~~ | **CLOSED 2026-07-28.** `set jam_comm_m <rangeM>` on any flying unit: one published scalar, receiver-side, other teams only, a distance test with no die. It is INAUDIBLE (no emitter signature, so no home-on-jam) and it denies the LINK and nothing else — the `site TRACK` line of a jammed run is byte-identical to the unjammed one's. A `wire` link is not jammable | — | — | MEASURED: `net-jam-late.fbm` loses the node **mid-run** (`net LOST reason=jammed` t=128.0, `net AUTONOMOUS fallback=hold`, 0 launches after having radiated since t=8.0) against `net-jam-start.fbm`, jammed from t=0 (0 JOIN, 0 CUE, 0 RADIATE, 0 launches). Blind against confidently blind, and the second one paid with its position |
| `C10` | **No dive or pop-up delivery.** The attack phase is a level laydown, by design and for a stated reason | **1** (W2's profile) | 3 | **MEASURED BY W2 2026-07-30 as the width of a band.** A `target_hard` fails inside **17.7 m** of an Mk-84 [DERIVED, `2.81e7/r²` against 9.0e4 J/m²]; over seventeen level releases `aimErrM` runs **6.36–50.83 m**, 96–99 % of it ALONG track, and `predErrM` is ground speed × a constant **0.228–0.241 s**. So the dome is killable at a RATE — the capstone puts **5 of 8** inside the radius — and the anchor's 35° dive is the mechanism that removes the band rather than a stylistic difference |
| `C17` | **One runway per mission; no divert field; a runway cannot be cratered or closed and an airfield has no state** | **2** (O5-07, W2-04) | 1 | **W2 2026-07-30:** `w2-04-loaded` runs its tanks dry **179.0 km short of Etzion** with nowhere else declarable, which is the correct answer to its own question and a poorer one than the real map would give. | **O5 dropped that mission rather than build it** (`.fbc` header): it would be a `target_hard` nothing in the package can kill whose death changes nothing. It also costs O5's sortie 10 the word *recovered* — "combat-effective at the end" is not "landed" |
| `C13` | **No RADAR jamming.** **SPLIT 2026-07-28 and halved:** the communications half is `C24` and is CLOSED; what stays wholly open here is the RADAR half — noise, deception, range-gate pull-off, angle-of-jam, burn-through, home-on-jam | **1** (O1's mechanism) | 1 | the decisive Israeli move at Bekaa is the one thing in the set with no substitute at all |
| `C16` | **Cloud affects only the EYE** (the `C3` round moved it there from the IRST) — not radar, not weapon delivery, not a decision to bring the bombs home; the cloud rebuild is unbuilt | **1** (W4's spec mission 8, dropped for it) | 1 | roadmap R5. **MEASURED BY W4 2026-07-29 on both sides:** in a strike file the deck has NO consumer at all (`w4-02`: 0 `vis` lines, and the whole 216 m of extra delivery error is wind, priced at **5.014 m per knot** on a six-point ladder); in the one geometry where an eye looks THROUGH the deck it closes the path completely (`w4-07`: `vis MASKED … transmittance=8.00571e-13` between an aircraft at 5 150 m and one at 8 000 m). `irst_masked` stays **0** in all ten files. **So a "bad weather campaign" in this tree is a WIND campaign**, and W4's spec mission 8 was dropped rather than flown as a measurement of an absence |
| `C19` | **No rules-of-engagement state** — nothing expresses weapons hold / tight / free. The vocabulary is now defined and gated **for ground units** in [`../air-defence-network.md`](../air-defence-network.md) §5; aircraft still have none | **2** (W5, O2) | 0 | **MEASURED by W5 2026-07-29 and it is worse than "no state": there is no BEHAVIOUR to restrain.** `w5-07` puts two armed F-16s with the master arm on under a fighter radar for 300 s and gets zero releases — with `eng_state` = `idle` for all 3 001 rows, because a pilot on `set task route` never enters the engagement machine. The alternative task does not become trigger-happy either: `set task intercept` ABORTS the identification geometry (`FBPilot.cpp:1040`, *inside 5.0 nm and never shot*) at t = 5.1 s |
| `C20` | **No terrain-following guidance.** `Direct` holds an ASL altitude, so a 30 m AGL ingress over varying terrain is not flyable | **1** (W2-02) | 1 | **MEASURED BY W2 2026-07-30 and it is not a tracking-error gap, it is a boot failure.** Over the campaign's own 0 m plane the guidance holds 240 m to **0.85 m over 300 km** and 30 m just as well; over the real Jordan and Iraq under `--elev tiles` the mission does not reach tick one — `reason="spawn altitude is below ground" altM=240 groundM=487.48` — and the route's ground reaches **1,599.22 m**. **Neither half of the anchor's [DISPUTED] ingress altitude is flyable over the ground the raid crossed** |
| `C11` | **No strafing** — gun bundles are not resolved against ground targets | 0 | 2 | |
| `C21` | **No declarable initial damage** — a jet can be switched off, not broken | 0 | 2 | |

### The foundation round (2026-07-28) — what it decided

Step 1 of the owner goal was a **spec round, no code**: the contracts for `C2`, `C3`, `C12` and `C0`
were written into the files that own them, and `C1` was given a home with a boundary. The decisions
worth knowing before reading them:

| Contract | The decision |
|---|---|
| `C2` | **Zulu only, and the default is *no clock at all*.** The absent-value case touches no channel, which is what makes the 84 existing missions byte-identical by construction rather than by hope. A client flag that contradicts a `time` line is a **boot error**, not a precedence |
| `C3` | **Recognition is a resolution test, not a lookup.** One quantity (presented extent over range) with Johnson N50 multiples for detect/recognise/identify; the type name is the module registry key, so two MiG-29s on opposite teams produce the identical string, and the anti-cheat pair survives. **Built and measured the same day; the two places the contract was wrong are corrected in [`../sensors.md`](../sensors.md) §9 rather than quietly satisfied** |
| `C12` | four kinds — `identify`, `protect`, `no_fire`, `deny release` — costing **one monotone bit and one float** on the roster. `identify` measures the **geometry**, not a sensor event, because the judge measures what the aircraft DID and never what it knew. A general `deny` and `escort` are refused, each with a reason |
| `C0` | a campaign carries **three** facts (units, ground targets, stores) and refuses the rest by a three-part test. The overlay may delete a line, never add one. Determinism is proven twice: one fingerprint over 9 runs, and each step re-runnable **standalone**. **Built the same day; the overlay ended up narrower than the contract allowed — it only ever deletes** |
| `C1` | **one data-driven class, nine catalogue rows** — not nine classes and not a module. It sees through **derivations** of bases that already read the registry, so the gate stays at six files; it **uses** `FBSystemHealth` unchanged rather than inheriting anything; it asks for exactly **one** core change (two emitter beams per unit, because a battery is two antennas) and **one** new enum value on each of `FBSeekerKind` (command guidance) and `FBEmitterKind` (surface search / surface fire control). `emcon hold` is a **passive receiver**, never a timer or a range trigger |

### The four things to build first, and why in this order

1. **`C1` — an emitting, shooting ground unit. SPECIFIED 2026-07-28 in
   [`../modules/ground/`](../modules/ground/INDEX.md), not built.** It blocks six campaigns, it is the top
   four rows of the cast table collapsed into one system, and it is the only gap whose absence removes the
   *reason* for tactical decisions rather than the ability to make them. Without it W2, W3, W4, O1, O3 and
   O5 are air-to-air campaigns wearing strike names. **The spec's own honest headline:** a site can be
   *heard* and not *seen* (no air-to-ground radar mode, no HARM — `C8`), so `C1` gives the ground the
   ability to shoot back long before it gives the air the ability to shoot first. That is the right order
   to build in and it is not a SEAD capability.
2. ~~**`C12` — three more objective kinds.**~~ **BUILT 2026-07-28** (four kinds). Small, self-contained,
   and it makes five campaigns *readable*. `deny`/`protect` is checked against the roster the `kill`
   objectives already use; `identify` needed a definition, and W5/O2 supplied one.
3. **`C2` — a mission clock.** One line in the format. It mislabels more artefacts than anything else
   in the list, and the renderer already has the ephemeris to consume it.
4. **`C6` — a controller that can change or vanish during a run.** It converts O1's stand-in into the
   real experiment, gives O2 its subject, and is the shared mechanism behind the one experiment three
   separate campaigns want to run (below).

### The precondition none of the ten can be flown around: the arena must not be saturated

Specified 2026-07-29 in [`../doctrine-evolution.md`](../doctrine-evolution.md) §4, and it comes **before**
these campaigns rather than after them. The measurement that forces the order:
[`../modules/air/module.md`](../modules/air/module.md) §State B6 — on the `mig21` attribution geometry,
**18 of 19 runs return the identical outcome** (−1450.0), `band_deck` = 0.0 and the control cell moves
nothing. On such a geometry the Bekaa yardstick below is not small, it is **undefined**: `band` is the
spread of a constant and `residue` is the difference of two constants. A campaign flown there produces a
verdict no lever on either side could have changed, which is a fixed point wearing a result's clothes.

The criterion, with its numbers: per geometry the modal outcome class ≤ **60 %** of runs and ≥ **3 of 9**
doctrine levers must move the outcome class; per arena ≥ **6** geometries of which ≥ **3** informative, no
two asking the same question — 6 because the duel matrix's own informative rate is a measured **2 of 4**.

**BUILT AND MEASURED 2026-07-29** (`sim/tools/fb_arena_check.py`), and the answer moved the arena rather
than the criterion:

| arena | geometries | informative | verdict |
|---|---|---|---|
| the one all published F-16 doctrine results were measured on (`mirror`, `split`) | 2 | 1 | **REFUSED** — and `mirror` itself is **100 % modal with 1 of 9 movers**, worse than the `mig21` cell that forced the criterion |
| the rebuilt one (aspect × energy × detection × **weapon obligation**) | 8 | **4** (`far`, `split`, `xmirror`, `xclose`) | **PASSED** |
| **a CAMPAIGN's own ten missions, 2026-07-30 — [`w1-red-flag.md`](w1-red-flag.md) §State** | 10 | **2** (Blue seat `w1-03`/`w1-10`; Red seat `w1-06`/`w1-09`) | **REFUSED** — modal share 55.6–100 %, doctrine movers 0–5 of 9, `S6` clean, `S3` n/a. 180 runs, twice, byte-identical |

**W1 is the first campaign whose own missions were put through the gate, and it failed it.** The
denominator had to be named, because the gate's instrument cannot be pointed at hand-authored files: per
geometry the population is the **nine declared doctrine levers** applied to one seat — the population `S2`
is defined on — not the six-variant yardstick field, so `S1`'s threshold is coarse (with n = 9, *"≤ 60 %"*
means *"≥ 4 runs outside the modal class"*). Two things fall out and they belong to all ten campaigns:
**the levers bite in opposite seats on opposite rungs** (Blue's on the 1v1-scale ones, Red's on the
multi-ship ones, which is `duels.md`'s own asymmetry), and **only the three rungs at 1v1 and 2v1 decide
anything at all**. Every capstone in this directory is 4v4 or larger; their air halves are therefore fixed
points and only their ground halves grade.

**What desaturated it was not a geometry, it was the AIRFRAME.** Eleven F-16-vs-F-16 candidates were
flown and every one failed; the three best cells in the passing arena are the two that put a MiG-29 in
the east seat plus the long approach. Two identical aircraft carrying the same weapon draw, and no
geometry fixes that — which is [`../duels.md`](../duels.md)'s own finding arriving at the campaign layer.

`band_deck ≤ 0.25 · band_doctrine` (S3) is **not computable on that arena** and the check says so rather
than printing a zero: its instrument perturbs a GENERATED catalogue deck, and the F-16 and the MiG-29 are
FlightBox's read-only model copies under principle 1. S3 stays live for a catalogue-row arena, where it
was defined.

The same file also replaces the fitness the yardstick is computed with: it is lexicographic
(verdict → objectives met → craft), **built as `sim/tools/fb_fitness.py`**, because the present weighted
one demonstrably ranked the doctrine that abandons the sort above the one that keeps it
([`../formation.md`](../formation.md): `f16_solo` 1097.8 against `f16_net` 977.1, both 0 kills and 0
losses). Re-measured under the new order that gap is **0.9 points of `shot lead` on an exact tie** — the
mechanism is gone, the direction is not, and the reason is that `mirror` decides nothing at all.


---

## The identification task (W5 and O2)

Two campaigns have a task with **no weapon in the success condition**: put eyes on an unknown
aircraft and say what it is. They are here because identification is the sharpest available test of
the anti-cheat boundary — and, unusually, a test that costs almost nothing to run.

### Why it is sharp

An AI that cheats about *shooting* is hard to catch: a fast, accurate kill can be good flying. An AI
that cheats about *identity* is caught in one diff, because the correct behaviour toward an unknown
contact is **the same regardless of what it turns out to be**, right up to the moment a sensor
discriminates. Any earlier divergence has exactly one possible source.

The tree's own restrictions make the test possible, and every one of them helps:

| Restriction | Where enforced | Consequence for the test |
|---|---|---|
| A radar contact carries range, bearing, azimuth, elevation, closure and a radar-local track number — **no unit id, no callsign, no team** | `core/FBRadarContact` | there is no legitimate path from a contact to an identity |
| IFF Mode 4 is **two-valued**: friendly, or no reply. There is no "hostile" value | `core/FBIffReply` | **silence is ambiguity**, and ambiguity is the whole subject |
| The unit registry reaches exactly six files in `systems/`+`modules/`, all of them sensors | grep-checked and pinned in `tools/verify_layers.py`, [`../sensors.md`](../sensors.md) §1.2 | the pilot has no path to the truth even if it wanted one |
| The cooperative datalink is faction-filtered and carries a **point**, never an identity | [`../formation.md`](../formation.md) §Knowledge 2 | a mate cannot leak an identity either |
| Runs are deterministic to one fingerprint over `--threads 1/2/4` × 3 repeats | [`../missions/runtime.md`](../missions/runtime.md) | a behavioural comparison is a **byte comparison**, not a judgement |

### The test, stated as an acceptance criterion

> Build each identification mission **twice**, identical in every line except the subject's `team`
> (`neutral` in one, `hostile` in the other). Require the interceptor's own telemetry to be
> **byte-identical up to the first tick at which one of its own sensors discriminates** between the
> two cases. A divergence before that tick names the tick and the column, and it is a defect of the
> same class the include graph already forbids structurally.

`w5-03` and `o2-08` are that experiment, on the two airframes. They need **nothing that does not
exist**, which is why both campaigns nominate them as the first pair to build.

**`o2-08` was flown 2026-07-29 and the criterion holds in its STRONG form.** Two runs differing in one
token (`team neutral` → `team friendly`): **5 of 5 `telemetry*.csv` byte-identical**, and `events.log`
differing in **exactly one line of 53** — `mission UNIT_RESULT … team=`, written by the RUNNER and
readable by no simulated system. Four perception channels were live for the whole 300 s (N019 plus its
interrogator, KOLS, the eye, SPO-15) and none of them moved. There is no first discriminator to be
identical *up to*, because IFF Mode 4 is two-valued and a stranger and an enemy are the same silence.

**And the pair needed a third file, which is now a rule.** A byte-identical pair has two possible causes —
no leak, or a dead channel — so `o2-07` is the control run: the same intercept with a subject that
ANSWERS. It differs from `o2-06` in exactly two log lines (`reply=none` → `reply=friendly` at the same
tick, plus the same `team=` field) and in **zero** telemetry bytes on the interceptor. The channel fires;
the pilot does nothing with it; and `w5-03` should budget the same third slot.

**`w5-03` was flown 2026-07-29 and budgeted the third slot from the start** (`w5-01` is the answering
subject, `w5-02` the silent one, `w5-03` `w5-02` with one token changed). The criterion holds in its
strong form on the western airframe too: **6 of 6 `telemetry*.csv` byte-identical, `events.log`
differing in exactly one line of 75** — the runner's own `team=` field.

**And the control run does NOT come out the same as O2's, which is the more interesting half.** On the
MiG, an answering subject moved *zero* telemetry columns. On the F-16 it moves **five of 184**
(`fcr_iff`, `flt_src`, `flt_assign`, `flt_switch`, `flt_dup`) and six log lines — because
`pilot/FBFlightPicture` sorts over a picture whose tracks carry an IFF field, and a track that answers
`friendly` is never assigned. So the comparable quantity is *"what an identity is worth **to a
flight**"*, and O2's own rule 11 catches it: neither run is wrong and the policies differ. **What holds
on both airframes: the identity moves no trajectory. Zero metres, both times.**

### What mission data must carry, and what it must not

| Must be declarable | Must NOT be declarable |
|---|---|
| the subject's `team` (already: `friendly` / `hostile` / `neutral`) | anything the pilot can read that names the subject |
| the subject's transponder state (`set iff_xpdr on\|off`) and its emission state | a "this one is the bad guy" hint of any kind, including a callsign convention |
| the interceptor's interrogator state (`set iff_interrogator on\|off`) | |
| a **geometry** the interceptor must reach and hold (the abeam box) — `[SET]`, declared per mission | |
| a rules-of-engagement state (needs `C19`) | |

### What the victory condition became (`C12`, built)

| Objective | Fulfilled when | Violated when |
|---|---|---|
| `identify unit <callsign> range <m> hold <s>` | this unit held the declared geometry for the declared cumulative duration | — |
| `no_fire` | no release and no burst by this unit for the whole run | any release or burst → immediate FAIL |

The sensor half was **deliberately dropped from the verdict**: producing "a discriminating sensor event
on unit N" means correlating an anonymous contact back to a unit, and that function's existence is the
identity leak these two campaigns exist to test. The IFF reply is therefore read out of `events.log`
(`radar IFF_REPLY … reply=none`) beside the verdict rather than inside it — the full argument, and the
price ("a pilot that flies the box with its eyes shut still scores"), is in
[`../missions/verdict.md`](../missions/verdict.md). `sim/missions/qra-identify.fbm` is the shape both
campaigns can now be written against; `qra-weapons-hold.fbm` is the same intercept with the hold broken.

### The eastern version is the harder one

On the MiG-29, identity has exactly **one** source and using it radiates
([`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md) §3): the IFF interrogator
works through the radar, the IRST has no IFF at all, and the SPO-15 warns of every radar including
friendly ones. So on the eastern side an AI that "just knows" is visible not only in its shooting but
**in its silence** — a MiG that never interrogates and is nonetheless always right is caught by the
same diff.

---

## Bekaa as the yardstick (O1)

The Bekaa campaign is the one that must not ask "does the MiG win". It asks two quantities, and the
machinery to compute both already exists and has been run once at single-ship scale.

**(a) The doctrine band.** With `O(v)` the outcome score of a Red doctrine vector `v` over a fixed
geometry set — the same score `fb_tournament.py` already computes, outcome dominating and craft only
ordering within equal outcomes:

```
band = max_v O(v) − min_v O(v)
```

**Both quantities are undefined on a saturated geometry, and the check that says so runs first**
([`../doctrine-evolution.md`](../doctrine-evolution.md) §4). The score itself is the lexicographic one
of that file's §1, not the weighted sum `fb_tournament.py` computes today.

The levers of `v` are **mission text only**: GCI present/absent, emission policy, commit range,
launch doctrine, altitude band, formation contract, reaction time. A lever that needs a new class is
not a doctrine, it is a rebuild. Precedent: the same measurement on a single MiG over one geometry
already produced a band of **978.7 points** and turned six losses into none
([`../duels.md`](../duels.md)).

**(b) The residue.** With both sides held at their best measured doctrine,

```
residue = O_blue(v*_blue) − O_red(v*_red)
```

The residue is what **no decision on either side removes** — the weapon obligation, the sensor reach,
the cross-section ratio, the four asymmetries already tabulated in [`../duels.md`](../duels.md)
§Knowledge 1. A small residue says the defeat was doctrinal and architectural; a large one says the
force was outmatched in materiel before anybody decided anything. **Both answers are publishable.
Refusing to compute the number is not.**

Three conditions on it, all non-negotiable:

1. It is a statement about **FlightBox's models**, never about 1982. The scale is staggered by design
   ([`../vision.md`](../vision.md)) and the flying type did not exist yet.
2. A loss caused by a **pilot-AI defect** is not part of the residue. The duel campaign found three
   such defects by measurement and fixed each where it belonged; a residue reported before that
   filtering is a bug report wearing a result's clothes.
3. The substitution direction is stated with the number. FlightBox's MiG-29 has a better warning
   receiver, a look-down radar and an IRST that the 1982 Syrian force did not — so a bad FlightBox
   result is a **stronger** statement than the record, and a good one is not a rehabilitation of
   anybody.

### The one experiment three campaigns share

`o1-02`, `w3-08` and `o5-04` are the **same single deleted line** (`set brief_gci`) in three
geometries whose anchors independently name ground control as the missing ingredient. FlightBox's
mechanism makes it unusually clean: without the brief the N019 never receives its scan elevation or
its ZONE third, so the aircraft is not "less informed" — it is pointing its radar at nothing in
particular, and the measured quantity is a **detection time**, comparable across all three campaigns
with no normalisation.

**If the three disagree, the answer is a property of the geometry rather than of the doctrine, and
that is itself the finding.**

**FIVE now, and W3 is the first to fly BOTH SIDES of the deciding line in one campaign.** `w3-07`/`w3-08`
declare `set n019_emission off` in advance and produce O2's answer at full strength — 50 Red radar
contacts and 5 launch solutions against **0 and 0** — while W3's attribution run A3, the same file with
that one token set to `illum`, produces O1's and O5's answer just as cleanly: 50 contacts, 7 solutions
and a run that ends at the *identical* t = 272.8 s as the briefed control. So the four earlier campaigns
were each right about their own file, the quantity is *"what the controller is worth GIVEN an emission
policy"*, and it is now measured on both sides rather than inferred.

**FOUR then, and they disagreed — and the reason is neither geometry nor doctrine.** `o2-04-no-gci` runs
the same deleted line in a fourth theatre and produces **zero radar contacts, zero emissions and an
intruder that never learns a fighter was there**, against o1-02's 8 → 4 contacts, o1-03's identical
outcome and o5-03's six seconds. The mechanism is one other line in the file: O1 and O5 spawn their MiGs
with `set n019_emission illum`, so deleting the brief leaves the antenna badly AIMED; O2 spawns them in
the documented power-up position `off`, where the brief's THIRD typed entry is the only thing in the
tree that ever turns the radar on. **The comparable quantity across the four is therefore not "what the
controller is worth" but "what the controller is worth GIVEN an emission policy",** and the three
earlier files should be read as measuring the aiming and this one as measuring the existence. The
one-line experiment is still one line; it is just not the same line in all four.

---

## Related

| Place | Relationship |
|---|---|
| [`../vision.md`](../vision.md) | the staggered scale and why anti-cheat is a game decision — the ground both cross-cutting sections above stand on |
| [`../duels.md`](../duels.md) | the 1v1 measurement campaign; O4 is largely its re-framing, and O1's yardstick reuses its tournament |
| [`../doctrine-evolution.md`](../doctrine-evolution.md) | **step 3 of the owner goal, and the precondition for these ten**: the fitness the yardstick is computed with, the five doctrine genes, the archive against circling, and the saturation criterion an arena must pass before a campaign result means anything |
| [`../formation.md`](../formation.md) | the flight as a fighting unit; every campaign with more than two aircraft a side depends on it |
| [`../missions/`](../missions/INDEX.md) | everything a `.fbm` can declare — the source of every "can" and "cannot" in the gap tables |
| [`../modules/f16/module.md`](../modules/f16/module.md), [`../modules/mig29/module.md`](../modules/mig29/module.md) | what the two flyable jets can actually do |
| [`../roadmap.md`](../roadmap.md) | R7 (one-way vehicles), R9 (missions for humans — the nearest existing home for `C0`) |
| [`PROGRESS.md`](PROGRESS.md) | the source-coverage ledger: which anchors are researched, which sources were identified but not read |

# W5 — Baltic Air Policing / QRA (identification as the task)

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet. It is also **half of the
identification argument**; the other half is [`o2-pvo-intercept.md`](o2-pvo-intercept.md), and the
argument itself is stated once in [`INDEX.md`](INDEX.md) §"The identification task".

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of NATO Baltic Air Policing and QRA practice | §Knowledge 1, every fact cited and tiered |
| **FlightBox sources** | what a `.fbm` can declare, and where the perception boundary runs | [`../sensors.md`](../sensors.md), [`../missions/sensors.md`](../missions/sensors.md), [`../missions/verdict.md`](../missions/verdict.md), [`../vision.md`](../vision.md) ("Anti-cheat is a game decision"), [`../modules/f16/datalink-iff.md`](../modules/f16/datalink-iff.md) |

Confidence legend and gap IDs `C0…C21`: [`INDEX.md`](INDEX.md).

**Temporal honesty: none needed.** F-16s of several NATO air forces have flown Baltic Air Policing
rotations, and the mission is current [T4]. What *is* unusual is that this campaign's success
condition contains **no weapon**.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| Mission start | **30 March 2004**, when the Baltic states joined NATO | [T4] |
| Bases | **Šiauliai (Zokniai) AB, Lithuania** (≈ 55.89 N 23.39 E); **Ämari AB, Estonia** from May 2014 (≈ 59.26 N 24.21 E) — *approximate, verify* | [T4] |
| Rotation | three months initially, later **four months** | [T4] |
| Detachment | typically **four fighter aircraft**, 50–100 support personnel | [T4] |
| Framework | part of the **NATO Integrated Air Defence System**, 24/7 surveillance; described as **purely defensive** | [T4] |
| Scramble trigger | an aircraft that **files no flight plan**, **squawks no transponder code**, or **does not talk to air traffic control** | [T3]/[T4] |
| Scramble classes | **Alpha scramble** (a live alert) versus **Tango scramble** (training) | [T4] |
| Reaction | typically **under 15 minutes** | [T4] |
| The task itself | **put eyes on it**: obtain a **visual identification**, shadow the contact along the airspace boundary, escort it clear, peel off. "Policing in the literal sense — presence, identification, escort — not combat" | [T3]/[T4] |
| Typical subjects | Russian military traffic over the Baltic, frequently without transponder or filed plan; transports, ELINT/recon (Il-20 class), bombers with fighter escort | [T4] |

### 2. The campaign contract — and why it is the strictest one in the set

| Contract | Acceptance / measurement anchor |
|---|---|
| **The task is identification, not engagement** | in eight of the ten missions a weapon release is a **failure**, not a result. The pilot's job ends at a stated range abeam a contact |
| **The intruder's team must not leak into the intercepting pilot's behaviour** | *the campaign's core acceptance test:* run each mission twice, identical but for the intruder's `team` line (`neutral` vs `hostile`), and require the interceptor's own telemetry to be **byte-identical up to the first tick at which a SENSOR discriminates**. A pilot that behaves differently before that tick has read the registry, and that is a defect of the same class the tree already guards against structurally ([`../sensors.md`](../sensors.md)) |
| Identity comes only from the two channels that exist | IFF Mode 4 (**two-valued**: friendly / no reply — there is no "hostile" value, [`../sensors.md`](../sensors.md)) and the cooperative datalink (own faction only). **A no-reply contact is not a hostile contact**, and every mission's victory condition must respect that |
| Shadowing is a geometry, not a kill | measured from telemetry: time spent inside a declared range/aspect box abeam the contact, without a weapon event |
| **Ground targets in every mission** | a QRA jet launches over a country: coastal radar sites, ports, a shipping lane. They are never targets here — they are what the intercept is *protecting*, and the campaign needs them declared so that "protect" can eventually be a verdict (`C12`) |
| The verdict is machine-read — and today it cannot be | the objective vocabulary has no *identify*, no *escort*, no *do-not-fire*. Every mission below states the verdict it needs and the workaround it must use until `C12` closes |

### 3. The ten missions

| # | Mission | Task | Time | Wx | Blue | Red / subject | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `w5-01-cooperative` | intercept a contact that answers IFF | day | calm | 2 F-16 (flight), QRA scramble | 1 F-16, `team friendly`, `set iff_xpdr on` | 1 `target_soft` (coastal radar site) | close to the abeam box, **no weapon event** | Does the pilot stop when the contact answers? The trivial case, and the baseline every later run is compared against |
| 2 | `w5-02-silent` | intercept a contact that does not answer | day | calm | 2 F-16 | 1 MiG-29, `team neutral`, `set iff_xpdr off` | as above | reach the abeam box, hold it, **no weapon event** | **A no-reply is not a hostile.** Does the pilot understand that, or does it treat silence as permission? |
| 3 | `w5-03-team-swap` | mission 2 with the intruder's `team` changed to `hostile` and nothing else | day | calm | 2 F-16 | 1 MiG-29, `team hostile`, `set iff_xpdr off` | as above | **the interceptor's telemetry must match mission 2** up to the first sensor discriminator | **The anti-cheat test itself.** One word in a file the pilot may not read. Any divergence is a finding |
| 4 | `w5-04-escorted` | a subject with a fighter escort | day | calm | 2 F-16 | 1 "transport" (F-16 stand-in, `team neutral`) + 2 MiG-29 escort, `team neutral` | 1 `target_soft` + 1 `target_hard` (port) | identify all three, **no weapon event** | With three contacts and two interceptors, does the flight sort onto the *subject* or onto the *escort*? The sort has no notion of "the one that matters" |
| 5 | `w5-05-shadow` | a long shadow along a boundary | day | `wx wind` | 2 F-16 | 1 MiG-29, `team neutral`, on a 200 km leg | 2 `target_soft` along the coast | hold the abeam box for a declared duration | Can the station-keeping law hold a position on a **manoeuvring, non-cooperating** aircraft, when everything it was built for is a datalink report from a friend? ([`../formation.md`](../formation.md)) |
| 6 | `w5-06-two-subjects` | two simultaneous scrambles | day | calm | 2 F-16 (one flight) | 2 MiG-29 on divergent tracks, `team neutral` | 2 `target_soft` | both identified, **no weapon event** | Does the flight split deliberately? There is no lead tasking (`C15`/`F6`) — the split can only emerge from the cost function |
| 7 | `w5-07-locked-on` | the subject locks the interceptor | day | calm | 2 F-16 | 1 MiG-29, `team neutral`, `set n019_emission illum` | as above | identify + **still no weapon event** | A contact that paints you is not thereby hostile. Does the pilot's RWR-driven defend rule ([`../pilot.md`](../pilot.md) §7.2 — a *seeker* on one's own aircraft is never negotiable) fire on a mere lock? It should not — and this mission is where that is checked |
| 8 | `w5-08-weather-id` | the same intercept in cloud | day | `wx fixture` | 2 F-16 | 1 MiG-29, `team neutral` | 2 `target_soft` | identify | **The mission that cannot be flown at all** (`C3`): the real task is a *visual* identification, and FlightBox has no eye. What is left is a radar contact and an IFF silence — which is exactly the information the real QRA pilot launches to get *past* |
| 9 | `w5-09-night-id` | night intercept of a non-squawking contact | **night** | calm | 2 F-16 | 1 MiG-29, `team neutral`, no transponder | 1 `target_hard` (port) + 1 `target_soft` | identify | Night visual identification is the hardest form of the real task and the one FlightBox is furthest from (`C2` + `C3`). Specified so the pair of gaps is visible together |
| 10 | `w5-10-alpha` | the full QRA: unknown intent, escort present, weapons hold | **night** | `wx fixture` | 4 F-16 (two flights, scrambled at different times) | 1 subject + 2 escorts, `team` **declared by the mission author and not visible to any pilot** | 3 `target_soft` + 1 `target_hard` | all three identified; **a weapon event is a FAIL unless the subject shoots first** | Everything at once: two flights, three contacts, no weapons release, a defensive trigger that must be real but must not be trigger-happy — and the whole run repeated across both `team` declarations with the identical-behaviour requirement of mission 3 |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| F-16C | flyable module | **yes** | the QRA pair |
| MiG-29 | flyable module | **yes** | stands in for every non-cooperating subject; a poor stand-in for a transport or an ELINT aircraft, and the mission headers must say so |
| Transport / ELINT (Il-20, An-26 class) | flyable module | **no** (`C7`) | the actual subject of most real intercepts — slow, large, unarmed, and therefore a *completely different* intercept geometry from a fighter |
| Bomber (Tu-22M, Tu-95 class) | flyable module | **no** (`C7`) | the escorted-subject case |
| Ground surveillance radar (coastal) | ground | **`target_soft` today, non-emitting** (`C1`) | what the intercept protects |
| Port / infrastructure | ground | **yes** (`target_hard`) | |
| Ship | surface unit | **no** (`C14`) | the Baltic case is full of them |
| Control and reporting centre (the thing that scrambles you) | infrastructure | **no** (`C6`) | the scramble itself is outside the mission today: `.fbm` spawns an aircraft, it does not scramble one |

### 5. What must be true before mission 1 can fly

`w5-01`, `w5-02`, `w5-03`, `w5-07` are buildable **today** with one caveat that must be written into
each header: the *verdict* has to be read out of telemetry (no weapon event, geometry held) rather
than declared, because `C12` has no objective for it. **Mission 3 is the one to build first** — it
needs nothing new, it is a two-run experiment, and it is the sharpest anti-cheat test in the whole
campaign set.

---

## State

**Nothing built.**

What exists and carries this campaign: IFF as a two-valued reply with no "hostile" value; anonymous
radar contacts carrying no unit id, callsign or team; the perception boundary enforced by the include
graph (`FBUnitRegistry` reaches exactly four files, none of them the pilot); the RWR that reports
bearing and mode but **never range**; the cooperative datalink limited to one's own faction; and the
determinism infrastructure (one fingerprint over `--threads 1/2/4` × 3 repeats) that makes the
byte-identical comparison of mission 2 against mission 3 a mechanical check rather than an opinion.

---

## Gaps

| ID | What is missing | Blocks here |
|---|---|---|
| `C3` | **no visual acquisition** — the pilot has radar, IRST (MiG only), RWR and datalink, and no eye | **the task itself.** Every real Baltic intercept ends in a visual identification; FlightBox cannot perform one, so eight of ten missions here measure the *approach* to an identification and not the identification |
| `C12` | **no `identify`, `escort`, `protect` or `do-not-fire` objective** | no mission here has a declarable victory condition. Everything is read out of telemetry, which means a human must interpret every run |
| `C19` | **no rules-of-engagement state** — nothing expresses "weapons hold", "weapons tight", "cleared to engage" | mission 10's defensive trigger has to be a pilot hook rather than a mission declaration |
| `C7` | **no transport, ELINT or bomber module** | the subject of a real intercept is not a fighter, and substituting one changes the geometry, the closure and the whole point |
| `C2` | **no time of day** | missions 9 and 10 are night intercepts |
| `C6` | **no scramble** — a `.fbm` spawns aircraft, it does not alert them | the 15-minute reaction *is* the mission in reality |
| `C1` | **coastal radars do not emit** | what the QRA protects is scenery |
| `C14` | **no ships** | |
| `C18` | **no radio** | a real intercept includes attempted radio contact on guard, and its failure is part of the identification |
| `C0` | **no campaign layer** | a rotation is 3–4 months of intermittent scrambles; ten independent runs are not a rotation |

### The honest headline

**W5 is the campaign whose subject FlightBox cannot simulate and whose discipline it can test
perfectly.** It cannot do the identification (`C3`, `C12`). It *can* do the thing that makes an
identification mission meaningful in a simulator with an AI opponent: prove that the AI does not
know what it has not been told. Mission 3 costs nothing to build and is worth more than the other
nine.

---

## Knowledge

### 1. The anchor with its sources

- **Mission, dates, bases, rotation and detachment size.**
  [Baltic Air Policing (Wikipedia)](https://en.wikipedia.org/wiki/Baltic_Air_Policing) [T4] — 30 March
  2004 start, Šiauliai/Zokniai as the original base, Ämari from May 2014, three- then four-month
  rotations, "usual deployments consist of four fighter aircraft with between 50 and 100 support
  personnel", the NATO Integrated Air Defence System framing and the "purely defensive" description.
  Institutional framing: [Baltic Air Policing & Air Shielding (milavreachout.org)](https://milavreachout.org/nato-enhanced-air-policing/)
  [T3].
- **The scramble trigger and the task.** [Baltic Air Policing & Air Shielding](https://milavreachout.org/nato-enhanced-air-policing/)
  [T3] — an aircraft that "files no flight plan, squawks no transponder code, or refuses to talk to
  air traffic control" launches the on-call jets on an **Alpha Scramble** "to put eyes on it"; the
  fighters "identify the contact visually, shadow it through allied airspace boundaries, and peel off
  once it is clear. It is policing in the literal sense — presence, identification, escort — not
  combat." Alpha vs Tango scramble and the sub-15-minute reaction:
  [Quick Reaction Alert (Wikipedia)](https://en.wikipedia.org/wiki/Quick_reaction_alert) [T4],
  [Scramble! (Smithsonian Air & Space)](https://www.smithsonianmag.com/air-space-magazine/scramble-180971215/)
  [T3].
- **What gets intercepted.** [Romania's F-16s score their first Baltic intercept — an Il-20M (MiGFlug)](https://migflug.com/jetflights/romania-f16-first-baltic-intercept-il-20m/)
  [T4]; [Swedish fighters intercept a Tu-22M escorted by Su-35S near NATO airspace (Army Recognition)](https://www.armyrecognition.com/news/aerospace-news/2026/swedish-fighter-jets-intercept-russian-tu-22m-bomber-escorted-by-su-35s-fighters-near-nato-airspace)
  [T4] — the two archetypes this campaign uses: the lone ELINT aircraft and the escorted bomber.

### 2. Where the sources are thin, and it is stated

| Thing | Status |
|---|---|
| Published rules of engagement, minimum separation, the abeam geometry a NATO interceptor actually holds | **not sourced and not guessable.** The "abeam box" in §3 is `[SET]` and must be declared as a FlightBox convention in the first mission header |
| Intercept statistics (how many per rotation, how many were non-cooperative) | **not established.** The Wikipedia article's own incident list is anecdotal |
| Whether QRA aircraft are armed, and with what | **not stated in the sources retrieved.** The campaign therefore arms them (a real QRA jet plainly is) and makes *not firing* the discipline being tested |

### 3. Why "identity" is the hardest thing to fake in this engine — and the easiest to test

The tree's identity picture is deliberately impoverished, and every restriction works in this
campaign's favour:

| Restriction | Where it is enforced | What it means here |
|---|---|---|
| A radar contact carries range, bearing, azimuth, elevation, closure and a radar-local track number — **no unit id, no callsign, no team** | `core/FBRadarContact` | the interceptor genuinely does not know who that is |
| IFF Mode 4 is **two-valued** — friendly, or no reply. There is no "hostile" | `core/FBIffReply` | silence is ambiguity, and the campaign's whole subject is what a pilot does with ambiguity |
| The unit registry reaches **four files** in `systems/`+`modules/`, all of them sensors | grep-checked, [`../sensors.md`](../sensors.md) | the pilot has no path to the truth even if it wanted one |
| The cooperative datalink is faction-filtered | `sensors/FBDatalinkSystem` | a friendly picture cannot identify a non-friendly |

So the **test** writes itself: change the intruder's `team` and require the interceptor's own
telemetry not to move. If the boundary holds, the two runs are byte-identical until a sensor
discriminates; if it does not, the diff names the tick and the column where the leak is. That is a
stronger statement than any amount of prose about anti-cheat, and it is why this campaign exists in
a set otherwise made of shooting wars.

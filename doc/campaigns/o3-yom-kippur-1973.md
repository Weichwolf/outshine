# O3 — Yom Kippur 1973: ground attack in contested airspace

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of the opening Arab air operations of 6 October 1973 | §Knowledge 1, cited and tiered |
| **FlightBox sources** | what the MiG-29 module can and cannot do on a ground-attack sortie | [`../modules/mig29/weapons.md`](../modules/mig29/weapons.md), [`../modules/mig29/module.md`](../modules/mig29/module.md), [`../missions/weapons.md`](../missions/weapons.md), [`../weapons.md`](../weapons.md) |

Confidence legend and gap IDs `C0…C21`: [`INDEX.md`](INDEX.md).

### Temporal honesty — and the harder problem underneath it

**The MiG-29 was not in 1973.** The type entered service a decade later; the opening strikes were
flown by **MiG-17, Su-7, Su-20, MiG-21 and Hawker Hunter** [T4]. Bekaa's substitution rule applies
again — *the situation is the anchor, not the serial number* — and again it makes the flying side
**stronger** than history.

But this campaign has a second, sharper problem that no other campaign in the set has:

> **The FlightBox MiG-29 cannot fly a ground-attack mission at all.**
>
> `set task attack` is **closed** on the `mig29` module, and for a stated reason rather than a missing
> weapon: the 9-12 carries no guided air-to-ground store, and its unguided delivery is a *director the
> pilot flies* rather than a *release moment he reacts to* — so `FBMig29FireControl` publishes no
> CCIP/CCRP block, and the attack phase has no cue to pickle on
> ([`../modules/mig29/module.md`](../modules/mig29/module.md), [`../modules/mig29/weapons.md`](../modules/mig29/weapons.md) §5.3).

That is gap `C9`, and it is not a detail: **it blocks this entire campaign**, not a mission in it.
The campaign is specified anyway, for two reasons — it is the only campaign that states the
requirement for a *director-based* delivery mode, and it is the eastern half of the ground-attack
question that W2/W3/W4 answer from the western side.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| Date / time | **6 October 1973, ≈14:00**, a coordinated Egyptian and Syrian surprise attack | [T4] |
| Egyptian opening strike | **≈220 strike aircraft** — MiG-21, Su-7, MiG-17, Hawker Hunter — plus ≈100 Mi-8 assault helicopters | [T4] |
| Syrian opening strike | **≈100 aircraft**; **Su-7 and MiG-17 fighter-bombers came in very low while MiG-21s provided top cover** | [T3]/[T4] |
| Targets | command posts, observation points, artillery positions, armour, fortifications | [T3] |
| Opposition on the first strike | **none from the Israeli air force**; defensive fire from Hawk batteries and scattered AAA | [T4] |
| Losses on the first strike | **light** | [T4] |
| The SAM umbrella | **>200 SAM batteries** (SA-2, SA-3, SA-6) massed to shield the canal crossing; **>40 SA-6 batteries**, each typically 3–6 launchers, providing mobile low-level cover, with fixed and semi-mobile SA-2/SA-3 above them | [T4] |
| Later Arab types | MiG-23 and Su-20 also present in the campaign | [T4] |
| Anchor region | Suez Canal ≈ 30.4–31.2 N 32.2–32.6 E; Golan ≈ 32.9–33.3 N 35.7–36.0 E (**approximate, verify**) | [T4] |

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| The subject is **delivery under an umbrella**, not air combat | the striker's job is to place ordnance and come home *inside* friendly SAM cover. Air-to-air is what happens when it goes wrong |
| **The umbrella is a constraint, not a shield** | until `C1` exists it protects nothing, so every mission declares its umbrella boundary as a **geographic limit on the `wp` set** and measures how often the striker leaves it |
| The delivery mode must be **the aircraft's own** | not the F-16's. A MiG-29 attack mode built by copying `FBF16FireControl`'s CCIP/CCRP would be a false statement about the aircraft. §Knowledge 3 states what the honest version requires |
| **Ground targets in every mission** — obviously, but also as *defended* objects | this is the one campaign where the ground target is the point and the fighters are the interruption |
| Low level is the profile | "came in very low" is the anchor's own phrase; that meets `C20` head-on |
| The verdict is machine-read | `objective kill unit <target>` + `survive`; a striker that dies after release still killed the target, and the verdict rule already handles that ([`../missions/verdict.md`](../missions/verdict.md)) |

### 3. The ten missions

Ours = MiG-29 (as the archetype fighter-bomber). Blue = Israeli side (F-16 module).

| # | Mission | Task | Time | Wx | Ours | Blue | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `o3-01-unopposed` | single-ship strike, no opposition | day | calm | 1 MiG-29 with unguided stores | — | 1 `target_soft` (artillery position) | `kill unit` | **The blocking mission.** Can this module deliver an unguided store at all? Today: no (`C9`). This mission is the acceptance test for the director-mode work |
| 2 | `o3-02-low` | the same strike at very low level | day | calm | 1 MiG-29 | — | 1 `target_soft` | `kill unit` + `waypoints` | Low-level delivery over real terrain, meeting `C20` (the guidance holds an ASL altitude, not an AGL one) |
| 3 | `o3-03-pair` | two-ship strike, line astern | day | calm | 2 MiG-29 (flight) | — | 2 `target_soft` (a battery position) | both `kill unit` | Does the second aircraft's release inherit the first's error, and can a flight put two aircraft over one target without a timing mechanism (`C15`)? |
| 4 | `o3-04-top-cover` | strikers with a fighter escort above | day | calm | 2 MiG-29 strike + 2 MiG-29 top cover | 2 F-16 CAP | 2 `target_soft` + 1 `target_hard` (command post) | strikers `kill unit` + ≥3 of 4 `survive` | The anchor's own structure: low fighter-bombers, MiG-21 top cover. Does the cover engage early enough to matter, or does it follow the strikers down? |
| 5 | `o3-05-hawk` | strike into a defended position | day | calm | 4 MiG-29 | — | 1 `target_hard` + 2 AAA/SAM sites (**inert, `C1`**) | `kill unit` on the hard target + all `survive` | The historical defence was Hawk batteries and AAA. Unanswerable today — the mission exists to keep the shape on record |
| 6 | `o3-06-inside-the-umbrella` | strike with a hard geographic limit | day | calm | 4 MiG-29, `wp` set confined | 4 F-16 (two flights) intercepting | 3 `target_soft` | `kill unit` + ≥3 `survive` **and no striker leaves the declared box** | The umbrella's real cost: a striker that may not chase, may not extend and may not climb. Measurable today as a **discipline** metric even though the umbrella itself is inert |
| 7 | `o3-07-pursued` | egress with an interceptor behind | day | calm | 2 MiG-29 with stores still aboard | 2 F-16 | 1 `target_hard` | `kill unit` + both `survive` | Does the pilot jettison? There is **no jettison decision** in the tree — stores come off through a release, so "get rid of the drag and fight" is not expressible |
| 8 | `o3-08-armour` | strike a moving column | day | calm | 4 MiG-29 | 2 F-16 | 6 `target_soft` in a column (**static — `C14`**) | ≥4 of 6 | Against small dispersed targets, how many aircraft does one column cost — and how much of the answer is the store rather than the pilot? |
| 9 | `o3-09-two-fronts` | two simultaneous strikes | day | calm | 4 + 4 MiG-29 (two flights, two axes) | 4 F-16 | 4 `target_soft` + 2 `target_hard` | ≥4 of 6 targets + ≥6 of 8 `survive` | With the defender forced to choose an axis, does splitting the attack pay — and does the run stay deterministic at 14 units? |
| 10 | `o3-10-october-six` | the opening strike | day | calm | 8 MiG-29 strike + 4 top cover (three flights) | 4 F-16 scrambled **late** | 6 `target_soft` + 2 `target_hard` | ≥6 of 8 targets killed AND ≥9 of 12 recover | The anchor's own result was "surprise, light losses". **Can FlightBox reproduce a strike that succeeds because the defender was late** — which is a statement about spawn timing, not about flying? |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| MiG-29 | flyable module | **yes, but cannot attack the ground** (`C9`) | archetype substitution for MiG-17/Su-7/Su-20 — and the substitution is *very* generous: those were subsonic or first-generation supersonic attack aircraft |
| MiG-17 / Su-7 / Su-20 / MiG-21 class | flyable module | **no** (`C7`) | the actual force |
| F-16 | flyable module | **yes** | Israeli side stand-in (the real defender flew F-4, Mirage and A-4) |
| Unguided bombs for the MiG (FAB class) | store catalogue | **no** (`C8`) | `core/FBStore.h` has Mk-82 only among unguided stores |
| Rocket pods | store catalogue | **no** (`C8`) | a primary weapon of the type being represented |
| SA-2 / SA-3 / SA-6 (friendly umbrella) | ground, emitting + shooting | **no** (`C1`) | the campaign's defining asset, and it belongs to **our** side here — the first campaign in the set where that is true |
| Hawk battery / AAA (hostile) | ground, shooting | **no** (`C1`) | the historical defence |
| Artillery position, command post, fortification | ground | **yes** (`target_soft`/`target_hard`) | |
| Armour column | ground, **moving** | **static only** (`C14`) | |
| Assault helicopter (Mi-8 class) | flyable module | **no** (`C7`) | ≈100 of them in the anchor |

### 5. What must be true before mission 1 can fly

**Nothing in this campaign is buildable today.** Mission 1 is blocked by `C9` and `C8` at once: the
module publishes no release cue and the catalogue holds no store it would drop. That makes O3 the
**only campaign in the set with zero runnable missions**, and it is listed as such in
[`INDEX.md`](INDEX.md).

---

## State

**Nothing built, and unusually: nothing buildable.**

What exists and would be reused the moment `C9` closes: the shared ballistics primitive
(`core/FBBallistics`, one forward integration answering both "where does it land" and "when must I
release"), the store-as-a-unit life cycle, the ground-target damage model with its two fragility
classes, the measured air-to-ground error budget on the F-16 side (22.2 m clean, 482 m two seconds
late, no effect at all on `target_hard`), and the sub-tick impact reconstruction.

---

## Gaps

| ID | What is missing | Blocks here |
|---|---|---|
| `C9` | **the MiG-29 module cannot fly `set task attack`** — no CCIP/CCRP block, because the real aircraft's unguided delivery is a director rather than a release cue | **the campaign, entirely.** Not one mission runs |
| `C8` | **no FAB-class bomb, no rocket pod** in the store catalogue | the aircraft has nothing to carry even once it can aim |
| `C1` | **no SAM, no AAA** — and here it is needed on **both** sides: as our umbrella and as their defence | missions 5, 6 |
| `C7` | **no period aircraft at all**, on either side | the substitution is the largest in the whole set |
| `C14` | **no moving ground units** | mission 8 |
| `C20` | **no terrain-following guidance** | "came in very low" is the anchor's own description of the profile |
| `C15` | **no strike timing** | a 220-aircraft coordinated opening strike is a timing problem before it is a flying one |
| `C11` | **no strafing** | gun attack on a ground target is not resolved at all |
| — | **no jettison decision** | mission 7; stores leave only through a release, so an encumbered fighter cannot clean itself up |
| `C0` | **no campaign layer** | |
| `C2` | **no time of day** | the 14:00 launch is a deliberate sun-angle choice in the anchor |

### The honest headline

**O3 is the campaign that says what the MiG-29 module is missing.** Every other eastern campaign can
be flown, partly or fully, with the module as built; this one cannot start. Its value is therefore not
ten mission files but one requirement, stated precisely in §Knowledge 3: *a director-based unguided
delivery mode, derived from the aircraft's own documented sighting, plus one unguided store in the
catalogue.* Until that exists, the eastern half of FlightBox is an air-to-air air force.

---

## Knowledge

### 1. The anchor with its sources

- **The opening strikes.** [The Arab-Israeli War of 1973: honor, oil, and blood (HistoryNet)](https://historynet.com/the-arab-israeli-war-of-1973-honor-oil-and-blood/)
  [T3] — the 220 Egyptian strike aircraft with the type list (MiG-21, Su-7, MiG-17, Hawker Hunter) and
  ≈100 Mi-8s, launched at 14:00 on 6 October against no air opposition.
  [Air operations during the 1973 Arab-Israeli war (Marine Corps study, GlobalSecurity)](https://www.globalsecurity.org/military/library/report/1985/MML.htm)
  [T1] — the Syrian strike of close to 100 aircraft against command posts, observation points,
  artillery, armour and fortifications, with **Su-7 and MiG-17 fighter-bombers coming in very low
  while MiG-21s provided top cover**. This is the campaign's most important single sentence and it is
  the one with the strongest source in the file.
- **The SAM umbrella.** [Yom Kippur War (Wikipedia)](https://en.wikipedia.org/wiki/Yom_Kippur_War)
  [T4] and [1973 raid on Egyptian missile bases (Wikipedia)](https://en.wikipedia.org/wiki/1973_raid_on_Egyptian_missile_bases)
  [T4] — over 200 batteries of SA-2/SA-3/SA-6 shielding the canal crossing, more than 40 of them SA-6
  with 3–6 launchers each, the SA-6 providing mobile low-level cover under the fixed SA-2/SA-3 layer.
- **Losses and defensive fire.** [Yom Kippur War (Wikipedia)](https://en.wikipedia.org/wiki/Yom_Kippur_War)
  [T4] — Hawk batteries and scattered AAA, Egyptian losses on the opening strike light.
- **Type presence.** [Sukhoi Su-17 (Wikipedia)](https://en.wikipedia.org/wiki/Sukhoi_Su-17) [T4],
  [Yom Kippur War aircraft (Military Factory)](https://www.militaryfactory.com/aircraft/yom-kippur-war-aircraft.php)
  [T4].

### 2. Where the sourcing is thin, and it is stated

| Thing | Status |
|---|---|
| Attack profiles: ingress altitude in metres, run-in speed, delivery mode (level, dive, toss) | **not sourced.** "Very low" is all the strongest source says. Every altitude and speed in a mission of this campaign is therefore `[SET]` and must be labelled |
| Ordnance per aircraft | **not sourced** |
| Precise loss figures for the opening strike, by type | **not sourced**; "light" is the strongest statement found and it is not converted into a number |
| The umbrella's actual coverage geometry | **not sourced**; missions declare a box `[SET]` |

### 3. The requirement this campaign exists to state

What a MiG-29 ground-attack mode must be, if it is built — and what it must **not** be:

| Must be | Must not be |
|---|---|
| Derived from the aircraft's own documented sighting: a **director** the pilot flies onto, computed by the aircraft and displayed as a steering cue, with the release decision belonging to the pilot | a copy of `FBF16FireControl`'s CCIP/CCRP block with different constants. That would be a false statement about the aircraft, and the reason the MiG module refused to publish the block in the first place |
| Measured against the same error budget the F-16 side already has (22.2 m clean, 482 m two seconds late) so the two are comparable | tuned until the numbers look similar |
| Accompanied by **one** unguided store in `core/FBStore.h` with mass, drag and a ballistics table from its own model, exactly as Mk-82 was | a re-labelled Mk-82 |
| Honest about the pilot's role: on a director delivery the *flying* is the accuracy, so the campaign's error budget will be dominated by tracking, not by computation | assumed to behave like a release-cue delivery |

That is a single, bounded work item, and this file is where its requirement lives until it is built.

### 4. Why the campaign is kept despite being unbuildable

Three reasons, all of them structural rather than sentimental:

1. It is the only place in the tree where the **friendly** side owns a SAM umbrella. Every other
   campaign treats surface-to-air as the enemy; `C1`'s design must accommodate both, and this file is
   the requirement that says so.
2. It is the eastern counterpart of W2/W3/W4 — without it, "FlightBox can do ground attack" is a
   claim about one module rather than about the architecture.
3. Its blocking gap (`C9`) is invisible from every other campaign. An air-to-air-only eastern set
   would never have surfaced it.

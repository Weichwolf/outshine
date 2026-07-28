# MiG-29A (9-12) — Defence: SPO-15LM "Beryoza" RWR, BVP-30-26 Dispensers, ECM

**Variant scope**: izdeliye **9-12** (Fulcrum-A), SPO-15LM. Deltas in §8.

**Sources**: **DCS-EA** = `doc/DCS MiG-29A Early Access Manual EN.pdf` (printed page == PDF page);
pages used: 24, 33–34, 38, 69, 105–112. **DCS-FM** = `doc/DCS MIG-29 Flight Manual EN.pdf`
(**printed page = PDF page − 6**); pages used: 13, 30–32, 62, 104.
Research in **§6 Technical depth**, tiered **T1** > **T2** > **T3** > **T4**.

**Depth declaration**: **FULL** on the SPO-15LM — `DCS-EA p.105–112` is an unusually deep, physically
argued description of an *analogue* RWR, and it is reproduced here essentially completely because
**every one of its limitations is a modelling requirement, not colour**. **MEDIUM** on the dispensers
(quantities and switch logic documented, programme parameters are not). **SHALLOW** on ECM — the 9-12
has none, and the S's Gardeniya is described only qualitatively.

> ⚠️ **The two manuals disagree outright on the RWR threat-type letters.** See §2.4. The `DCS-EA`
> table is the correct/physical one; the FC3 table is a simplification. Do not merge them.

---

## Spec

### 1. The one architectural thing to internalise

The SPO-15 is **entirely analogue**: application-specific circuits, no processor, **one azimuth channel
processed at a time, clockwise** (`DCS-EA p.111`). Where the F-16's ALR-56M is described as a
detector + a threat library + a priority sorter (`doc/modules/f16/defence-rwr-cm.md`), the SPO-15 is better
modelled as **a set of independent receivers whose physics leaks into the display**:

- It does not report **range** — it reports **received power over threshold in 2 dB steps**
  (`DCS-EA p.108`). Same as the F-16 in principle, but here the power scale is *the* display axis.
- Its **azimuth resolution is a function of the antenna pattern**, and the pattern varies with the
  emitter's **frequency** and **power** — so the *same* emitter can light one sector, two sectors, or
  (worst case) **all of them at once** (`DCS-EA p.111`).
- **When the MiG-29's own radar radiates, the whole forward hemisphere of the RWR is switched off**
  (`DCS-EA p.112`) — because the device cannot filter its own HPRF. **This is the single most
  tactically consequential fact in the file**: *using your radar blinds your RWR forward*.

---

### 2. SPO-15LM "Beryoza" — the receiver (**FULL**)

Designation in the airframe list: **L006LM "Berjoza"** (`DCS-FM p.13`). Purpose: warn of RF emission
**in the centimetre band** for defence against hostile fire-control radars (`DCS-EA p.105`).

#### 2.1 What it reports (`DCS-EA p.105`)
- Azimuth to the illumination event
- Mode of the illuminating radar (**search or track**)
- **Type** of the threat
- Determination of the **main threat**
- **Closing dynamics** of the main threat
- For SAM batteries, the **estimated weapon-engagement-zone border in terms of equivalent signal
  power**
- **Main threat elevation** relative to own aircraft

#### 2.2 Antenna geometry and azimuth resolution (`DCS-EA p.105–106`) — **the core model**
| Property | Value |
|---|---|
| Coverage | **360° azimuth, ±30° elevation** (dedicated receiving channels) |
| Minimum indication per detection event | **≥125 ms flash** in the corresponding channel + a **low audio tone** |
| Azimuth channels, LM variant | **10 physical, 8 logical** |
| Forward-hemisphere antenna | array of **4 separate feeds with a common Luneburg lens** per side |
| Per-channel half-power beamwidth (forward) | **20° azimuth × 60° elevation** |
| Peak separation in azimuth | **20°** |
| LM-specific | **the 70° and 50° channel feeds are merged into a single logical channel** |
| Rear hemisphere | standard **spiral antennas**, 40 %-power beamwidth **≥60°** |
| Designed sector overlap (forward) | **10°**, at average frequency and power → in-between signals show as a **double detection** |
| **Resulting azimuth resolution** | **10° within the forward ±50° sector**; **±20° beyond it**; **±45° in the rear hemisphere** |
| **Virtual sector** | In the LM, the **90° sector on the indicator is virtual** — it corresponds to a *simultaneous* detection in the rear hemisphere **and** in the 50° sector on the same side |

#### 2.3 Indicator layout (`DCS-EA p.33`, repeated `p.106`)
1. Device-ready light · 2. **Main-threat azimuth light** · 3. Threat azimuth light · 4. **Tracking
warning light — RED** · 5. **Emitter power-level indicator** (circular scale) · 6. Own-aircraft
silhouette · 7. Threat azimuth indicator (rear channels) · 8. Primary-threat azimuth indicator (rear
channels) · 9. Main-threat **type** light · 10. Threat type light · 11. Brightness knob ·
12. **BIT switch MANUAL–AUTO** · 13. Main-threat position light (not implemented) ·
14. **Elevation indicators: "В" = upper hemisphere, "Н" = lower hemisphere**.

Control panel, right console (`DCS-EA p.34, p.107`): **1. Allow-search on/off** (the search filter) ·
**2. Power on/off** · **3. Audio volume**. (The volume pot has an off detent that lights a separate
"sound disabled" lamp, `DCS-EA p.112`.)

#### 2.4 Threat types — ⚠️ **the manuals contradict each other**

**`DCS-EA p.107` (correct, physically argued — model this one):**
| Letter | Typical threats | Description |
|---|---|---|
| **П** | F-4, Aegis ships | **LPRF fire-control radar with a CW illuminator in SA guidance mode** (co-located pulse + CW) — *"should be treated as a launch warning in most cases"* |
| **З** | Vulcan | AAA / short- and medium-range SAM fire-control radar |
| **Х** | Hawk CWAR/HiPIR | **Continuous-wave radar**; HPRF radars at low power levels |
| **Н** | Nike-Hercules | Strategic SAM search and fire-control radar |
| **F** | F-14, F-15, F-16 | **4th-generation HPRF fighter radar** |
| **С** | F-4, F-5, Mirage F1, naval radars | LPRF fire-control radar with the CW illuminator channel disabled; other known threat radar types incl. LPRF fighter FCR and surveillance radars |

**`DCS-FM p.31` (FC3, simplified — do NOT model):** П = airborne radar, З = long-range, Х =
medium-range, Н = short-range, F = early warning, С = AWACS. This is a *category* mapping, not the
device's real PRI/pulse-width classification. Two manuals, two meanings for the same six letters.

**Stock threat programme** (`DCS-EA p.109`):
П: F-4, F-104, Talos, Terrier, Tartar, SM-1 · З: Vulcan, Sea Wolf, Mk 35, AN/SPG-48, AN/SPG-53 ·
Х: Hawk CWAR/HiPIR · Н: Nike-Hercules TTR/MTR · F: F-14, F-15, F-16, F/A-18, M-2000 ·
С: F-4, F-5, F-104, Mirage F1, Jaguar, Talos, Terrier, Tartar, SM-1.

#### 2.5 Recognition, search vs track (`DCS-EA p.108`)
- Classification is by **pulse width and PRF**. Six types; **if the signal matches nothing, no type lamp
  lights** (unknown ≠ a symbol).
- **Friendly radars are by convention not in the threat programme** — but a friendly radar with similar
  parameters **may be misrecognised** as a threat type. Two hostile radars with overlapping parameters
  collide the same way: **the lower-priority one is displayed as the higher-priority one.**
- **Search vs track is decided by the length of the detection event: longer than 125–250 ms = track
  mode** → higher priority, **red light in the middle of the indicator** and a **continuous high audio
  tone**.
- The **search filter switch** is tied to exactly this property: switched off, **emitters not
  classified as tracking are filtered out**.

#### 2.6 Priority logic (`DCS-EA p.108–110`) — **FULL**
| Priority | Condition |
|---|---|
| 1 | Nike-Hercules launch (not implemented) |
| 2 | **Emitter in track mode** |
| 3 | Emitter **within** the azimuth-altitude priority range and closest to the front of the type-priority row |
| 4 | Emitter **outside** the azimuth-altitude priority range and closest to the front of the type-priority row |
| 5 | **Emitter PRF above 781 Hz** |
| 6 | Emitter signal power is the highest |

Type-priority rows:
- **High-priority row**: **П > З > Х > Н > F > С**
- **Low-priority row**: **Х > Н > F > П > З > С**

Row selection is by an **azimuth-altitude criterion**, and here is the MiG-29 peculiarity:
> **"In the MiG-29, the altitude is permanently assumed to be 26,000–55,000 ft, and threats with WEZ
> ceiling below 26,000 ft are always considered low priority."** (`DCS-EA p.109`)

i.e. **the RWR does not know your actual altitude** — it hard-assumes a high-altitude interceptor
profile. Flying low, the device systematically **de-prioritises exactly the short-range SAMs that can
actually reach you**.

Azimuth criterion per type (`DCS-EA p.110`):
| Types | Low-priority geometry | High-priority geometry |
|---|---|---|
| **П, F** | abeam targets | everything else |
| **З, Х** | — | **forward hemisphere** |
| **Н** | — | head-on and abeam |
| **С** | — | head-on and non-abeam |

Main-threat memory (`DCS-EA p.108`): **8–12 s in search mode, 2–4 s in track mode**.
Signal power: displayed in **2 dB increments** on the circular scale. Elevation: two lights (**В** /
**Н**), **mutually exclusive**, and **the border between them shifts with signal power** — "both signal
power and elevation should be treated as estimates".
**SAM WEZ cue**: for surface-to-air types, the **weapon-engagement-zone border of the main threat type**
is flashed on the power scale — deliberately **conservative** (it corresponds to the *lowest-power*
emitter of that type).
Nike-Hercules launch would be shown by **modulating the track light and its tone at 2 Hz** (not
implemented).

#### 2.7 Device limitations (`DCS-EA p.111–112`) — **all of these are modelling requirements**
1. **Sequential clockwise per-channel processing**; the only inter-channel interaction is the power
   measurement. Azimuth indication is therefore limited by the physical antenna pattern, not only by
   channel count.
2. **Forward antenna directivity varies strongly with frequency** → **low-frequency radars light
   multiple sectors**. The mitigation is tuned for a specific power level, so **the effect gets worse at
   high power**. Worst case: the device is **"blinded" — all azimuth channels trigger at once**.
3. **High-power search signals can be misread as track**, via side-lobe detections lengthening the
   illumination event.
4. **Two pulse radars on the same azimuth**: distinguishable only if they differ by **≥6 dB (3 levels
   on the indicator)**. With more radars, only the strongest is recognised (if ≥6 dB above the rest);
   otherwise **none** is recognised, because the PRF cannot be measured.
5. **Jitter/stagger/random-PRF radars** may not be recognised at all, even if in the programme.
6. **Track is a property of the whole azimuth channel, not of an event**: once any track is detected,
   the sector is **marked tracking for the next 2–4 s** — so a second, search-mode radar in that sector
   can be reported as tracking, and can **overwrite** the real tracker if it is higher priority.
7. **CW + type С on the same azimuth, same mode → falsely identified as a single type П.** And **if
   type П is selected as priority, it suppresses display of types Х and С entirely.** The device filters
   this false П only when the two radars are in *different* modes.
8. **The CW detection circuit falsely detects HPRF as low-power CW.** HPRF emitters are typically seen
   as **type Х 6–14 dB before** they are seen as pulsed. **A flashing Х (as opposed to solid) marks this
   ambiguous low-power CW identification.** Once an HPRF type is positively identified, **the CW circuit
   is shut down — blocking detection of real CW threats.** (Consequence: 4th-gen fighters + Hawk
   batteries in the same airspace = a genuine blind spot.)
9. **Elevation channels have low antenna efficiency** → they usually do not activate until **high power
   levels, often well inside the weapon-engagement zone**.
10. **At power level 0 only CW signals are detected**; pulsed signals are deliberately filtered until
    **level 1**, to avoid chaotic flicker.
11. **When the onboard FCR is radiating, the forward hemisphere of the device is completely disabled.**

#### 2.8 Built-in test (`DCS-EA p.112`, and the crew procedure `DCS-EA p.74`)
- **AUTO (switch right)**: automatic system + light test. The **device-ready light goes out**; all other
  lights come on; **if ready returns within 5–7 s the test passed**. Brightness is **halved** during
  test. (Crew procedure `DCS-EA p.74` gives the same 5…7 s.)
- **MANUAL (switch left)**: feeds a CW test signal to one azimuth channel; repeated presses cycle
  channels. **First 16 presses = higher frequency sub-band, next 16 = lower sub-band** — *"in the 9-12
  this is the only time the device operates the two receivers separately, as binning by carrier
  frequency is permanently disabled in this aircraft"*. Each sub-band cycles channels **twice**, except
  the two merged forward channels (50°/70°). Elevation channels are tested with the rear channels on the
  first pass. In the higher sub-band the device should report **main type Х in track**. Exit by pushing
  the switch right. **After a test a false main type may persist for 2–12 s.**

#### 2.9 Threat programme provenance (`DCS-EA p.110`)
- The recognition circuits are **plug-in cartridges**, in principle field-replaceable, in practice
  changed by **resoldering jumpers**. Programmes were **assigned per theatre**, and typically **never
  updated after the Warsaw Pact dissolved**.
- DCS therefore offers **Stock** (the historical, obsolete programme — threats are still *detected*, but
  **not recognised correctly**) and **Automatic** (generated from the mission's threat list; precedence
  aircraft > air defence > navy > surveillance; **overlaid on top of the stock programme, so stock
  entries remain in memory and can still produce false IDs**).
- ⚠️ The Stock/Automatic selection is an **ED mission-editor construct**, but it is a faithful model of
  a real property: **the SPO-15's classification is only as current as its hardware programme.**

#### 2.10 FC3 cross-check (`DCS-FM p.30–32`) — simplified, kept for the record
Azimuth ±180°, elevation ±30°. **Unlimited threats on screen.** **Threat history duration 8 s.**
Function modes: **All (acquisition) or Lock** (the "ОБЗОР/ОТКЛ" switch). *"If the time between radar
spikes of a threat radar is eight or more seconds, the azimuth lights will not blink."* Acquisition →
low tone; lock → steady high tone + Lock/Launch light; **radar-guided missile launch → the Lock/Launch
light flashes** with a high-pitched tone. **ARH missile detected after its own seeker goes active — it
becomes the primary threat; the cue is a rapid rise in signal strength.** *"RWS does not have IFF
capabilities … the system warns of every radar, both adversarial and friendly."*

---

### 3. Countermeasures — BVP-30-26 dispensers

| Property | Value | Source |
|---|---|---|
| Blocks | **2 × BVP-30-26**, mounted **in front of the vertical stabilisers** | `DCS-FM p.13`, `DCS-EA p.112` |
| Capacity | **30 × PPI-26 cartridges per block = 60 total**, calibre **26 mm** | `DCS-FM p.13` |
| Cartridge designation (research) | **PPI-26-1V** | T4, §6.2 |
| Release button | **right throttle grip** | `DCS-EA p.69` (item 6), `p.112` |
| Mode selector | **front instrument panel, under the emergency chaff/flare jettison button** | `DCS-EA p.24, 112` |
| **Programme selector** | 3 positions: **"GROUND" · "FHS" (front sphere) · "RHS" (rear sphere)** | `DCS-EA p.24` |
| Emergency jettison | dedicated button on the same panel | `DCS-EA p.24` |
| Counter | count indicator window on the panel | `DCS-EA p.24` |
| Dispenser check button | present, **not implemented** in DCS | `DCS-EA p.34` |
| Cockpit counter | "**Flares counter**" on the central panel | `DCS-FM p.22` (item 19) |

**Rebuild note vs the F-16's ALE-47**: the MiG-29 has **no mode state machine** (no OFF/STBY/MAN/SEMI/
AUTO/BYP), **no consent logic**, and **no per-programme burst/salvo parameters** in either manual. What
it has is **three fixed programmes selected by geometry** — ground, forward-hemisphere threat, rear-
hemisphere threat. That is a *much* simpler model than `doc/modules/f16/defence-rwr-cm.md` §2.2, and the
simplification is real, not a documentation gap for the *selector*. **The burst/salvo parameters of the
three programmes are, however, a genuine gap** (§7).

**Chaff vs flare split**: neither manual states how the 60 cartridges are divided between IR and RF
decoys, nor whether the split is a ground-crew loadout choice. ⚠️ Gap.

---

### 4. ECM — "Gardeniya" (**not on the 9-12**)

`DCS-FM p.62` describes the active jammer explicitly as a **MiG-29S** system:
- **"Gardeniya"**, product **L203**, for individual protection against radar-guided air-to-air and
  surface-to-air weapons.
- Modelled **only in noise-speck mode with range stealing** — the target cannot determine *range*, hence
  cannot employ missiles effectively.
- **Effective only at relatively long range; useless in a dogfight and not used there.**
- Sectors: **±60° azimuth, ±30° elevation, front and rear hemisphere.**

Research (T4) names **"Gardeniya-1FU"** in the MiG-29's equipment list generally; the fit is
podded/variant-dependent. **For a 9-12 build, model no onboard jammer.**

---

### 5. Related warnings

`DCS-FM p.104` voice messages relevant to the defensive suite:
| Trigger | Message |
|---|---|
| ECM not functional | "**ECM failure**" |
| **Missile launch warning system (MLWS) not functional** | "**MLWS failure**" |
| Hostile missile within **15 km**, per clock position and relative altitude | "**Missile, 12/3/6/9 o'clock low/high**" |

⚠️ **Flag**: the 9-12 has **no missile-approach warning system**. The FC3 "MLWS" and the directional
missile calls are **ED game-avionics constructs** (they give the player an omniscient MAWS the real
aircraft does not have). Do **not** implement them in a faithful 9-12 module — the MiG-29's only launch
cue is the SPO-15 reading a **type П** emitter or a **track-mode** transition (§2.4, §2.5).

---

### 8. Variant notes
- **9-13 / MiG-29S**: **BVP-30-26M** dispensers; **"Gardeniya" L203 active jammer** available (§4);
  same SPO-15LM.
- **Earlier SPO-15 (non-LM)**: has **separate 50° and 70° forward channels** (they are merged only in
  the LM, §2.2) and **carrier-frequency binning enabled** (permanently disabled in the 9-12, §2.8).
  Both differences are stated in `DCS-EA` and are worth recording in case an earlier-block model is
  ever attempted.
- **MiG-29SMT/MiG-35**: L-150 "Pastel" digital RWR — a different device entirely; nothing in §2 carries
  over.

---

## State

**Superseded by build — the authoritative state is [`module.md`](module.md).** The SPO-15LM
(`FBMig29Rwr`) was built in stage 2b; this round added the **BVP-30-26 dispensers** (`FBMig29Cmds`, a
`sensors/FBCountermeasureSystem` override). What is [DOC] is the magazine — 60 combined cartridges
(2×30 PPI-26, 26 mm, §3); what is [SET] is everything §3/§7 names as a gap — the 30/30 chaff/flare split
(no source states it), a 5/5 BINGO, and the programme burst/salvo parameters (schema from the source,
values set, exactly as the F-16's ALE-47). The three geometry programmes (GROUND/FHS/RHS) are mapped
onto the generic six-slot machine so the pilot's SEMI/AUTO-on-RWR path is the F-16's. There is NO MAWS
(§5), so an infrared shot is answered only by a briefed throw, not automatically. Flares seduce an IR
seeker through `sensors/FBIrstSystem::SelectFlare` — `mig29-defend.fbm` measures it. The paragraphs
below are the original **spec-first** commitment, kept for its reasoning.

The historical note: **Nothing in this file is implemented.** FlightBox has no MiG-29 module, no
`sim/src/modules/mig29/` and no JSBSim MiG-29 model. The airframe exists only as a **spec-first
contract** — [`module.md`](module.md), whose own status
line reads *"spec only. Nothing is built."* Everything below is therefore a **forward commitment**,
not a description of code.

| Roadmap stage | What it will take from this file |
|---|---|
| **R3** — knowledge base | *running*: this file is the R3 deliverable for the SPO-15 and the dispensers |
| **R6** — asymmetric weapons + RCS | the SPO-15 priority logic keys off assumed geometry, so it interacts with whatever RCS/emitter model R6 introduces |
| **R7** — enemy units at BVR scale | the eleven documented analogue limitations (§2) are **modelling requirements, not colour** — several of them (own-radar blanking of the forward hemisphere, per-channel track marking, CW/HPRF confusion) create exploitable, deterministic behaviours a FlightBox `FBRwrSystem` override must reproduce; the BVP-30-26 dispensers map onto `FBCountermeasureSystem` |
| **R8** — JSBSim model | nothing directly |

**The scale caveat that governs every row** (from the module file): the MiG-29 is a
**BVR-scale** opponent — what has to be right is what he can reach, how fast he gets there, what he
can see and what he can shoot. A failing knife-fight comparison is not a defect of the model; a wrong
envelope is.

Roadmap chain: [`../flightbox/roadmap.md`](../../roadmap.md) — **R3** (this knowledge base,
running) → **R6** (asymmetric weapons + RCS) → **R7** (enemy units, MiG-29 at BVR scale) → **R8**
(the JSBSim MiG-29 model). Nothing after R3 has begun.

---

## Gaps

**Source gaps** — the file's own itemised list follows, section number unchanged. The
**GAF T.O. 1F-MIG29-1** is the one acquisition that would raise several of them to T1
(`PROGRESS.md`).

**Implementation gaps** — none statable yet: nothing is built (see State).

### 7. Open gaps (honest)
1. **Dispenser programme parameters** — the single biggest gap. FlightBox's
   `FBCountermeasureSystem` schema (salvo size/interval, salvo count/interval, per type) has **no
   MiG-29 values to fill it with**. Anything used must be marked `[SET]`, exactly as the F-16's ALE-47
   programme values are.
2. **Chaff/flare loadout split** of the 60 cartridges.
3. **SPO-15 detection sensitivity in absolute terms** — the manual speaks only in "power levels over
   threshold" and 2 dB steps; there is no dBm figure, so an absolute range model must be derived from
   the emitter side (as FlightBox already does with `kBeamRangeFactor`).
4. **The 781 Hz PRF priority threshold (§2.6, priority 5)** is stated without explanation — presumably
   an HPRF/MPRF discriminator. No source explains it.
5. **Elevation channel activation threshold** — stated qualitatively ("high power levels, often well
   within the WEZ"), never numerically.
6. **Audio tone frequencies/patterns** — described as "low"/"high"/"continuous", never specified.
7. **Whether the FCR-blanking of the forward hemisphere (§2.7 item 11) is instantaneous or has
   hysteresis** — unstated.

---

---

## Knowledge

### 6. Technical depth (researched)

#### 6.1 SPO-15 family
The DCS-EA text is itself the deepest public description found this pass and is treated here as the
primary source; no independent T1–T3 document was located. The **L006 / L006LM "Beryoza"** designation
and the **±30° elevation** figure are corroborated by `DCS-FM p.13` and `p.31` independently of the EA
chapter.

#### 6.2 BVP-30-26
*"Two BVP-30-26 passive countermeasure blocks with 60 **PPI-26-1V** countermeasure cartridges
(26 mm)"*; the 9-13 uses **BVP-30-26M** blocks. Sources: Russian aviation encyclopaedic sites
(`ot-a-do-ya.org`, `snariad.ru`) — **T4**, consistent with `DCS-FM p.13`.

#### 6.3 What research did **not** produce
No public source for: chaff/flare **split**, **programme timing** (burst count, salvo count, intervals),
**cartridge burn time or RF payload**, or the **firing pattern** of the GROUND/FHS/RHS programmes.

---

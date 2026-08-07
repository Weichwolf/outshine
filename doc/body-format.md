# The body format — one declaration from furniture to rockets

> Owner, 2026-08-05: *„du denkst dir ein eigenes Modul-Format aus das für Fische, Schlangen, Pferde,
> Wölfe, Menschen, Sackkarren, Kutschen, Autos, Flugzeuge, Raketen und Möblierung funktioniert."*

Spec-first. Nothing here is built. This file states the contract; `## State` stays empty until it is.

## Spec

### 0. Why a format at all, and why not an existing aircraft dialect

The obvious candidate — a mature flight-dynamics XML — is excellent **and it is an aircraft dialect**:
`<aerodynamics>` with axis-wise force
tables, `<flight_control>` channels, `<propulsion>` with tanks and thrusters, `<ground_reactions>` with
gear. Reading it means implementing its property tree, its `<function>` expression trees and its channel
semantics — and that interpreter, not the physics, is the bulk of the library. A fish has no channel.

This format asks the opposite question: **what is the smallest set of declarations that spans the
owner's list?** The answer is five, and the list collapses onto them:

| The list | Segments | Joints | Contacts | Force sources | Medium |
|---|---|---|---|---|---|
| Möblierung (a chair, a crate) | 1 | — | resting | — | air |
| Sackkarre | 1 + 2 wheels | 2 revolute | wheel | pulled (external) | air |
| Kutsche | body + 4 wheels + drawbar | 5 | wheel | pulled | air |
| Auto | body + 4 wheels | 4 revolute, 2 steered | wheel | drive torque | air |
| Flugzeug | body + gear + surfaces | gear + control | gear | thrust, aero | air |
| Rakete | 1, mass falling | fins optional | — | thrust, aero | air → vacuum |
| Fisch | 5–9 chain | revolute | — | body wave | water |
| Schlange | 12–20 chain | revolute | ground friction | body wave | air |
| Pferd, Wolf, Mensch | 12–20 tree | revolute + limits | foot | actuator per joint | air |

**Five declarations, ten creatures.** What differs is which are populated — and *that* is the same
pattern this tree already uses for MFD pages, telemetry columns and pilot genes: **a body declares what
it has; nothing is inherited.** See `doc/architecture.md` on the module catalogue.

### 0.1 The subjects, the three axes, and the acceptance bar

> Owner, 2026-08-05: *„wir müssen einen Wolf, einen Menschen, Autos, Hubschrauber, Panzer, Bierdeckel,
> Plattenspieler, Dampfmaschine simulieren können. Gut genug um glaubwürdig zu wirken in **Bewegung,
> Entscheidung und Darstellung**. Deklarativ über Position, Masse, Impuls, 3D-Modell, Materialien und
> **Gehirn**. Auch eine Dampfmaschine darf ein Gehirn haben. **Physik muss für grafische Darstellung
> reichen.** Das LLM kann anhand Metadaten wie Alter, Laufleistung, Energiebezug entscheiden, ob sie
> kaputt geht."*

**Believability has three axes, and they are judged separately** — this is what §4's list A was missing:

| Axis | What must be credible | Judged by |
|---|---|---|
| **motion** | it moves the way that thing moves | list A where someone would notice; „it does not look wrong" everywhere else |
| **decision** | it acts the way that thing acts | the brain — regulation or LLM (`doc/mods.md` §2.1) |
| **representation** | it looks like that thing | the asset ladder, the modeller/critic pair |

A beer mat fails on *representation* if it is the wrong thickness and on *motion* if it does not flutter;
it has no decision axis at all. A wolf can be perfect on all three and still be wrong, because a wolf that
moves and looks right but decides like a machine is the failure everyone notices first.

**And the physics bar is now stated, which it never was:**

> **Physics must suffice for graphical representation.**

That is the whole requirement outside list A. Not fidelity, not a stated error band against some other
solver — *enough that the picture is credible.* A beer mat needs to land flat and skitter; nobody integrates its
boundary layer. This collapses the solver problem by an order of magnitude and it is the reason the
oracle switch (§3) is affordable at all. List A remains the hard gate exactly where a knowledgeable
person checks — a vehicle's cornering speed, a person's step length — and nowhere else.

### 0.2 What a body declares, and the brain

The five declarations of §1 (SEGMENT, JOINT, CONTACT, FORCE SOURCE, MEDIUM) cover motion. The owner's
list names three more, and together they are the whole entity:

| Declaration | Covers |
|---|---|
| position, mass, impulse | §1's segments and their state — the ECS component `Module` may depend on |
| **3D model** | the glTF sidecar; it already shares node names with the joints (§2), so hinge and visual hinge are one name or the format is wrong |
| **materials** | one declaration read twice — appearance (shader, texture) and behaviour (friction, restitution, damping). A tyre's grip and its look are the same material or they will drift apart |
| **brain** | what this entity can decide with, and with what |

**The brain contradicts §2 as it stood**, which said *„No behaviour. A body says what it is; what it does
is the actor's business."* That was too clean. The resolution is not to admit behaviour into the body but
to see what a brain declaration actually is:

> **A body declares that it HAS a brain and what that brain can reach — never what it decides.**

Which is exactly `Module`'s declaration list, one layer down: the capability list is the brain's tool
schema (`doc/mods.md` §2.1), the perception list is its input, and the decider behind it is regulation or
an LLM. Behaviour still is not in the body. The *socket* for behaviour is.

**„Auch eine Dampfmaschine darf ein Gehirn haben"** is the load-bearing sentence, not a joke. It means the
brain is a slot on *any* entity, not a property of things that are alive or driven. A beer mat declares
none. A steam engine may declare one — and what it decides is whether it breaks.

### 0.3 Failure has two channels, and only one of them is physics

> *„Das LLM kann anhand Metadaten wie Alter, Laufleistung, Energiebezug entscheiden, ob sie kaputt geht."*

This is a design inversion and it is correct for a game engine:

| Channel | Cause | Decided by | Example |
|---|---|---|---|
| **violence** | an impact, a hit, an overload in one moment | physics — `DamageModel`, areal energy density in J/m² against zone thresholds | a shell, a crash, an over-g |
| **wear** | age, mileage, duty cycle, energy draw, neglect | **the brain, from metadata** | a bearing that has run twenty years, a gun that has fired too long, a boiler fed badly |

Nobody simulates metal fatigue for a steam engine, and nobody needs to: the entity carries its history as
metadata, and a decider reads that history and rules. That is cheap, it is *explainable* (the reason is
the metadata), and it produces the thing physics cannot — failure that happens at a dramatically right
moment rather than a statistically average one.

**Two rules keep this from becoming a cheat:**

1. **Both channels write through `SystemHealth`**, which is monotone with private mutators and exactly
   one friend. A narrative failure is still a failure that cannot heal itself. If the brain could set
   health directly, everything the tree built would be gone.
2. **The metadata is state, not narration.** Age, mileage and energy draw are accumulated by the
   simulation like fuel is. A brain may *read* them; it may not invent them.

### 1. The five declarations


**SEGMENT** — a rigid piece. Mass, inertia tensor (or a shape it is derived from), a reference frame.
One is the root. Mass may be time-varying (a rocket's burn, a tank's fuel), declared as a rate or a
schedule, never written from outside.

**JOINT** — how two segments are held together. Kind (revolute, prismatic, fixed, ball), axis, limits,
and **who drives it**: `free` (a trailer wheel), `actuated` (a muscle, a servo, a steering rack) or
`pulled` (a drawbar — the constraint transmits, nothing generates). This one field is the whole
difference between a horse and a carriage.

**CONTACT** — where the body touches the world. A point or a shape, with a material: stiffness, damping,
friction along and across. A wheel is a contact with a rolling direction. A foot is a contact that comes
and goes. A resting crate is contacts and nothing else.

**FORCE SOURCE** — what pushes. Each is a declared generator on a named segment:
`thrust` (magnitude along an axis), `surface` (an aerodynamic or hydrodynamic panel: area, normal, and
a lift/drag curve), `drive` (torque into a joint), `wave` (a travelling joint-angle pattern — the one
generator that makes a fish and a snake the same animal), `buoyancy`.

**MEDIUM** — density, viscosity, flow. Air with an atmosphere table, water with a current, vacuum with
none. `surface` and `buoyancy` read it. **A fish is an aircraft in a denser medium with its surfaces on
a driven chain**, and that sentence is the format's justification.

### 1.1 Wind is not a vegetation feature — it is MEDIUM plus JOINT plus `surface`

> Owner, 2026-08-06: *„Windparameter generisch halten. Das ist nicht nur für Vegetation nützlich sondern
> auch für Stoffe und bewegliche Teile. Ein Windrad und ein Baum müssen gleich funktionieren."*

**A wind system that lives inside a tree library is the wrong shape**, and §1 already shows why: the
three declarations wind needs are all there. `MEDIUM` carries the flow, `surface` converts flow into
force on a panel, and `JOINT` decides how the segment answers. **The wind model is identical across
subjects; only the joint declaration differs.**

| Subject | Segments | Joints | Force source |
|---|---|---|---|
| tree | trunk · branch · twig | **`sprung`** — elastic, stiffness falling outward | `surface` on leaf panels |
| **wind turbine** | mast · nacelle · blade | mast `sprung`; rotor **`free`** about its axis | `surface` on the blades — *the same panel type as a leaf* |
| flag, awning, cloth | a chain of panels | `sprung`, very low stiffness | `surface` per panel |
| shutter, gate, door | one panel | `revolute free` with limits | `surface` |
| chain, rope, hanging lamp | a chain | `sprung` ≈ 0, gravity-dominated | `surface`, small |
| grass blade | 2–3 segments | `sprung` | `surface` |

A turbine and a tree differ in **one field**: the rotor joint is `free` about a fixed axis, the branch
joint is `sprung`. Everything else — the flow field, the panel integration, the LOD treatment — is
shared. That is the same test §0 applies to every subject in this format.

**Three things are genuinely missing and must be added:**

1. **`JOINT` needs a fourth driver: `sprung`.** Today the field is `free` / `actuated` / `pulled`. A
   branch is none of them — it deflects under load and returns. Declared as stiffness plus damping about
   the joint axes, so the response is a physical property of the joint, not a shader constant. A stiff
   `sprung` is a mast, a soft one is cloth, and near zero it becomes a pendulum. **One field spans the
   whole table above.**
2. **`MEDIUM.flow` must be time-varying.** Today it reads as a steady current. Wind is mean plus gust
   plus turbulence, and it must be a **closed-form function of position and time** — no stored state.
   That is what lets any segment evaluate `flow(t − Δt)` for motion vectors even when it did not exist
   in the previous frame, which is exactly the correspondence problem a cluster-LOD renderer otherwise
   has ([`render/lod.md`](render/lod.md)).
3. **Response amplitude must attenuate with LOD.** A cluster that merges a thousand leaves into ten
   triangles must carry the *branch* motion, not the *leaf* motion — otherwise the silhouette flickers
   at a frequency it no longer resolves. The declaration is per joint depth, so the renderer picks the
   deepest level its current detail still resolves. This is a property of the body, not of the tree
   library, and a distant turbine's mast sway follows the identical rule.

**What this buys beyond vegetation:** weather stops being decoration. The same declared flow that bends
a branch turns a rotor, lifts a tarpaulin, slams a shutter and sways a crane jib — and an epoch that
raises decay ([`goal.md`](goal.md)) can lower a joint's stiffness instead of swapping a model.

### 2. What the format does NOT do, and why

- **No behaviour — but a socket for it.** A body says what it *is* and what its brain can *reach*
  (§0.2); what it *decides* is the decider's business (`Pilot`'s successor, or an LLM). Muscles are
  actuated joints, not intentions.
- **No damage model.** `DamageModel` already owns the violence channel, in J/m² against zone
  thresholds, and it is already half-general (see `doc/architecture.md`). Segments give it what it lacks:
  an addressable part instead of a 1-D interval on a longitudinal axis. A wolf's left foreleg is a
  segment. The **wear** channel (§0.3) is not physics at all and is decided from metadata.
- **No visual mesh.** But it **shares node names** with the glTF sidecar: the assets already declare
  `ctl.aileron.l`, `gear.main.r`, `turret.yaw` with hinge points at the real hinge. Physics joint and
  visual joint are the same name or the format is wrong.

### 3. The rule that makes it trustworthy

**Every number carries its origin**, as everywhere in this tree: derived (with the formula), measured
(with the measurement), or `[SET]`. A body file with an unmarked number is a defect.

And the one that matters more: **there is no second solver to check against any more.**

> **This section was rewritten on 2026-08-06 and the change is a loss, not a refinement.** It used to
> say *„the third-party solver becomes the oracle, not the dependency"*, and it laid out a staged switch
> in which every body that also existed as that solver's model was flown both ways and the difference
> measured. **That solver is deleted.** The staging is gone with it, and so is the free instrument. What
> remains is the part that was always the real work — and it is now the *only* work.

**The order is fixed, and the first item is not the solver:**

| # | Step | Why it comes first |
|---|---|---|
| 1 | **write list A** — for each body class, the places a knowledgeable person checks, each with a band **and a published source** | §4. Without a second solver there is no free comparison, so every band has to come from literature, a manufacturer figure or a measurement of the real thing. That makes list A harder than it was and strictly more honest: a band whose source is another simulator was never evidence about the world |
| 2 | build the harness that judges a declaration against its band | **nothing exists.** The previous runner and its declarations were deleted with the combat layer; a band is a number in a table and needs a harness that reads it |
| 3 | the solver | only meaningful once 1 and 2 say what „still believable" means as numbers |

**And the warning that came out of the previous instrument must be carried, because it is the reason
this order is not negotiable:** seven declared anchors sat OUTSIDE their bands behind a green gate, and
five of six deliberately corrupted model values passed the whole net. A gate that does not fail on a
known-wrong value is not a gate. Whatever judges list A must be demonstrated to go red — by breaking a
value on purpose and watching it.

### 4. Acceptance — and the criterion is BELIEVABILITY, not fidelity

> Owner, 2026-08-05: *„in einem Game Engine ist immer **alles falsch** — die Frage ist, ist es noch
> **glaubhaft**."*

This corrects the obvious version of this section, which asked for closeness to another solver inside a
stated band. That is the wrong target and it is expensive in the wrong places: a coefficient deep in a
regime may be far off and nobody — player, expert or judge — can tell, while a quantity that is felt in
the first second of control is not forgivable at all.

**So the criterion splits in two, and only the first is a hard gate:**

| | Contract | Measurement anchor |
|---|---|---|
| **A** | **It does not break where someone would notice.** Believability is not vague — it is a list, written down before the solver, of the places a knowledgeable person checks for THAT body class: a person's walking and running speed, step length and stair behaviour; a vehicle's acceleration, braking distance and the speed at which it lets go in a corner; an aircraft's cornering speed and stall onset. **Each gets a band and a published source.** | one declaration per claim, judged by a harness that does not exist yet, each band stated as a number |
| **B** | Everywhere else, nothing is claimed at all | there is no oracle to record a delta against. What replaces the delta table is the honest sentence: *outside list A this body is unmeasured*, said once rather than implied |
| | The format spans the list | one body file per row of §0, each loading and stepping |
| | Furniture costs nothing | a body with no force source and no joint does no integration work; measured µs/sim-s |
| | Determinism holds | `--threads 1/2/4` byte-identical, as everywhere else |

**And the oracle question gets its answer from this, too.** „What could be wrong while all ten harnesses
pass?" is unanswerable in general and answerable in this frame: *anything outside list A*. The switch is
defensible exactly when list A is honest — which makes writing list A, not writing the solver, the
gating work.

Believability was always the standard. A solver was one way to reach it, never the definition of it —
which is why removing the solver removed a convenience and not the criterion.

## State

Nothing built. This file is the contract only.

## Gaps

- **There is no oracle at all.** Every band in list A must now come from a published source or a
  measurement of the real thing. That is the gate on everything else, and it got harder on 2026-08-06.
- **Multibody with constraints is the hard part** — segments and joints are easy, a stable solver for a
  20-segment wolf at 100 Hz is not. Whether this tree needs one, or whether creatures are kinematic
  (animated) with only their root integrated, is undecided and cheap to decide wrongly.
- **`SystemId` is a closed aircraft enum** ([`core.md`](core.md) §Gaps 4) and blocks the segment/part
  idea downstream.
- **The single state writer lost its shape.** The one private constructor with one friend that made
  „nothing writes state behind the simulation" a compile error was the deleted solver's. Whatever spawns
  a body must re-earn it ([`conventions.md`](conventions.md)).
- **The wear metadata does not exist.** §0.3 needs age, mileage, duty cycle and energy draw accumulated
  as ordinary simulated state; nothing accumulates any of them today.
- **„Sufficient for graphical representation" is not yet a measurement.** §0.1 states the bar in words.
  Outside list A it has no anchor, and a bar without an anchor is how seven declared rows sat out of
  band behind a green gate.
- **The first body is a PEDESTRIAN, and this file is written for vehicles.** Roadmap R1 needs a human
  at eye height that walks over the DEM. Its list A is short (walking and running speed, step length,
  eye height, how it meets a slope and a step) and none of it is written.

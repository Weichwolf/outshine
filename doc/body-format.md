# The body format — one declaration from furniture to rockets

> Owner, 2026-08-05: *„du denkst dir ein eigenes Modul-Format aus das für Fische, Schlangen, Pferde,
> Wölfe, Menschen, Sackkarren, Kutschen, Autos, Flugzeuge, Raketen und Möblierung funktioniert."*

Spec-first. Nothing here is built. This file states the contract; `## State` stays empty until it is.

## Spec

### 0. Why a format at all, and why not JSBSim's

JSBSim's XML is excellent **and it is an aircraft dialect**: `<aerodynamics>` with axis-wise force
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

That is the whole requirement outside list A. Not fidelity, not a stated error band against JSBSim —
*enough that the picture is credible.* A beer mat needs to land flat and skitter; nobody integrates its
boundary layer. This collapses the solver problem by an order of magnitude and it is the reason the
oracle switch (§3) is affordable at all. List A remains the hard gate exactly where a knowledgeable
person checks — an F-16's corner speed — and nowhere else.

### 0.2 What a body declares, and the brain

The five declarations of §1 (SEGMENT, JOINT, CONTACT, FORCE SOURCE, MEDIUM) cover motion. The owner's
list names three more, and together they are the whole entity:

| Declaration | Covers |
|---|---|
| position, mass, impulse | §1's segments and their state — the ECS component `FBModule` may depend on |
| **3D model** | the glTF sidecar; it already shares node names with the joints (§2), so hinge and visual hinge are one name or the format is wrong |
| **materials** | one declaration read twice — appearance (shader, texture) and behaviour (friction, restitution, damping). A tyre's grip and its look are the same material or they will drift apart |
| **brain** | what this entity can decide with, and with what |

**The brain contradicts §2 as it stood**, which said *„No behaviour. A body says what it is; what it does
is the actor's business."* That was too clean. The resolution is not to admit behaviour into the body but
to see what a brain declaration actually is:

> **A body declares that it HAS a brain and what that brain can reach — never what it decides.**

Which is exactly `FBModule`'s declaration list, one layer down: the capability list is the brain's tool
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
| **violence** | an impact, a hit, an overload in one moment | physics — `FBDamageModel`, areal energy density in J/m² against zone thresholds | a shell, a crash, an over-g |
| **wear** | age, mileage, duty cycle, energy draw, neglect | **the brain, from metadata** | a bearing that has run twenty years, a gun that has fired too long, a boiler fed badly |

Nobody simulates metal fatigue for a steam engine, and nobody needs to: the entity carries its history as
metadata, and a decider reads that history and rules. That is cheap, it is *explainable* (the reason is
the metadata), and it produces the thing physics cannot — failure that happens at a dramatically right
moment rather than a statistically average one.

**Two rules keep this from becoming a cheat:**

1. **Both channels write through `FBSystemHealth`**, which is monotone with private mutators and exactly
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

### 2. What the format does NOT do, and why

- **No behaviour — but a socket for it.** A body says what it *is* and what its brain can *reach*
  (§0.2); what it *decides* is the decider's business (`FBPilot`'s successor, or an LLM). Muscles are
  actuated joints, not intentions.
- **No damage model.** `FBDamageModel` already owns the violence channel, in J/m² against zone
  thresholds, and it is already half-general (see `doc/architecture.md`). Segments give it what it lacks:
  an addressable part instead of a 1-D interval on a longitudinal axis. A wolf's left foreleg is a
  segment. The **wear** channel (§0.3) is not physics at all and is decided from metadata.
- **No visual mesh.** But it **shares node names** with the glTF sidecar: the assets already declare
  `ctl.aileron.l`, `gear.main.r`, `turret.yaw` with hinge points at the real hinge. Physics joint and
  visual joint are the same name or the format is wrong.

### 3. The rule that makes it trustworthy

**Every number carries its origin**, as everywhere in this tree: derived (with the formula), measured
(with the measurement), or `[SET]`. A body file with an unmarked number is a defect.

And the one that matters more: **JSBSim becomes the oracle, not the dependency.** Any body that also
exists as a JSBSim model is flown both ways and the difference is *measured* on the harnesses this tree
already has — corner speed, envelope, the 296-mission regression, determinism over thread counts.
CLAUDE.md principle 1 forbids *unmeasured* deviation; an oracle preserves its purpose while its letter
changes. **The owner made that change on 2026-08-05** — *„und danach die Ablösung von JSBSim"* — and
CLAUDE.md principle 1 now reads accordingly. It is **staged, not switched**: JSBSim stays linked and the
old rule applies unchanged until list A (§4) is written and green.

**The order is fixed and the first two items are not the solver:**

| # | Step | Why it comes first |
|---|---|---|
| 1 | read the **7 out-of-band anchors** in `sim/test/modules/air/envelope.json` | replacing physics while the measuring instrument is known-broken in seven places would make every later result unreadable. Model defect or band too tight — decided one by one, **never by widening a band** |
| 2 | **write list A** — corner speed, sustained turn rate, stall onset, roll rate, take-off and landing speeds, acceleration, ceiling; each with a band and a source | §4: *„writing list A, not writing the solver, is the gating work."* The instrument for it now exists — list A is `envelope.json`'s shape, one declaration per claim |
| 3 | the solver, JSBSim flown beside it as the oracle | only meaningful once 1 and 2 say what „still believable" means as numbers |

### 4. Acceptance — and the criterion is BELIEVABILITY, not fidelity

> Owner, 2026-08-05: *„in einem Game Engine ist immer **alles falsch** — die Frage ist, ist es noch
> **glaubhaft**."*

This corrects the obvious version of this section, which asked for closeness to JSBSim inside a stated
band. That is the wrong target and it is expensive in the wrong places: a drag coefficient at Mach 0.83
may be 8 % off and nobody — pilot, expert or judge — can tell, while a corner speed 10 kt off is felt in
the first turn.

**So the criterion splits in two, and only the first is a hard gate:**

| | Contract | Measurement anchor |
|---|---|---|
| **A** | **It does not break where someone would notice.** Believability is not vague — it is a list, written down before the switch, of the places a knowledgeable person checks: corner speed, sustained turn rate, stall onset, roll rate, take-off and landing speeds, time to accelerate, service ceiling. **Each gets a band and a source.** | the existing harnesses (`test-corner`, `test-air`, `test-mig29`, the envelope tests), each with its band stated as a number |
| **B** | Everywhere else, the difference is *recorded*, not bounded | a published delta table against the oracle. A 20 % difference in a quantity nobody perceives is a note, not a failure |
| | The format spans the list | one body file per row of §0, each loading and stepping |
| | Furniture costs nothing | a body with no force source and no joint does no integration work; measured µs/sim-s |
| | Determinism holds | `--threads 1/2/4` byte-identical, as everywhere else |

**And the oracle question gets its answer from this, too.** „What could be wrong while all ten harnesses
pass?" is unanswerable in general and answerable in this frame: *anything outside list A*. The switch is
defensible exactly when list A is honest — which makes writing list A, not writing the solver, the
gating work.

This also resolves the tension with CLAUDE.md's *„F-16 zuerst. Referenz ist das MODELL, nicht der echte
Jet"*: the model's properties were already declared **accepted rather than defects**. Believability was
always the standard; JSBSim was the way to reach it, not the definition of it.

## State

Nothing built. This file is the contract only.

## Gaps

- **The oracle's sharpness is unanswered** and it is the gate on everything else.
- **Multibody with constraints is the hard part** — segments and joints are easy, a stable solver for a
  20-segment wolf at 100 Hz is not. Whether this tree needs one, or whether creatures are kinematic
  (animated) with only their root integrated, is undecided and cheap to decide wrongly.
- **`FBSystemId` is a closed 14-entry aircraft enum** and blocks the segment/part idea downstream.
- **The wear metadata does not exist.** §0.3 needs age, mileage, duty cycle and energy draw accumulated
  as ordinary simulated state; nothing accumulates any of them today.
- **„Sufficient for graphical representation" is not yet a measurement.** §0.1 states the bar in words.
  Outside list A it has no anchor, and a bar without an anchor is how seven envelope rows sat out of band
  behind a green gate.

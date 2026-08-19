Type: feature
Area: core
Tags: scope

**outshine runs a declared script, and the subset is written down in both directions**

A small bounded interpreter in `src/core`, with **two consumers and one capability**: a UI declaration
gains a handler that does something rather than only naming an action, and the CSS corpus's harness
gains the ability to run a document's own `onload` — which [MEASURED] is what 18 of the 24 cases the
`css-overflow` and `css-sizing` families contribute are waiting on.

**THE LIBRARY EXECUTES AND THE CONSUMER SUPPLIES THE WORLD.** Every name a script can reach comes from
a `Host` the consumer implements: what `document` is, what a member means, what a call does. Nothing in
`src/core/Script.h` knows a document, an element, an actor or a button — the same rule the renderer
follows about content nouns, applied to a language. A `Ref` is an opaque handle the host owns and the
interpreter never looks inside.

## The subset, and it is the scope

**In:**

| | |
|---|---|
| values | number · quoted text · `true` / `false` · `null` / `undefined` · an opaque host handle · nothing — **the literals are the LANGUAGE's and not the host's**, which the corpus found by putting cases outside the subset with *reaches a name this runner does not provide: false* |
| expressions | variables · member reads · calls · `+ - * / %` · `< <= > >= == !=` · `&& \|\| !` · parentheses |
| statements | assignment to a name · assignment to a member · a call · `if` / `else` · `while` · blocks |
| truth | zero and empty text are false, a handle is true, nothing is false — **stated here rather than inherited from a language nobody named** |

**Out, and each is a decision:**

functions a script defines · objects and arrays a script makes · `for` · closures · `try`/`catch` ·
prototypes · `var`/`let`/`const` scoping — a script here assigns a name and the name exists ·
regular expressions · `eval` · anything asynchronous. **Every one of them is how a handler becomes a
program nobody can bound.**

## What must be true

- [ ] **Everything that grows states its bound**, and each is a number somebody chose: `kMaxTokens`
  4096 · `kMaxNodes` 2048 · `kMaxDepth` 32 · `kMaxSteps` 100 000 · `kMaxNames` 128 · `kMaxArgs` 8. A
  script that reaches one is **refused with the bound named**, never truncated — a program cut in half
  is a program that did something else
- [ ] **A parse is once and a run is many.** `Read` produces a tree and `Run` walks it, so a handler
  never meets the parser on an event and the text may go once it is read
- [ ] **A call the host does not know is a refusal**, not a silent nothing
- [ ] **The frame path is not where a script runs.** A handler runs on an event; a fixture runs when a
  case is prepared. Neither is inside `RenderFrame`, and that is stated because it is the one place
  this could become a per-frame allocation

## Why it is not the third path content ships a program by

`CLAUDE.md` refuses a material that ships a program: *generator bakes, or renderer implements, and
there is no third path.* That rule is about the RENDERER's pipeline state, which a material may not
switch. A handler on a declared surface switches nothing in the renderer — it reads and writes values
the consumer owns, and the picture is still a function of the declaration the consumer then hands over.

## Comments

The owner asked for this after the round measured that 18 of 24 new corpus cases are script-driven.
The shape chosen was **one interpreter with two consumers** over two narrower answers: a DOM subset cut
to exactly what WPT uses would have been a fit to the corpus rather than a capability, and a purely
declarative binding language would have solved neither the corpus nor game logic.

## A script is bound to a host, and the reach is a value rather than a name

**The interpreter has no globals of its own, and that is the security property rather than a setting.**
The only door is `Host::Global`, which the consumer builds — so a script cannot escape, because there
is nothing to escape from. There is no ambient authority to leak, which is a stronger statement than
*sandboxed* and it is a property of the shape rather than of a check.

**Bound to a HOST and not to an object.** A consumer writes one host per KIND of thing and gives each
instance its own `Ref`. A thousand light switches then cost one host and not a thousand — the same
sentence `CLAUDE.md` makes about one key serving a million instances, applied to authority.

**THE REACH MUST BE A VALUE AND NEVER A NAME, and this is the rule a consumer can get wrong.** A host
exposing `world.lightAt("hall")` lets every switch address every light, and the binding is gone; a host
handing the script a `Ref` to its OWN light makes any other one unspellable. That is the difference
between a capability and a convention, and `Ref` already carries it — the interpreter never looks
inside one and no script can construct one.

**A passcode panel is the case that tests it.** The script decides nothing about the door: it calls an
`unlock()` its own host provides. A panel whose host has none opens nothing, and the call **refuses
loudly** rather than quietly doing nothing — which is `board:1448`'s own rule about an unknown call
arriving where it matters.

**Authority and cost are two bounds and only one of them is here.** A page on a wall costs a layout and
a pass whoever owns it, so the declared size and the redraw-on-change rule in `board:1452` are the other
bound. Neither implies the other.

## A script drives movement, which puts it on the frame path

**The owner's decision, and it changes the COST story rather than the authority one.** A script that
moves something runs once per tick, not once per event — so it is inside the frame, beside every other
term that has to be bounded.

- [ ] **The step bound is per TICK and not per run.** 100 000 steps is generous for a handler and
  absurd for two hundred actors a frame. A tick declares its own number, and a script that reaches it
  is **refused by name** like everything else here — a script quietly cut short would move something
  half way and report nothing
- [ ] **The frame path does not allocate, and that is measured rather than asserted.** `Names_` and
  `Held_` keep their capacity across `clear()`, so a numeric script allocates nothing after its first
  run — which is a claim, and a claim about allocation is worth an instrument
- [ ] **The step comes from the declaration and never from a clock.** `CLAUDE.md`: *if pace decides the
  result, the coupling is a bug.* A movement script is handed the declared `dt` by its host —
  `self.x = self.x + speed * dt` — so two runs of one scenario are one scenario
- [ ] **Authority is unchanged.** `self` is a `Ref` the host gave to that one actor; a script moves what
  belongs to it and nothing else. The tick adds a cost bound, not a reach

*What the other engines call `Update` or `Tick` is this, and the reason to say so is that the shape is
settled rather than invented: what is new here is that the reach is a capability and the cost is a
declared number, and both are checkable.*

## An actor keeps its own state, and the host is the world's capability surface

**A model railway that drives by itself and stops at the station now and then** needs memory between
ticks — a phase, a waiting time. **The program IS that memory.** The consumer holds the `Program`, so
the consumer holds its names; `kMaxNames` bounds them, `Named` reads them out for a save file, and
`Reset` is the explicit door to a fresh run. Persisting them inside the interpreter with no way to see
them would be memory growing per actor where nobody can count it.

- [ ] `Run` keeps the names it assigned; `Reset` clears them. A fixture that wants a clean slate says
  so, and an actor that wants to remember says nothing

## THE SAME INTERFACE THE LLM INTEGRATION WILL USE, and that is the far-reaching part

**`Host` is not a scripting convenience: it is the capability surface of the world.** What an actor can
PERCEIVE is what the host answers; what it may DO is what the host calls. A script and a model differ
in *who chooses the next call* and in nothing else.

**That buys something this repository is otherwise short of: an actor's reach becomes testable without
a model.** Anything a model could do, a script can do — deterministically, offline, in a corpus, on a
Tuesday. A capability only a model can reach is a capability nobody can measure, and a shared host is
what makes sure there is none.

*So the host interface is designed as a WORLD API and not as a language binding: sensors, actuators,
and no ambient authority. The three vocabularies the browser needed — `select`, `suite`, `scroll` --
are the same shape a switch, a door and a train will use.*

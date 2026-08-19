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
| values | number · quoted text · an opaque host handle · nothing |
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

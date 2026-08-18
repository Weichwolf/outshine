Type: feature
Area: scenario
Tags: scope

**A scenario declares whether an asset's own animation plays, is ignored, or is the engine's**

Three answers and all three must work, because all three are things a game does:

| the consumer says | what happens |
|---|---|
| **play it** | the file's own animation drives the subject, which is what an animated prop or a rigged character ships with |
| **ignore it** | the subject stands in its rest pose. glTF states this outright as a client's right -- *client implementations may select an animation entry and pause it on the first frame, play it automatically, or ignore all animations until further user requests* |
| **take it over** | the engine drives the same nodes itself -- a character blending two clips, a door the world opens, a machine whose speed is a simulation's |

**IT IS A DECLARATION AND NEVER A FALLBACK.** Today the reader ignores what it cannot drive, which is
the right SHAPE and the wrong REASON: a consumer that gets a still because this engine has no code for
a channel cannot tell that from a consumer that asked for a still. **The picture is a function of the
declaration**, so the answer belongs to whoever declares the scenario and not to what happens to be
implemented.

## What must be true

- [ ] The three are one enumeration on the consumer's side, not a pair of booleans
- [ ] **`ignore` and `could not drive` are different answers**, and a capability reports both -- the
  reader already counts undriven channels and names their pointers (`board:1392`)
- [ ] **Taking it over does not mean re-reading the file**: the engine drives the same node table the
  file's own animation would have, so a clip from anywhere composes with one from the asset
- [ ] **The corpus always plays.** That is the owner's ruling and it is what the corpus is FOR: a case
  that ignored an animation would be testing that we can ignore one

## Comments

**The corpus's own sequence check is what makes `play` load-bearing** -- *the drawn subject moves over
the declared grid, so the sequence is not a still rendered once per frame and agreeing with the oracle
by construction*. A case whose subject does not move is comparing a still, and the check says so.

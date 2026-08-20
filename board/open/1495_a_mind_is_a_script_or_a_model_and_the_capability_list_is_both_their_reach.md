Type: task
Parent: 1480
Area: world
Tags: scope

**A mind is a script or a model, and the capability list is both their reach**

`board:1448` wrote it down before there was anywhere to put it: **a script and a model differ in WHO
CHOOSES THE NEXT CALL and in nothing else.** So a kind does not declare a programme -- it declares a
**mind**, and the mind chooses `script` or `model`.

```xml
<kind name="settler" tickHz="4">
  <mind chooses="script" programme="wander.js" seed="7"/>
  <may do="walk"/><may do="speak"/>
</kind>

<kind name="mayor" inherits="settler" tickHz="0.2">
  <mind chooses="model" prompt="mayor.md" model="local-small"
        temperature="0.7" tokenBudget="512" latencyBudgetMs="800"/>
  <may do="walk"/><may do="speak"/><may do="trade"/>
</kind>
```

**THE `may do` LIST IS THE FUNCTION-CALLING SCHEMA.** It is already the host capability surface a script
may reach; a model is offered exactly the same set as its tools. **One declaration, two consumers**, and
that is what makes an actor's reach testable without a model at all -- anything a model could do, a
script can do, deterministically, in a corpus, on a Tuesday.

## The three constraints a model brings that a script does not

**A MODEL IS NOT ON THE FRAME PATH AND CANNOT BE.** A script tick costs [MEASURED] p99 0.292 us; a model
call costs hundreds of milliseconds. So a mind that is a model **proposes** and the proposal is applied
when it arrives -- which is `CLAUDE.md`'s own rule that *a worker signals readiness; it never asks a
question*, and the same completion-queue shape the generators already use.

**AN ANSWER OUTSIDE THE DECLARED SET IS A REFUSAL.** A model that names a function no `may do` declares
is refused by name and the actor does what it did before. That is the whole safety property and it is
the shape rather than a check: there is no ambient authority to leak because the tool list IS the
capability list.

**A SCENARIO WITH A MODEL IS NOT DETERMINISTIC AND MUST SAY SO.** `CLAUDE.md`: *the picture is a
function of the declaration, not of the machine.* A `seed` makes a script's mind reproducible; a model's
is not, whatever the temperature, so a scenario that declares one **declares that it is not
deterministic** and the scenario suite's determinism check reads that rather than failing.

## What must be true

- [x] **A kind declares a mind and the mind declares who chooses** -- `chooses="script"` names a
  programme and a seed, `chooses="model"` names a prompt, a model, a temperature and BOTH budgets
- [x] **A kind with no mind thinks nothing**, which is most of them, and that is 0 of 0..N
- [ ] **A model's budgets are enforced and both are published** -- tokens spent against the budget and
  latency against it, in both directions, because a mind with no bound on what it may spend is a term
  the frame cannot carry
- [ ] **A proposal arrives through a completion queue drained at ONE declared point in the frame**, and
  a tick that finds nothing waiting does what it did before
- [ ] **A call outside the `may do` set is a named refusal**, counted per actor so a scenario can see a
  mind that keeps asking for what it does not have
- [ ] **A scenario carrying a model mind declares itself non-deterministic**, and the determinism check
  reads the declaration rather than reporting the model as a defect
- [ ] **A transcript can be RECORDED and REPLAYED**, so a run with a model becomes a run with a script
  for the purpose of a test -- which is the only way a model-driven scenario enters a corpus at all
- [ ] **The host interface is one interface**: outshine declares what it needs of a model -- a prompt, a
  tool list, an answer -- and calls nothing else, the way it does for the GPU and the audio device

## What this may not do

**It may not let a mind reach the world except through its capabilities.** A host exposing
`world.actorNamed("mayor")` gives every mind every actor and the binding is gone; a host handing a mind
a `Ref` to its OWN body makes any other one unspellable. `board:1448` states this and it is the line
that matters most once the thing choosing is a model.

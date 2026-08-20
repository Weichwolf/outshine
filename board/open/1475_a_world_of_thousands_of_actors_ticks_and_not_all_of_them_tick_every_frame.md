Type: feature
Area: world
Tags: perf

**A world of thousands of actors ticks, and not all of them tick every frame**

`board:1448` gave an actor a program: a bounded interpreter, a host that IS the world's capability
surface, a tick that takes the declared `dt` and allocates nothing after its first run. [MEASURED] a tick
costs **p99 0.292 us** and takes **0 bytes** from the allocator, which is 11 418 actors inside a `[SET]`
fifth of a 16.67 ms frame **if every one of them ticks every frame**. That last clause is the whole item:
an open world does not run that way, and no engine that ships one runs it that way either.

## The mechanism, looked up rather than recalled, and the two answers are different

| | what it does | what transfers |
|---|---|---|
| **Unreal** | `FTickFunction::TickInterval` and `AActor::SetActorTickInterval` let a thing tick at a minimum interval instead of every frame; the **Significance Manager** buckets actors by significance and lowers or disables their ticking, and the Particle System Component uses it to deactivate tick functions outright and reactivate them when significance returns | **all of it.** A tick rate is a level of detail, and this engine already has one currency for detail |
| **RAGE** | `scrThread::Run(opsToExecute)` takes an op budget per invocation, and a script **yields** with `WAIT`, its thread state surviving across frames | **the budget as a PARAMETER, not the yield.** Yielding needs a continuation, and `board:1448` declares no closures and no coroutines -- so a script here runs to completion or is refused |

**A fixed number is right for exactly one job and it already has it.** `kMaxSteps` is the runaway
detector -- Unreal spells the same thing `GMaximumScriptLoopIterations`, a config value whose job is to
kill a broken script. **It is not a schedule**, and the round that tried to make it one wrote down why:
a script cut short half way has moved something and reported nothing.

## What must be true

- [ ] **A tick is a declared RATE and not a frame.** An actor states how often it wants to run, and the
  scheduler answers how often it did -- **both directions published**, like every capability here
- [ ] **The number of actors that tick in one frame is BOUNDED and the bound is a number somebody
  chose.** *Everything that grows states its bound*, and a world that streams actors in is exactly the
  thing that grows
- [ ] **Significance is the ONE currency this engine already has**, or it is a second one. Projected
  error is what decides geometry; whether it also decides tick rate is a question this item must answer
  rather than assume -- *selection by distance ratio is the technique this engine refuses*, and a tick
  rate keyed on distance would be that refusal broken in a new place
- [ ] **An actor that did not tick this frame is not an actor that stopped.** It carries `dt` since it
  last ran, which is the same declared step and not a clock
- [ ] **The schedule is deterministic.** Two runs of one scenario tick the same actors in the same
  order, or *the mathematics is deterministic* is false the moment a world has actors in it
- [ ] **The cost is measured over a moving camera** at p50/p95/p99 with the actor count named, in the
  scenario suite -- a mean over ticks is not a frame budget

## What this item may not do

**It may not quote 11 418 actors as a capability.** That number is one tick's cost multiplied by a
`[SET]` share, over a population of ONE script in a frame test. The number this item owes is a
distribution over a declared run with a named actor count, and until it exists the honest statement is
that the per-tick cost is known and the schedule is not built.

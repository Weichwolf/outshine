Type: issue
Area: world

**Whether the physics is ours or a package the host provides**

`board:1509` declares the vehicle. **What integrates it is a separate question and it is the owner's**,
because it decides how much of this engine is written here.

## What is actually needed, which is less than "a physics engine"

**A car on a road is a specific model and not a general solver**: one rigid body with six degrees of
freedom, four suspension struts, tyre forces from slip, and a query against the world's surface. **A
train is simpler still** -- one degree of freedom plus the vertical. Neither needs a constraint solver,
a joint hierarchy, or continuous collision between arbitrary convex hulls.

**JSBSim makes exactly this point by existing**: it flies airliners with no general physics engine
underneath, because a vehicle in its own medium is a force model and an integrator.

## The options

| | what it costs | what it buys |
|---|---|---|
| **A. ours, specific** | a rigid-body integrator, a suspension model, a tyre model -- weeks, not months | **determinism to the bit**, one medium dial as `CLAUDE.md` already promises, no dependency, and a model we can reason about when a finding is disputed |
| **B. a package** (Jolt, Bullet, PhysX) | a host dependency, and `CLAUDE.md` allows one that the host provides | mature collision, joints and stacking -- **none of which this instrument needs** |
| **C. ours now, a package later** if stacking, ragdolls and debris arrive | the interface is written twice if the shapes differ | the decision is deferred to when there is a measurement |

## THE OWNER'S ANSWER: A. Everything is written here, and no further dependency.

*Recorded before the reasoning below, because the reasoning was the recommendation and the ruling is
what decides it.* **The engine takes no physics package.** The integrator, the suspension, the tyre and
the medium dial are ours, and `board:1509`'s vehicle declaration is what keeps that from meaning "a car
in the code".

**What the ruling costs, stated so it is not discovered later**: collision between arbitrary bodies,
stacking, joints and ragdolls are all work this repository owes itself the day a scenario asks for them.
**`CLAUDE.md`'s answer to that is already written** -- *something missing is a task, not a limit* -- so
they are tasks when a measurement asks, and until then they are absent rather than borrowed.

## The recommendation this replaced, and its reason still stands

**The drive suite must re-drive a crash from a seed and find the same crash.** A general engine's
determinism is a mode you enable and a promise across versions and platforms that nobody owes you;
**ours is a property of the code.** And `CLAUDE.md` already commits to *one physics system carrying
walking, driving, flying and swimming* -- **no shipped library does that as one dial**, so B buys a
collision solver and still leaves the medium model to write.

**What would change the recommendation**: the day a scenario needs a hundred crates to fall over
convincingly. *That is a real requirement and it is not this instrument's* -- and when it arrives it
arrives with a measurement, which is the right time to take B.

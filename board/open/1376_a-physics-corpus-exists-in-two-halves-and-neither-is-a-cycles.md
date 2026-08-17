Type: issue
Area: scenario
Tags: scope, instrument

**A physics corpus exists in two halves, and neither of them is a Cycles**

**The owner's question**: a game engine needs physics -- is there a corpus like the Khronos glTF one to
drive its development against? **Two exist and they supply different halves**, which is the whole of the
answer.

| | supplies | does not supply |
|---|---|---|
| **IFToMM Library of Computational Benchmark Problems** (`iftomm-multibody.org/benchmark`) | the **oracle half**: multibody problems WITH reference solutions, each carrying a defined error measure and a CPU time, submitted and comparable | game scenes. Mechanisms, pendulums, linkages |
| **MuJoCo Menagerie** (`google-deepmind/mujoco_menagerie`) | the **declaration half**: curated, versioned models with masses, inertias, joints and collision geometry -- the exact shape of `glTF-Sample-Assets`, with a CHANGELOG and a commit to pin | MJCF rather than a game format, and no verdict on whether a simulation is right |

## Why the glTF pattern does not transfer, and it is structural

**Cycles is an oracle because rendering is a well-posed integral.** Contact-rich rigid-body simulation is
not: contact forces on a redundantly supported body are **not unique**, and the result is **chaotic** --
two correct engines diverge within a second. A corpus that compared trajectories against a reference
would be measuring how closely we reproduce somebody else's integrator choice.

## And for a GAME engine the question mostly dissolves

Game physics is not judged on trajectory accuracy. Every one of these is decidable **without a reference
simulator**:

| | |
|---|---|
| **stability** | a stack stands or falls; nothing explodes; nothing jitters at rest |
| **non-penetration** | penetration depth within tolerance, per frame, against nothing |
| **determinism** | the same run twice, bit for bit -- and the result independent of body order |
| **conservation** | energy, momentum, angular momentum without dissipation. **A drifting integrator is measurably wrong with no reference at all** |
| **the budget** | 16.67 ms at p50/p95/p99 over a moving camera -- the same fourth constraint the renderer answers to |
| **closed forms** | projectile · pendulum, small-angle and exact · torque-free rigid rotation (Dzhanibekov) · rolling without slipping · the static-friction angle `tan θ = μ`, exactly |

**This tree already runs that shape**: `Triangle` is decided against `rho*L` in closed form, not against a
second image. For physics it would be the main road rather than the fallback.

## The recommendation is shaped by OUR need and not by what happens to exist

`CLAUDE.md`: *one physics system carries walking, driving, flying and swimming*. That is a character
controller, a vehicle and buoyancy on one system -- **not a robot arm**. Menagerie and IFToMM both aim
elsewhere.

- [ ] **The corpus we need is mostly DECLARED HERE**, the way `test/outshine/render/` declares its own
  subjects, and the verdicts are ours: invariants and budgets rather than reference paths
- [ ] **IFToMM is a verification annexe** for the joint solver, where we have one, and it is worth its
  own case class when we do
- [ ] **Menagerie is worth pinning only if articulated bodies enter the engine.** A model zoo we cannot
  spend is a dependency without a claim

**Filed and worked around, never waited on**: phase one is the glTF corpus and nothing here blocks it.

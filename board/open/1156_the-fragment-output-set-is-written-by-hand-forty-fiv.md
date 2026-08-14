Type: bug
Area: render
Tags: instrument

**The fragment output set is written by hand, and a forgotten write is an undefined attachment**

Three target families are each set by their own macro at every fragment entry point:
`SUBJECT_SET_VELOCITY`, `SUBJECT_SET_SHADING_NORMAL` / `SUBJECT_NO_SHADING_NORMAL`, and now
`SUBJECT_SET_SURFACE_IDENTITY` — **15 arms times 3 families**, hand-written, with **15 textually identical
`SUBJECT_SET_SURFACE_IDENTITY(o, surface);` lines** among them. A sixteenth arm, or a fourth target, is
one omission away from a fragment that writes an attachment the pass declared and the shader did not.

**The failure is silent in the direction that matters.** An undeclared output renders correctly and
Metal's validation aborts on it — that is `board:1121`, already paid for once. An **un**written output is
worse: it is whatever the target held, it reads as a plausible value, and it is caught only by the
`~validated` arm. `SceneSurfaceIdentity` is the case that shows it, because a stale index **names a
material that exists** and a reader cannot tell it from an answer.

**THIS IS THE HOUSE CRITERION FAILING, NOT A TIDINESS COMPLAINT.** The engine's stated preference is the
shape that makes a mistake unspellable over the rule that forbids it. Three macros are **three rules a
checker counts**; what is wanted is **one rule the compiler holds**. `board:1141` plans two more targets,
which makes it five families over fifteen arms — **seventy-five hand-written sites** for a set that is
fully determined by the compiled plan.

**What would be right instead: the output set is BUILT, never assigned.** One function per arm family that
takes every field the plan can attach and returns the struct — the colour, the velocity, the shading
normal or its declared absence, the identity — so that **a missing field is a compile error** rather than
an undefined attachment. That is `C.41` (a constructor creates a fully initialized object), `Type.6`
(always initialize a data member) and `ES.20`, and it collapses `ES.3`'s fifteen copies into one. The
per-arm difference is then **what it passes**, which is the only thing that actually differs between them.

**A hard number the next round needs before it plans a target.** `Stage::Subjects` now contributes
`SceneHdr · SceneVelocity · SceneDepth · SceneShadingNormal · SceneSurfaceIdentity` = **5 of `kMaxEdges`
= 8**, and the array needs its `kNoEdge` terminator. `board:1141`'s two term targets bring it to **7 plus
the terminator — exactly full.** So the fragment output set is one target away from a constant that has to
move, and `board:1141` must recount before it starts rather than discover it.

**What is NOT filed here, checked and cleared.** The slot index riding in the surface row
(`kSurfaceFloats` 12 → 13) is a `C.1` deviation **with its reason standing beside it** — the row is the
only per-slot binding, and a second uniform would be a second thing to keep in step — plus the `slot + 1`
that makes the attachment's clear distinguishable from the first slot. Per this repository's own rule a
deviation with its reason next to it is not a defect, and this one's reason is better than the
alternative.

**Done when** no fragment entry point assigns an attachment by hand, an arm that omits an output does not
compile, and adding a sixteenth arm requires nothing to be remembered.

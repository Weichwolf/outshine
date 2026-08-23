Type: bug
Area: render
Regresses: 1738

**A refused crossing leaves no staged state behind, and the ring's two derivations agree**

The 1738 rework opened the ring once and made the happy path sound. Its refusal paths are
not state-clean, and its two capacity derivations contradict each other:

1. **Partial hand on run-count refusal.** src/render/stages/SubjectResidency.cpp:126-136:
   when the `kStagedCrossings` refusal fires mid-loop, the bytes of the WHOLE hand were
   already memcpy'd (:118-122) and the entries before the refusing one were already appended
   to `Staged_` with `StagedCount_` advanced — but `StagingUsed_` was not. The next Cross
   overwrites the same region while those entries still point into it, and FlushCrossings
   uploads the torn mix.

2. **Half-staged pose across a failed SetPose.** src/render/stages/SubjectDraw.cpp:631-639:
   SetPose stages the vertex streams (HandStreams, deferred), then refits the BVH, then
   stages it (HandVisibility). A failure between the two — Refit at :634, or HandVisibility's
   own refusal — returns false with the stream entries STILL staged; nothing drops them
   (`DropStaged` runs only in SetMesh, :439). Renderer::RenderFrame (src/render/Renderer.cpp:691)
   flushes unconditionally next frame: new-pose vertices under the old pose's BVH — shadows
   and visibility of a body that is not the one drawn, silently.

3. **The run budget promises what the byte budget refuses.** SubjectResidency.h:63-65 derives
   `kStagedCrossings = 32` as "three-fold headroom for a second same-frame hand"; but the
   ring's bytes (SubjectDraw.cpp:541-543) are the aligned sum of ONE full hand, so a second
   full same-frame hand refuses at SubjectResidency.cpp:104-108 — with a message blaming
   "the pose outgrew what its own mesh declared", which is false. One population, two
   derivations, two answers.

Demanded: every refusal in Cross and every failure in SetPose ends with the staged state of
the aborted hand dropped (and the refusal text saying what actually happened); the two
constants derive from the SAME population — either the ring holds two hands or the comment
stops promising one. Proof: a device twin in render/outshine/shader that refuses a second
oversized hand and shows the NEXT frame's flush uploads exactly and only the intact hand.

Type: feature
Area: harness
Tags: oracle, instrument

**A case the oracle cannot import has no way to say so**

The standing goal states it plainly: *every case is either within the picture bound or carries a
DECLARED REDUCTION naming why the oracle cannot decide it*. **The second half has no spelling.** A
manifest can declare an acceptance class, a threshold and a note; it cannot declare *the oracle is not
the right side for this case, and here is the measurement that says so*.

## The case that made it necessary, and it is not a guess

[MEASURED] `CubeVisibility` refuses at preparation with Blender's own message:

```
Error: Extension KHR_node_visibility is not available on this addon version
```

**That is the oracle's limit and not ours.** Our reader refuses the same extension, so today the case
is red on both sides for two unrelated reasons and neither number is about the picture. Two models at
the pin require `KHR_node_visibility` -- `CubeVisibility` and `LightVisibility`.

**What the tree does with it now is the defect**: the case simply is not prepared, so it falls out of
the population silently. *A case that vanishes reads as a case that passed, and both published counts
shrink by one without saying why.* That is the same failure this round already found twice -- a
missing thing reported as a pending one.

## What a reduction must carry, or it is an excuse

| | |
|---|---|
| **what the oracle cannot do** | named in the oracle's own words, quoted, not paraphrased |
| **the measurement that shows it** | the refusal, the version it was taken at, and what was tried |
| **what is still decided** | a reduction is per `(case, metric)` and never per case: an oracle that cannot import a visibility extension can still decide the geometry of every node that is visible |
| **what is NOT decided** | stated, so no green rests on it |

## What must be true

- [ ] **A manifest can declare a reduction**, and the schema knows it -- one declaration, both enforcers
- [ ] **A reduced case is PREPARED and SCORED** on what survives, never skipped. Skipping is what makes
  it invisible
- [ ] **The run's trailer counts reductions as their own column**, beside `criteria met` and `within the
  bound`, and neither of those two may absorb it
- [ ] **A reduction names the rung above it that was ruled out** -- *fix the engine · reduce the oracle ·
  patch the asset · disqualify* -- so reducing is never the first answer
- [ ] **`CubeVisibility` and `LightVisibility` are the first two**, and they are cited from the test

## The owner's ruling, and it settles what a reduction is FOR

**What Blender cannot do is built in Outshine anyway, and correctness is judged directly.** The engine's
capability set is not bounded by the oracle's; where the oracle cannot decide, the capability is still
built and the judgement is made by eye and by invariant.

**So a reduction is never a reason not to build.** It is the statement of WHICH INSTRUMENT decides a
metric when the oracle cannot -- and the case is prepared, scored on everything the oracle can still
speak to, and judged by a named instrument on the rest. A case whose capability was skipped because the
oracle could not follow is the failure this item exists to prevent, not an outcome it permits.

**What that costs, stated rather than waved past.** An eye is not a photometer: a judgement of this kind
decides *is the visible set right* and never *is this pixel a code off*. So a reduction names the
question it answers, and no green anywhere else may rest on it.

## Comments

**The ladder's own order says the engine comes first here, and the ruling above agrees.** Neither of
these two models can be decided by the oracle, and our reader does not implement `KHR_node_visibility`
either -- so the sequence is to build the extension and then decide what the oracle can still be held
against. A reduction declared before the engine rung was tried would be the ladder skipped.

## Three oracle limits now, not one, and they are named

[MEASURED] six cases never prepared at all. Two were ours and are fixed (`board:1375`). **The other
four break inside Blender, and three of them are the oracle rather than us:**

| case | what Blender does | kind |
|---|---|---|
| `CubeVisibility` | *Error: Extension KHR_node_visibility is not available on this addon version* | a refusal |
| `LightVisibility` | the same | a refusal |
| `AnimationPointerUVs` | **`KeyError: 'animations'` inside `io_scene_gltf2`** | **a crash** |
| `MeshoptCubeTest` | prepares as far as an identity quaternion no convention derives | ours (`board:1375`) |

**A crash is a worse shape than a refusal and is worth separating.** A refusal names a capability the
oracle does not have; an unhandled `KeyError` names one it thinks it has. Both end this case, and only
the first can be trusted to end it for the reason it states.

**All three are `KHR_`-extension files whose engine side is either built or listed as a task**, so the
reduction this item asks for is what stands between them and a number -- not any work on the engine.

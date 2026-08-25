Type: task
State: active
Parent: 1382
Area: gltf
Tags: khronos

**Gltf 2 0 base is audited against the reader once by enumeration**

**A grep proves a string absent, never a capability.** The reader's own refusals name one base gap, which
says what it REFUSES and nothing about what it silently ignores. This task walks the glTF 2.0
specification's property tables and states, per property, whether the reader reads it, refuses it or
drops it.

- [ ] The enumeration is the specification's, not the corpus's
- [ ] Every DROPPED property becomes its own work item, or the audit produced a document instead of work
- [ ] The audit is a test where it can be -- a property the reader must not drop is better held by a
  case than by a table

---

Sharpened (review round 21, 2026-08-23) -- three concrete DROPPED/LENIENT spots the audit
must carry, found by reading:

- src/gltf/Document.cpp:323 -- `ShapeAllowed` ends in `return true`: any semantic that
  matches no known prefix is accepted with ANY accessor shape. The spec requires
  application-specific attributes to start with `_`; a misspelt standard semantic
  ("NORMALS", "TEXCOORD0") is silently carried and then silently ignored by
  `Find()` -- the mesh loses its normals without a word.
- src/gltf/Types.h:77 with Document.cpp:504-509 -- `Accessor::Min/Max` are parsed and then
  used by NOTHING: not validated against the decoded data, not demanded on POSITION where
  the spec REQUIRES min/max, not used for bounds. Dead fields; the audit decides use-or-drop.
- src/gltf/Document.cpp:874-880 -- the sampler count rule uses `values.Count % wanted != 0`,
  which is right for `weights` (per-target multiplicity) but LENIENT for
  translation/rotation/scale where the spec demands equality; a T/R/S sampler with 2x the
  outputs passes and the surplus is silently unread.

---

Progress -- the three sharpened spots are repaid: ShapeAllowed's fallthrough now enforces
the spec's underscore rule (a misspelt standard semantic refuses instead of silently losing
its meaning), a POSITION accessor without min/max refuses (the bounds the spec requires are
now demanded, which is their first USE; five unit fixtures that violated the spec gained
their bounds), and T/R/S channels demand EXACTLY one output per keyframe (times three for
cubic) -- the weights-only divisibility leniency is gone. All three proven by refusal arms
in AFileThatCannotMeanAnythingIsRefusedByName. The full property-table enumeration remains
this item's body of work.

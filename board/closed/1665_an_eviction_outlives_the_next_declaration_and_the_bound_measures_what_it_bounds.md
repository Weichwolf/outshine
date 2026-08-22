Type: bug
Area: clients
Tags: api, telemetry
Parent: 1485

**An eviction outlives the next declaration, and the parked bound measures what it bounds**

The 1485 slice (fd6c8fb2) is right to bound the parked set and right to name the evicted
room. Three defects in how it did it:

- **The publication channel erases itself in the only flow that matters.** The eviction is
  pushed onto `S_->Carried` (src/clients/Engine.cpp:207), and `Carried` is REASSIGNED to
  `Unacted(scenario)` on every `Declare` (Engine.cpp:124) and every `Read` (Engine.cpp:154).
  A door is Park → Declare/Resume — so in the doorway sequence the trace dies one call
  after it is born, and the client whose interior vanished reads a Carried that never
  mentions it. The proving test only sees the message because its loop happens to END on
  the ninth Park with no subsequent Declare
  (test/render/outshine/client/AClientRunsAScenarioInFourLines.cpp:331-357) — it proves
  the narrow window, not the contract. `Carried` also has a stated meaning — what THIS
  declaration carries that is not yet acted on — and an engine-history event does not
  belong in it. Demanded: a channel the next declaration cannot erase (an `Evicted()`
  accessor, or Park reporting the eviction to the caller), and the test rearranged so a
  Declare follows the evicting Park and the trace still stands.
- **The bound's derivation cites a measurement that measures nothing.** kParkedBound's
  comment (Engine.cpp:13-15) says "state is ~KB per declaration (measured in the
  four-lines proof)"; the proof's Note is `sizeof(outshine::Scenario)`
  (AClientRunsAScenarioInFourLines.cpp:331) — the struct's INLINE size. Scenario is
  vectors of strings, instances, tables, persisted values: the heap payload, which is the
  actual cost of a parked declaration, is invisible to sizeof. A scenario with ten
  thousand instances parks megabytes and the Note still prints the same number. 1485's
  own text demanded the evict-or-refuse decision be made "with a measurement"; this
  number is not one. Demanded: measure a populated declaration's real footprint (walk the
  containers, or measure allocator delta across a Park) and restate the [SET] 8 against
  it — or state the bound as a count with no mass claim attached.
- **Eviction discards the sole copy, and that is a decision to put on record, not in a
  comment.** Bethesda's cell buffer — the cited mechanism — is a CACHE over a save file;
  re-entering an evicted cell reloads it from disk. `Asleep` holds the only copy of the
  parked state, parking is explicitly not a save (1485, and rightly), so the ninth door
  silently destroys what the player left on the floor in room 0, and `Resume("room 0")`
  refuses. Degrade-on-detail/refuse-on-existence says a parked scenario's EXISTENCE
  should not degrade silently. Either Park REFUSES at the bound (the caller decides what
  to drop, loudly), or eviction spills the declaration through the 1493/1492 state door
  once it exists — the choice belongs in 1485's body with its reasoning, and the current
  LRU-drop should not stand as the default merely because it was easy.

---

Closed, and the adjudication FLIPPED on the reviewer's argument: a park is the ONLY copy of
that scenario's state -- no savefile stands beneath it (board:1492 is not built) -- so
Bethesda's cell-buffer eviction, which is a cache over a save, does not transfer. "Degrade on
detail, refuse on existence" decides it: the ninth park REFUSES, naming the least recently
live door to resume or discard. The self-erasing Carried trace is gone WITH the eviction --
nothing vanishes, so nothing needs tracing; the bound's derivation stopped citing
sizeof(Scenario) (which measured no heap and therefore nothing). Proving test: the four-lines
proof stands nine rooms, sees the ninth refuse naming room 0, resumes it, and parks on --
nothing was ever lost. 133/133.

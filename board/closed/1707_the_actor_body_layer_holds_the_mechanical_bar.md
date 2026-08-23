Type: bug
Area: actor

**The actor body layer holds the mechanical bar**

Three residues in src/actor/body, filed together because one sweep repays them:

- **`Bear` returns a verdict nothing may drop** — src/actor/body/Rig.h:58 lacks
  `[[nodiscard]]`. `Reading` carries `PastLimit`/`OffTheSurface`/`Airborne`; a caller that
  ignores it steps a broken rig. The house rule is nodiscard on every value-returning
  query ALWAYS; `Press`, `ShedAt`, `Shed`, `Relaxed`, `EnergyJ` all carry it — Bear is the
  one gap.
- **A runtime clamp where assembly already refuses** — Rig.cpp:31 clamps
  `of.Count` to `kMaxMounts` per tick, but Rigging.cpp:40-44 refuses >kMaxMounts at
  stand-up, so the clamp is dead defensive code on the hot path. Either the refusal is the
  truth and the clamp dies, or `Rig::Count` cannot be trusted and that is the finding.
- **One refusal skips the Refuse helper** — Rigging.cpp:60-65 (the no-driven-contact arm)
  sets `out.Error` and returns without `Refuse()`; it works only because `Stood` defaults
  false. The sibling arms all speak through Refuse; this one drifts the day Rigged's default
  changes.

Also noted, no filing: Bear silently `continue`s on a degenerate normal (Rig.cpp:44) — if a
provider can ever hand one, a counter in `Reading` would say so; today CorridorLay's
normals are unit by construction.

---

Closed -- Bear carries [[nodiscard]] (the compiler is the proof; all three callers already
take the Reading), the per-tick kMaxMounts clamp died because Stand's refusal is the truth
(Count trusted, the fast path takes nothing), and the no-driven-contact arm speaks through
Refuse like its siblings. Proven by the gate: unit/actor + unit/sim green, 21/21.

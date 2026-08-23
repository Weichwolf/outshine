Type: issue
Area: scene
Tags: frame-path, refusal

**A runtime refusal costs no allocation, and Use demands a standing claimant**

The interaction verbs are the tick-path face of the scene graph — Free → Claimed → Occupied
is a loop minds run while the world plays — and every refusal builds a `std::string`:

- src/scene/Store.cpp:405-445 — `Claim`/`Use`/`Release` return through
  `Refuse(std::string why)` (line 463), which constructs and moves a heap string. "Every
  seat of this offer is claimed or occupied — come back or go elsewhere" (line 418) is a
  NORMAL runtime answer, not an exceptional one: a full petrol pump refuses every passing
  mind, every tick, and each refusal allocates. `Link`'s refusals even concatenate
  (lines 190-207). Assembly-time verbs may pay this; the runtime verbs may not.
- Asymmetry: `Claim` (line 407) demands both ends standing, `Use` (line 421-423) only the
  object — a dead claimant's retained handle still flips its old claim to Occupied, because
  the seat reap (line 412) lives only in `Claim`.

Demanded: refusal texts become a `constexpr` catalogue of `string_view`s (the texts are all
static already), `Refuse` stores a view or an index and allocates nothing, and `Use`/
`Release` hold the same standing-claimant bar as `Claim`. The unit mirror proves a full-seat
refusal leaves the error readable without an allocation on the verb (and a dead claimant
cannot Use).

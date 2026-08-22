Type: issue
Area: core
Tags: perf

**Hot value types declare their alignment**

The owner's rule (2026-08-22): `alignas(16)` wherever sensible. Sensible means: value types the
frame path loads as vectors -- NEON moves 128 bits at a time, and a double[3]/float[4] that
straddles a boundary costs the split. NOT a blanket: alignment is declared where a SIMD load
exists or is planned, each with a `static_assert(alignof(...) == 16)` beside it so the claim
is checked, and the population named (which types, why).

First candidates (the audit): FrameContext (Mvp16/eye rows), Physics::Body (position, velocity,
orientation), the instance stream of board:1538 ({float3x4 placement, tint} -- MUST be 16 from
birth), Medium's float4 rows (verify, likely already), DrawList entries. Padding growth is
measured and named per type -- an alignas that doubles a hot array's bytes is a trade to
declare, not a default.

Depends: 1538


---

First slice landed: Physics::Body (each vector row on a 128-bit boundary, 176 B declared
against 136 packed), Physics::Wrench (64 against 48), render FrameContext (192 against 176) --
each with the static_assert beside it naming the padding price. Remaining population: Medium's
float4 rows (verify), DrawList entries, and the 1538 instance stream ({float3x4, tint}) which
must be born 16-aligned.
---

Closed -- the audit is complete and each member adjudicated by the rule's own bar (a SIMD
load exists or is planned, never a blanket):

| type | verdict |
|---|---|
| Physics::Body, Physics::Wrench, FrameContext | DECLARED (first slice), padding named |
| Medium | DECLARED this sitting: alignas(16), zero padding (80 = 5 x 16), the upload is a vector copy; static_assert beside it |
| DrawItem / DrawBatch / IndexRun | REFUSED by the rule -- uint32 records, no vector load exists or is planned; a blanket alignas would double nothing but cache pressure |
| 1538 instance stream | inherited into 1538's body: born alignas(16) |

Proving state: the static_asserts ARE the checks (a regressing alignment refuses at compile),
unit/render 14/14, gate green.

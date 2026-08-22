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
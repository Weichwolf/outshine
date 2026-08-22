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

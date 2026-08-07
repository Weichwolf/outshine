# GPU determinism — what the specifications guarantee, and what an identity may therefore be made of

Two decisions taken on 2026-08-05 collide here, and the collision was asserted away rather than measured:

- **Identity of an entity = its quantised birth position**, and a save is a sparse overlay keyed by
  that. Owner, 2026-08-05: *„Position reicht eigentlich. Reine Physik. Keine zwei Objekte können am
  gleichen Ort sein."* — the file that carried that design was retired with the mission layer; the
  claim is restated here because this file is its refutation and a refutation needs its subject.
- [`visual-target.md`](visual-target.md) §1: terrain and vegetation are **generated on the GPU**, because
  bandwidth is the shortage and upload is traffic too.

The bridging sentence, owner's, explicitly *claimed and unchecked*:

> **„Platzierung ist Ganzzahl, Aussehen ist Fließkomma."**

This file checks it against the two normative specifications. **Verdict: the sentence is half true, and
the false half is the load-bearing one.** §0 states what replaces it.

---

## Spec

### 0. The verdict, and the six contracts

| | |
|---|---|
| **Integer half — HOLDS, stronger than hoped.** | WGSL defines *every* integer edge case: overflow, division by zero, `INT_MIN / -1`, shift ≥ width. There is no implementation-defined and no undefined integer arithmetic in the language. §1 |
| **Float half — HOLDS as a permission, but the permission is far wider than „may reorder".** | WGSL specifies **no rounding mode at all**, permits reassociation, permits fusion, permits flush-to-zero, permits ignoring the sign of zero, and gives `atan` a **4096 ULP** budget. f32 is not reproducible across vendors, and nothing pins it across driver versions of one vendor either. §2 |
| **„Platzierung ist Ganzzahl" — FALSE as stated.** | Placement in the sense meant (*where a thing sits*) is derived from terrain height, noise and density. Those are float, and no quantisation of a float recovers a stable key — the grid needed is larger than the error, and at 10 km from a local origin one f32 ULP is already **0.98 mm**, so the file's own „millimetres suffice" rider is arithmetically false by a factor of ~4. §3 |
| **And order is the larger problem, as suspected.** | WebGPU: compute invocations run *„in any order the device chooses"*; workgroups *„cannot be assumed"* to launch in any order; atomics are **relaxed**; „forward progress" appears **zero times** in either specification. `atomicAdd`'s **returned** value — the slot index an append-buffer allocator uses — is the arrival rank and therefore scheduling-dependent by construction. Arithmetic determinism does not survive a nondeterministic allocator. §4 |
| **The tree has already measured the float half, and the number is the argument.** | `--cloudcheck` compares C++ against WGSL for one formula with identical constants: max \|Δ\| **1.9 × 10⁻⁵**, on **one device, one language pair**. The harness was correctly written with a *tolerance*, because equality would never have passed. 1.9 × 10⁻⁵ is a fine cloud and a destroyed key. State |

**The replacement sentence, and it is a refinement rather than a retraction:**

> **GD1 — Existence and identity are integer. Position and appearance are float.**

| # | Contract | Where |
|---|---|---|
| **GD1** | **Existence and identity are integer; position and appearance are float** | §0, §5 |
| **GD2** | **The identity is the generator's INPUT, not its output** — the *birth address* `(tile, cell, slot)` | §5 |
| **GD3** | **The birth address travels as an integer payload** and is never recomputed from geometry | §5 |
| **GD4** | **A slot is DERIVED, never ALLOCATED** — no `atomicAdd` return value may reach a birth address | §4, §5 |
| **GD5** | **The existence predicate is integer-only over integer inputs** — one float in an accept/reject test makes the *set* of objects vendor-dependent, and nothing downstream recovers that | §5 |
| **GD6** | **Identity is committed on first touch, on the CPU** — the GPU never reads or writes the delta store | §5 |

### 1. Integer arithmetic in WGSL — everything is defined

Every row is normative WGSL text. Nothing in this table is implementation-defined and nothing is
undefined.

| Construct | WGSL says | § |
|---|---|---|
| `i32` | *„the set of 32-bit signed integers … two's complement representation, with the sign bit in the most significant bit position"* | [6.2.3](https://www.w3.org/TR/WGSL/#integer-types) |
| `u32` | *„the set of 32-bit unsigned integers"* | 6.2.3 |
| overflow of `+ - *` | *„Expressions on concrete integer types that overflow produce a result that is modulo 2^bitwidth"* | 6.2.3 |
| `-e` where `e` = most negative | *„the result is e"* | [8.7](https://www.w3.org/TR/WGSL/#arithmetic-expr) |
| signed `e1 / 0` | at runtime: **`e1`**. Shader-creation error if const, pipeline-creation error if override | 8.7 |
| `INT_MIN / -1` | at runtime: **`e1`**. Creation error if const/override | 8.7 |
| unsigned `e1 / 0` | at runtime: **`e1`** | 8.7 |
| signed `e1 % 0` | at runtime: **`0`** | 8.7 |
| `INT_MIN % -1` | at runtime: **`0`** | 8.7 |
| sign of `%` | *„e1 - truncate(e1 ÷ e2) × e2"* — truncated division, remainder takes the **dividend's** sign. Note in spec: *„Use unsigned division when both operands are known to have the same sign"* | 8.7 |
| `<<` / `>>` with amount ≥ width | *„The number of bits to shift is the value of e2, **modulo the bit width of e1**."* Creation error only if the amount is a const/override expression — at runtime it wraps | [8.9](https://www.w3.org/TR/WGSL/#bit-expr) |
| `>>` on `i32` | arithmetic: *„If e1 is negative, each inserted bit is 1"* | 8.9 |
| `>>` on `u32` | logical: *„insert zero bits at the most significant positions"* | 8.9 |
| `<<` losing high bits at runtime | *„discarding the most significant bits"* — the overflow clause is a **creation** error for const/override only | 8.9 |
| `~ & \| ^` | exact bitwise, no latitude | 8.9 |
| `f32 → i32/u32` | **clamp, then truncate toward zero.** *„This clamping requirement is one place where WGSL mandates a meaningful result, but which would yield undefined behavior in C and C++"*. `1e20f → i32` = `2147483520i`. Only NaN gives an indeterminate value | [15.7.6](https://www.w3.org/TR/WGSL/#floating-point-conversion) |
| `atomic<T>` | *„T must be either u32 or i32"* — **there are no float atomics in WGSL.** The classic nondeterministic float accumulator is not expressible | [6.2.8](https://www.w3.org/TR/WGSL/#atomic-types) |
| `bitcast` | *„the reinterpretation of bits in e as a T value"* — exact. **Caveat in §2** | [17.2.1](https://www.w3.org/TR/WGSL/#bitcast-builtin) |

**Conformance backs it, which matters more than the prose.** The WebGPU CTS computes its expected
values for `u32` multiplication with JavaScript's `Math.imul` — i.e. it *asserts* wraparound modulo
2³² — and asserts `x / 0u == x` and `x % 0u == 0` at runtime while requiring a creation error for the
const forms. A conformant implementation cannot deviate.
[`u32_arithmetic.cache.ts`](https://github.com/gpuweb/cts/blob/main/src/webgpu/shader/execution/expression/binary/u32_arithmetic.cache.ts) ·
[`u32_arithmetic.spec.ts`](https://github.com/gpuweb/cts/blob/main/src/webgpu/shader/execution/expression/binary/u32_arithmetic.spec.ts)

**And the word that is absent.** „deterministic" and „reproducible" occur **zero times** in the WGSL
specification, and **once** in the WebGPU specification — about `new ArrayBuffer()` throwing. Neither
document offers determinism as a property. What §1 establishes is not a guarantee of determinism; it is
the absence of any *licence to differ*, which for integers is the same thing and for floats is not.

### 1.1 The hazard is the PORT, not the GPU

WGSL is the strictest of the languages involved. Everything it compiles *down to* is looser, and so is
the C++ the tree already has. The same source text therefore means different things in different places,
and that — not vendor hardware — is where an integer hash actually breaks.

| Operation | WGSL | GLSL 4.60 | SPIR-V | Metal (MSL) | C++17 |
|---|---|---|---|---|---|
| shift ≥ bit width | **masked** (`e2 mod 32`) | **undefined** | **poison** | masked | **UB** |
| unsigned overflow | wraps | wraps | wraps (low N bits) | wraps | wraps |
| **signed** overflow | **wraps** | wraps | wraps | **undefined** | **UB** |
| `int / 0` | `= e1` | unspecified | undefined | unspecified | **UB** |
| `INT_MIN / -1` | `= INT_MIN` | — | — | unspecified | **UB** |

[GLSL 4.60 §5.9](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html) ·
[SPIR-V unified1, `OpShiftLeftLogical`](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html)
(*„The resulting value is poison if Shift is greater than or equal to the bit width"*) ·
[Metal Shading Language Specification §3.1](https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf)
(*„The result of signed integer overflow is undefined"*)

**WGSL's uniformity is manufactured by the translator, and it costs.** Naga emits guard code for signed
division, remainder, `abs` and negation precisely because MSL/HLSL/SPIR-V declare those UB —
[wgpu#6961](https://github.com/gfx-rs/wgpu/issues/6961),
[wgpu#7012](https://github.com/gfx-rs/wgpu/pull/7012) — and the guards were expensive enough that an
opt-out had to be added, [wgpu#9443](https://github.com/gfx-rs/wgpu/pull/9443). The guarantee is real; it
is not free.

**Three rules follow, and they are cheap to hold:**

| # | Rule | Because |
|---|---|---|
| **1** | **Hashes are written in `u32`, never `i32`.** | the signed path is the one every *other* language declares UB, so it is the one the translator must wrap and the one a C++ reference gets wrong |
| **2** | **Never emit a shift by a variable that can reach 32.** | WGSL reads `x >> 32u` as `x`; GLSL, SPIR-V and C++ read it as nothing in particular. A CPU-side golden generator written in C++ has UB at exactly that line |
| **3** | **No `i32` constant is left-shifted in a const expression.** | a shift that flips the sign bit under const folding is a **shader-creation error** in WGSL while the identical HLSL compiles — measured on a FastNoiseLite-style prime, [gpuweb#4166](https://github.com/gpuweb/gpuweb/issues/4166) |

**What the canonical hash papers do NOT say.** Jarzynski & Olano, *Hash Functions for GPU Rendering*,
JCGT 9(3), 2020 — the source of `pcg3d`/`pcg4d` — contains **no statement about cross-vendor
reproducibility**. It measures statistical quality (TestU01 BigCrush) and speed on **one machine, one
GPU** (GTX 1080), and it *assumes* CPU/GPU bit-identity without arguing it: the HLSL sources were
compiled unchanged as C++ so that *„the HLSL code was used for both UE4 and TestU01 for consistency"*.
The assumption happens to be correct under WGSL's rules — but it is inherited, not established.
[jcgt.org/published/0009/03/02/](https://jcgt.org/published/0009/03/02/)

**And the historic „integers are emulated as floats" fear is retired for this tier.** GLSL ES 1.00 —
WebGL 1 — stated it outright: *„It is assumed that integers will be mapped to floating point hardware in
many implementations … any wrapping or clamping behavior cannot be relied upon"*, with a minimum of 10
bits plus sign in the fragment stage
([GLSL ES 1.00 §4.5 and Issues](https://registry.khronos.org/OpenGL/specs/es/2.0/GLSL_ES_Specification_1.00.pdf)).
WGSL mandates exact 32-bit types (§1) and every backend it targets mandates exact low-order-32-bit
unsigned results, so hardware that emulates a 32-bit multiply with narrower ones changes *speed*, not
*value*. **No modern instance of this failure was found.**

### 2. Floating point in WGSL — not reproducible, by specification

| Latitude the spec grants | Verbatim | § |
|---|---|---|
| **no rounding mode** | *„No rounding mode is specified. An implementation may round an intermediate result up or down."* — and „correctly rounded" is defined as *„the smallest value in T greater than x, **or** the largest value in T less than x … WGSL does not specify a rounding mode"*. **So even `x + y` is not bit-determined** | [15.7.2](https://www.w3.org/TR/WGSL/#differences-from-ieee-754), [15.7.4](https://www.w3.org/TR/WGSL/#floating-point-accuracy) |
| **reassociation** | *„An implementation may reassociate operations."* | [15.7.5](https://www.w3.org/TR/WGSL/#reassociation-and-fusion) |
| **fusion** | *„An implementation may fuse operations if the transformed expression is at least as accurate as the original formulation."* | 15.7.5 |
| **`fma` is not fused** | *„the WGSL fma function may expand to an ordinary multiply (including a rounding step) and an add (and another rounding step)"* | 15.7.2 |
| **flush-to-zero** | *„Any inputs or outputs of operations listed in §15.7.4 may be flushed to zero"* — **and explicitly the intermediates of `bitcast`, packing and unpacking.** So `bitcast<u32>(subnormal)` may legitimately be `0` on one device and not on another | 15.7.2 |
| **sign of zero** | *„Implementations may ignore the sign field of a floating point zero value."* | 15.7.2 |
| **overflow/NaN/Inf at runtime** | Finite Math Assumption: *„Implementations may assume that overflow, infinities, and NaNs are not present during shader execution"* → the result is *„an indeterminate value of the target type"*, and *„some functions (e.g. min and max) may not return the expected result"* | 15.7.2, [15.7.3](https://www.w3.org/TR/WGSL/#floating-point-overflow) |
| **near-overflow rounding** | for `MAX(T) < X < 2^(EMAX+1)`, *„either rounding direction is used: X' is MAX(T) or +∞"* | 15.7.3 |
| **`i32`/`u32` → `f32`** | when the integer is not exactly representable, *„**WGSL does not specify whether the higher or lower representable value is chosen, and different instances of such a conversion may choose differently**"* — i.e. above 2²⁴ even a widening conversion is a coin flip, and *within one shader* two occurrences may disagree | [15.7.6](https://www.w3.org/TR/WGSL/#floating-point-conversion) |

**The accuracy budgets, for scale** ([15.7.4.1](https://www.w3.org/TR/WGSL/#floating-point-accuracy)):

| f32 operation | Budget |
|---|---|
| `+ - *`, `abs`, `ceil`, `clamp` | correctly rounded — *which still means either direction* |
| `x / y` | **2.5 ULP** |
| `cos`, `sin` | absolute error ≤ 2⁻¹¹ on [−π, π] — *absolute*, so catastrophic in relative terms near a zero |
| `atan` | **4096 ULP** |
| `atan2` | 4096 ULP, and *„the error in the result is **unbounded**"* near the origin or for subnormal/infinite inputs |
| `acos`, `asin`, `cosh`, `degrees`, `dot`, `length`, `distance` … | *inherited* from a named expression, which is *„only one valid implementation"* — the implementation may choose another |

**And across driver versions of one vendor: the same answer.** Nothing above is scoped to a vendor. The
shader is compiled by the driver at pipeline creation, so a driver update recompiles it and may fuse
differently; the spec grants that permission unconditionally. The only pinning construct in the language
is `@invariant`, and it (a) applies **only** to the `position` built-in of a vertex shader, and (b)
guarantees only *„invariant across different programs and different invocations of the same entry
point"* ([12.10](https://www.w3.org/TR/WGSL/#invariant-attr)) — a same-device statement about two
shaders agreeing, not a cross-device or cross-driver one.

**And it is measured, not merely permitted.** The most common float hash in the whole procedural-graphics
corpus — `fract(sin(x) * 43758.5453)` — **diverges across Apple GPUs under WebGPU today**: iPhone A12 and
A14 fail screenshot validation where an M4 Max passes, and the fix required forcing `MTLMathModeSafe`
plus precise FP functions. The maintainer's summary is the whole of §2 in one line: *„You fundamentally
can't really expect consistent floating point operations across GPUs."*
[wgpu#9561](https://github.com/gfx-rs/wgpu/issues/9561). Note that the A18 Pro is the development target
of [`visual-target.md`](visual-target.md) §1 — this is the family of hardware the tree ships on.

**And there is no fast-math control to turn off.** The request exists and is still open, with a measured
report attached: *„This has caused **severe numerical inconsistencies across platforms** for us,
sometimes producing errors of several thousand percent … 8 orders of magnitude off on Mac OS (Metal
backend)"* — [gpuweb#2076](https://github.com/gpuweb/gpuweb/issues/2076), proposed `precise_math` attribute
[PR#2080](https://github.com/gpuweb/gpuweb/pull/2080), neither landed. Worse, the reassociation clause was
*loosened on purpose*: the „at least as accurate" qualifier that still guards **fusion** was deliberately
removed from **reassociation** ([gpuweb#2402](https://github.com/gpuweb/gpuweb/issues/2402),
[PR#2403](https://github.com/gpuweb/gpuweb/pull/2403)). The direction of travel is away from
reproducibility, not toward it.

**One float idiom is safe, and only because it never leaves the normal range.** The standard
`bitcast<f32>((h >> 9u) | 0x3f800000u) - 1.0` produces a value in [1,2), always normal, so the
flush-to-zero permission cannot reach it. A raw `bitcast<f32>(h)` on an arbitrary hash is **not** safe:
a substantial fraction of bit patterns is subnormal, NaN or infinite, and the spec permits flushing and
indeterminate values for exactly those.

> **Conclusion for §3: there is no float quantity in a WGSL shader whose bits may be relied on.**

### 3. Quantisation does not rescue a float pipeline — the derivation

The position-key design carried a rider:

> *„quantise — position is floating point; regeneration must land on the same value or the lookup misses.
> Round to a grid — **millimetres suffice** — and the key survives any change in arithmetic order"*

**That is arithmetically false, and the numbers are not close.**

f32 has a 23-bit trailing significand, so `ULP(x) = 2^(floor(log2|x|) − 23)`:

| \|x\| | ULP(f32) |
|---|---|
| 100 m | 0.0076 mm |
| 1 000 m | 0.061 mm |
| **10 000 m** | **0.977 mm** |
| 100 000 m | 7.8 mm |
| 6 371 000 m (ECEF) | **500 mm** |

**At 10 km from a local origin, one ULP is already ~1 mm.** A millimetre grid is therefore *finer than
the representation*: the low bit of the key is pure noise. At Earth radius the ULP is half a metre, which
independently kills any ECEF float position.

**And a coarser grid does not fix it, it only makes the failure rarer.** Quantisation of a continuous
quantity is a discontinuous function: a value within ±ε of a cell boundary flips its key. With
accumulated error ε (take 4 ULP as a floor for a short chain — one multiply, one add, one conversion),
the per-object miss probability is `p ≈ ε / grid`. The constant is only good to a factor of two — for an
error uniform on [−ε, ε] over uniformly placed values it is `ε/2grid`, for a two-sided boundary count
`2ε/grid` — which does not matter, because the argument is about orders of magnitude and every row below
is wrong by three or more:

| Local magnitude | Grid | ε = 4 ULP | p | misses per 10⁷ objects |
|---|---|---|---|---|
| 1 km | 1 mm | 0.24 mm | 0.24 | 2.4 × 10⁶ |
| 1 km | 1 cm | 0.24 mm | 0.024 | 2.4 × 10⁵ |
| 1 km | 10 cm | 0.24 mm | 0.0024 | 2.4 × 10⁴ |
| 10 km | 1 mm | 3.9 mm | **1.0** | 10⁷ |
| 10 km | 1 m | 3.9 mm | 0.0039 | 3.9 × 10⁴ |
| 10 km | **10 m** | 3.9 mm | 0.00039 | **3.9 × 10³** |

A 10 m identity grid is already useless for furniture, and it *still* loses about four thousand objects
per ten million. **An identity system needs zero, not few** — one miss is a chair that teleports back, or
two chairs where one was saved. `ε = 4 ULP` is optimistic besides: §2 permits reassociation, fusion and
flush-to-zero, whose divergence is not bounded by a small ULP count at all.

**The one honest statement about quantisation:** it converts a *small, harmless, everywhere* error into a
*rare, catastrophic, unlocatable* one. That is a worse failure mode than the one it replaces, because it
is not reproducible on the machine that reports it.

**Two further defects of the position key, independent of arithmetic:**

| | |
|---|---|
| **it does not span a planet** | `i32` millimetres reach ±2 147 483 647 mm = **±2 147 km**. A global key needs `i64` or a tile-relative split — and a tile-relative split *is* a birth address with extra steps |
| **its claimed advantage is illusory** | *„a chair at a place stays that chair however much better its mesh becomes"* is true of a seed key too — and its own design already conceded the other side: *„this retires the generator-version worry for everything except placement."* Both keys survive a mesh change; **neither survives a placement change.** The position key buys nothing the seed key does not already have |

### 4. Order — the larger problem, confirmed

WebGPU's normative algorithms say it outright:

| Construct | What the spec says | § |
|---|---|---|
| compute invocations | *„For every invocation in computeInvocations, **in any order the device chooses**, including in parallel"* | [WebGPU 23.1](https://www.w3.org/TR/webgpu/#computing-operations) |
| workgroup launch order | *„WebGPU provides no guarantees about: … Whether invocations from one particular workgroup begin executing before the invocations of another workgroup. That is, **you cannot assume that workgroups are launched in a particular order**."* | [WGSL 15.3](https://www.w3.org/TR/WGSL/#compute-shader-workgroups) |
| workgroup completion | *„Some devices may appear to execute in a consistent order, but this behavior **should not be relied on** as it will not perform identically across all devices."* | WebGPU 23.1 |
| **forward progress** | **the term occurs zero times in either specification.** WGSL committee minutes, 2025-11-04: *„those forward progress guarantees are the wild west"* | [gpuweb#4894](https://github.com/gpuweb/gpuweb/issues/4894#issuecomment-3499598709) |
| atomics — memory order | *„All atomic built-in functions use a **relaxed** memory ordering … No synchronization or ordering guarantees apply between atomic and non-atomic memory accesses"* | [WGSL 17.8](https://www.w3.org/TR/WGSL/#atomic-builtin-functions) |
| atomics — modification order | *„atomic modifications are mutually ordered, for each object"* — an order **exists**; the spec never says **which** | [WGSL 6.2.8](https://www.w3.org/TR/WGSL/#atomic-types) |
| `atomicAdd` | *„returns the original value stored in the atomic object before the operation"* — i.e. the arrival rank | [WGSL 17.8.3.1](https://www.w3.org/TR/WGSL/#atomicadd-builtin) |
| barriers | `workgroupBarrier`/`storageBarrier` have **Workgroup** execution scope. They order memory *inside one workgroup*; they do not order workgroups and give no execution order | [WGSL 17.11](https://www.w3.org/TR/WGSL/#sync-builtin-functions) |
| subgroups — size | *„All subgroup sizes are powers of two within the range [4, 128] … The actual size depends on the shader, device properties, and **the device compiler**."* No defined relation between subgroup id and `local_invocation_index` | [WGSL 15.5](https://www.w3.org/TR/WGSL/#subgroups) |
| subgroups — portability | the proposal itself: *„testing indicates that behavior is **not widely portable across devices** … **portability cannot be guaranteed**"*. Measured in Chrome's cross-device matrix: *„The **Nvidia device seemed to give non-deterministic results across multiple runs**."* | [proposals/subgroups.md](https://github.com/gpuweb/gpuweb/blob/main/proposals/subgroups.md), [gpuweb#4306](https://github.com/gpuweb/gpuweb/issues/4306#issuecomment-1795498468) |
| vertex stage side effects | *„any side effects, such as writes into … 'storage' bindings, **may happen in any order**"* — and *„no guarantee that a single vertexIndex will only be processed once"* | [WebGPU 23.2.2](https://www.w3.org/TR/webgpu/#vertex-processing) |
| fragment stage side effects | *„Processing of fragments happens in parallel, while any side effects … **may happen in any order**"* | [WebGPU 23.2.6](https://www.w3.org/TR/webgpu/#fragment-processing) |
| **primitive order — the one guarantee in the whole pipeline** | *„the order of primitives affects later stages, such as **depth/stencil operations and pixel writes**"*. That single sentence is all WebGPU says; Vulkan by contrast spells out *„guaranteed to execute in **rasterization order** … Blending, logic operations, and color writes"* ([Vulkan §34.3](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#primsrast-order)) | [WebGPU 23.2.5](https://www.w3.org/TR/webgpu/#rasterization) |
| indirect dispatch / draw | **deterministic given a deterministic producer** — the parameters are plain `u32` reads, and exceeding a limit is a specified no-op rather than UB | [WebGPU `dispatchWorkgroupsIndirect`](https://www.w3.org/TR/webgpu/#dom-gpucomputepassencoder-dispatchworkgroupsindirect) |
| operand evaluation inside one invocation | **fixed**: *„The order of evaluation for operands of an expression is left-to-right in WGSL."* — the one thing the language pins, and §2's reassociation licence takes most of it back | [WGSL 15.1](https://www.w3.org/TR/WGSL/#program-order) |

**Found while reading, outside this file's subject and owed to
[`visual-target.md`](visual-target.md):** the fixed-function parts of the raster path are *less* specified
than the programmable ones. Polygon coverage on an edge — *„whether or not it's included is **not
defined**"* ([23.2.5.4](https://www.w3.org/TR/webgpu/#polygon-rasterization)); the **multisample resolve
algorithm** is one sentence with no filter and no weighting named ([`end()`](https://www.w3.org/TR/webgpu/#dom-gpurenderpassencoder-end));
alpha-to-coverage is *„**platform-dependent and can vary for different pixels**"* and not even guaranteed
monotone in alpha ([23.2.9](https://www.w3.org/TR/webgpu/#alpha-to-coverage)); per-sample shading *„may
run once per-pixel with the result broadcast"* ([23.2.10](https://www.w3.org/TR/webgpu/#per-sample-shading)).
`visual-target.md` §2 names anti-aliasing on alpha-cutout foliage **the priority investment**, and three
of those four sentences land on exactly it. Carried to Gaps; not resolved here.

**The exact shape of the atomic problem, and it is narrow enough to design around:**

| | Deterministic? |
|---|---|
| the **final value** of an atomic `u32` counter after N increments | **yes** — integer addition is associative and commutative modulo 2³², and §1 pins the wraparound |
| the **returned** value of any individual `atomicAdd` | **no** — it is the arrival rank, and arrival order is *„any order the device chooses"* |
| therefore: an append buffer's **contents as a set** | yes |
| an append buffer's **contents as a sequence** | no |

That is exactly why **GD4** exists. A stream-compaction allocator is the canonical GPU placement idiom
and it is the single construct that would destroy identity while every arithmetic operation around it
stayed bit-exact.

**The standard repair is a sort, and its price is measured.** The industry recipe — write `(key,
payload)`, then *„sort those records into a stable order and then reduce each group"* — is what NVIDIA
Warp does to make `atomic_add` reproducible, because *„CUDA is **free to apply those updates in different
orders**. Floating-point addition is not associative."*
([Warp: Deterministic Execution](https://nvidia.github.io/warp/latest/user_guide/execution_and_performance/deterministic_execution.html)).
Measured on an RTX 4090:

| Case | Sort-then-reduce vs. atomics |
|---|---|
| high contention (1 output, 65 K / 262 K / 1 M writes) | **0.76× / 0.26× / 0.13×** — *faster*, because atomic serialisation at one hot address disappears |
| low contention (65 K outputs) | **13.6× / 23.9× / 10.0×** — *slower* |
| deterministic slot allocation (count pass + scan + prefix) | **~6–7.6×** |

Procedural placement is the low-contention case, so the repair would cost about an order of magnitude.
**GD2/GD4 avoid it entirely**: if the slot is *derived* from the cell rather than allocated, the buffer
order may be whatever the device likes, because order no longer carries information. Ordering the buffer
becomes a *rendering* concern, and rendering is the float half.

**And the general principle, stated by the people who sell the hardware:** *„exactly-associative
operators (integral addition) automatically produce reproducible results **regardless of scheduling**"*
([NVIDIA CCCL, determinism](https://raw.githubusercontent.com/NVIDIA/cccl/main/docs/cccl/determinism.rst)).
CUB grades its own algorithms in three tiers — `not_guaranteed` (atomics), `run_to_run` (a fixed
hierarchical tree instead of atomics), `gpu_to_gpu` (reproducible summation, **+20–30 %**)
([CCCL blog](https://developer.nvidia.com/blog/controlling-floating-point-determinism-in-nvidia-cccl/)).
Even cuBLAS's bitwise guarantee holds only *„on GPUs with the same architecture and the same number of
SMs"* ([cuBLAS](https://docs.nvidia.com/cuda/cublas/index.html)). **GD1 is the free tier of exactly that
ladder** — and the only one available to a page that ships to unknown hardware.

### 4.1 What the practitioners do — and the shape of their answer

| Source | Finding |
|---|---|
| **Factorio, FFF-370** — cross-platform lockstep between x86 PC and ARM Switch | They did **not** make floating point portable; they hunted the specific UB. *„when casting a double to an integer, if the value does not fit in the integer, it is considered undefined behaviour and the resulted value is different on ARM and x86 CPUs."* Verification was a **state CRC per tick, per architecture, over 2 417 tests** ([FFF-370](https://www.factorio.com/blog/post/fff-370)). A separate desync was caused *purely by a differing CPU core count* — *„deterministic multithreading even more so"* ([FFF-415](https://www.factorio.com/blog/post/fff-415)) |
| **Gaffer on Games, *Floating Point Determinism*** | Jon Watte, quoted: *„As long as you stick to a **single compiler, and a single CPU instruction set**, it is possible to make floating point fully deterministic."* Recommended practice: pin the compiler, pin the FPU control word and re-assert it every tick, avoid SSE and transcendentals, use `/fp:strict`, accept the performance loss ([gafferongames.com](https://gafferongames.com/post/floating_point_determinism/)) |
| **StarCraft: Brood War** | *„The math is **fixed-point** so two CPUs never disagree on a rounding bit"*, and a replay is a seed plus the command stream — no positions, no state ([Inside Brood War](https://marianogappa.github.io/inside-brood-war/index.html)) |
| **No shipped lockstep simulation runs on the GPU** | searched and **not found** — in Age of Empires, Factorio, Brood War and both Gaffer articles the GPU is not part of the simulation or of the recorded state; Gaffer does not mention it once. Absence of evidence, stated as such |
| **The one GPU physics engine that takes a position** | NVIDIA Flex: *„**Flex is not deterministic.** Although simulations with the same initial conditions are often reasonably consistent, they may diverge over time, and **may differ between different GPU architectures and versions**."* ([Flex manual](https://nvidiagameworks.github.io/FleX/1.2/lib_docs/manual.html)). PhysX's GPU rigid-body documentation makes **no determinism statement at all**; its CPU documentation promises reproducibility only for *„the same PhysX release running on the same platform"* with the same actor insertion order ([PhysX 5.5](https://nvidia-omniverse.github.io/PhysX/physx/5.5.0/docs/RigidBodyDynamics.html)) |
| **CPU physics, for contrast** | Box2D: *„For the same input, and **same binary**, Box2D will reproduce any simulation"*, *„deterministic under multithreading"*, and *„cross-platform determinism as of version 3.1"* ([Box2D FAQ](https://box2d.org/documentation/md_faq.html)). Rapier's `enhanced-determinism` buys cross-platform results and is **mutually exclusive with its `parallel` and SIMD features** ([rapier.rs](https://rapier.rs/docs/user_guides/rust/determinism/)) |

**Two things follow, and the second is the important one.**

1. **The condition under which float determinism is attainable is exactly the condition a GPU cannot
   meet.** „Single compiler, single instruction set" is a statement about a pinned toolchain. A WebGPU
   shader is compiled by the *user's driver* at pipeline creation, on hardware chosen by the user, and
   the tree ships to Edge on an Xbox and to a phone ([`visual-target.md`](visual-target.md) §1). There is
   no `/fp:strict` and no FPU control word to re-assert.
2. **Factorio's answer is precisely GD5's shape**: don't make the arithmetic portable, make the
   *decisions* portable, and verify by hashing the state on both architectures. §6's anchor is that test,
   with backends where Factorio had architectures.

3. **It ratifies the split the tree used to have.** The headless client was GPU-free by construction
   (`nm` = 0 Dawn/WebGPU symbols was a build gate) and is deleted; that is precisely the property that
   makes the mission regression and the `--threads 1/2/4` determinism gate mean anything. **Nothing in
   §1–§4 may be read as permission to move a physics step, a sensor evaluation or a pilot decision onto
   WebGPU.** The prior art is unanimous and the specification offers nothing to appeal to. This file is
   about *world generation*, which is upstream of the simulation and hands it a fixed artefact.

**One pleasant inversion worth recording:** the exact bug Factorio had to hunt — out-of-range
`double → int` — is the one case WGSL *mandates* a value for (*„clamp … then round toward zero"*, §1,
[15.7.6](https://www.w3.org/TR/WGSL/#floating-point-conversion)). WGSL is a stricter language than C++
here, because for a web-facing language undefined behaviour is a security problem rather than an
optimisation opportunity. The integer half of §1 is a gift, and this file's whole design is to move as
much as possible onto it.

### 5. What identity is made of

**GD2 — the birth address.** The identity is the generator's *input*:

```
birth address = (tile_id : u32, cell_index : u16, slot : u16)      // 64 bits
```

| Property | Why it follows |
|---|---|
| integer by construction | it is never computed, it is *enumerated*. No arithmetic, no rounding, no conversion |
| available before any float runs | it is the loop variable of the placement dispatch, not its result |
| globally unique without an allocator | the cell decomposition is fixed and the slot is a fixed per-cell capacity index, so no counter is involved. **The `u32` reaches zoom 15 and no further** — 2¹⁵ × 2¹⁵ = 1.07 × 10⁹ tiles fits, zoom 16 at 4.29 × 10⁹ does not. The widths above are an illustration whose exact split follows `fb-tiles`' own scheme, not a decision taken here |
| smaller than the position key | 64 bits vs. 96, and unlike the position key it actually covers a planet (§3) |
| sorts by locality | which is what a sparse overlay wants anyway |

**GD3 — it travels, it is never recomputed.** The placement shader writes the birth address into the
instance record as an integer payload beside the float transform. Downstream — culling, LOD, impostor
selection, the depth prepass — may reorder, cull and duplicate freely; the payload is copied, not
derived. **A driver difference then costs a sub-millimetre difference in where a tree stands, and nothing
else.** This is the move that makes §2's verdict survivable: float divergence is demoted from an identity
failure to an invisible one.

**GD4 — a slot is derived, never allocated.** No value returned by `atomicAdd` may reach a birth address
(§4). Per-cell fixed capacity with an integer occupancy test; compaction may renumber the *buffer*, never
the *address*.

**GD5 — the existence predicate is integer-only.** This is the sharp constraint, and it is where the
real work is. If a float decides *whether* an object exists, the divergence is in the **set**, and no
payload trick recovers it: a tree that exists on one device and not another has no identity to preserve.
So every input to an accept/reject test is integer or fixed-point:

| Input | Integer form |
|---|---|
| hash / jitter | `u32` PCG or Wang — §1 makes these exact, §1.1 rule 1 makes them `u32`-only |
| density threshold | `u32` compare against the hash, not a `f32` compare against a noise value in [0,1] |
| landcover / vegetation template | the 8-bit albedo index of [`vegetation.md`](vegetation.md) — already integer |
| terrain height (tree line, water line) | fixed-point over the DEM's integer samples, with a power-of-two cell so the bilinear weights are dyadic and the interpolation is an exact integer expression. **The width is not chosen and it is not free** — derivation: the four weights sum to `N²`, so the accumulator peaks at `h_max · N²`. With `h_max` = 9 000 m in centimetres = 9 × 10⁵, `N = 2⁶` gives 3.69 × 10⁹ and fits `u32` (4.29 × 10⁹) with 14 % to spare; `N = 2⁸` gives 5.9 × 10¹⁰ and does not. **So the interpolation weight has at most 6 fractional bits in `u32`**, or the accumulation must be staged. Named as work, not waved through |
| slope | fixed-point cross-difference of the same integer samples, compared against an integer threshold |

`f32` may then compute the *position within the cell*, the height offset for the visual, the wind phase,
the LOD blend — everything downstream of existence.

**GD6 — identity is committed on the CPU, at first touch.** The save model it serves is *„a delta store
for **touched** things only, never for generated ones. A house nobody touched costs zero bytes."* The count of touched things is hundreds to thousands, not millions, so the whole identity
system lives on the CPU where determinism is total and free. The GPU's only obligation is to hand back
the birth address of what was hit — integer data it carried, not data it computed. **The GPU never reads
and never writes the delta store**, which is also the right shape for the tree's other rule: the delta
store is world state, and world state has one writer.

### 5.1 The four candidates, judged

The owner named four alternatives. **They are not four alternatives.** One is adopted, two are parts of
it, one is rejected — and the fifth row is the option the design started from.

| Candidate | Verdict | Why |
|---|---|---|
| **Placement on the CPU** | **REJECTED** | it buys a determinism GD2 already has, and pays with the one *measured* shortage — 60 GB/s, [`visual-target.md`](visual-target.md) §1, where the whole „generate, don't upload" decision comes from. And it does not even solve the real problem: GD5's predicate would then exist twice, once on each side, and two implementations of one truth is the failure mode this tree forbids everywhere else |
| **Identity from the generation seed** | **ADOPTED — this is GD2/GD3** | integer by construction because it is *enumerated, not computed*; upstream of every approximation; 64 bits; planet-wide; sorts by locality. Nothing about it can be corrupted by anything downstream, which is the entire property that was wanted |
| **Fixed-point `i32` with hand-written operations** | **ADOPTED as a COMPONENT of GD2, not as an alternative** | and narrower than proposed: §1 shows WGSL already defines `+ * >> /` `%` exhaustively and the CTS asserts it, so hand-writing *arithmetic* would re-derive a normative guarantee. What must be written in fixed point is only what would **otherwise be float** — GD5's DEM interpolation, slope and density tests. That is a handful of functions, not an arithmetic layer |
| **Identity at first touch** | **ADOPTED as a COMPLEMENT of GD2 — this is GD6** | it cannot stand alone: a touch must *name* what it touched, and the name is a birth address. What it buys is scale — identities number in the thousands, not the millions, so the whole identity system lives on the CPU where determinism is total and free |
| *(the starting point)* **quantised birth position** | **REJECTED** | §3, with the derivation. Below the f32 ULP at 10 km, ±2 147 km range in `i32` millimetres, 96 bits, and its advertised advantage over a seed key does not exist |

**The one-sentence recommendation:** *keep placement on the GPU, and make the identity the thing that was
put in rather than the thing that came out.*

### 6. Acceptance

| Contract | Anchor |
|---|---|
| **GD5 is machine-checked** | the existence predicate's WGSL contains no `f32`/`f16` in its data flow — a gate of the same shape as `sim/tools/verify_layers.py`, over the shader source rather than includes |
| **GD4 is machine-checked** | no value returned by `atomicAdd`/`atomicSub`/`atomicExchange` reaches an instance record's identity field — same gate |
| **GD1/GD2 are measured across backends** | the same world region generated on two backends (Metal on the A18 Pro, and the browser's on the delivery target) yields the **identical set of birth addresses**, compared by hash — Factorio's per-tick state CRC across x86 and ARM (§4.1), with backends in place of architectures. This is the only test that can falsify the design, and it needs two devices — a single-device run proves nothing about portability and would be theatre |
| **the save survives the crossing** | a delta store written on backend A loads on backend B with **zero** key misses. Not few |
| **the float half is allowed to differ, and the difference is bounded** | the same comparison over positions must differ by less than the smallest visible amount at the nearest LOD — a number, not a hope. It has never been taken |

---

## State

**Nothing in §5 is built** — no placement compute shader, no instance record, no birth address, no delta
store, no two-backend harness. [`vegetation.md`](vegetation.md) State: *„Nothing of the template system is built."*

**But GD1 is already practised in the tree, one scale down, and it was arrived at independently.** The
cloud density field is *one formula, two evaluators*: `core/CloudDensity.h` in C++ and
`render/stages/CloudDensityWGSL.h` as a transliteration whose constants are emitted from the C++ ones
([`clouds.md`](clouds.md)). Its author reasoned exactly as §1.1 does, in the source:

> *„Integer hash. **uint32 wraps identically in C++ and WGSL**, which is the whole reason the noise is
> hashed rather than sampled from a texture."* · *„Deliberately hand-written instead of `std::`
> equivalents: `smoothstep` and integer hashing **must be bit-comparable against WGSL, and the built-ins
> are not specified to the same rule**."*

Both sentences are confirmed by §1 and §2 respectively. `CloudHash2`/`CloudHash3` are `u32`-only
murmur-style finalizers, which is §1.1 rule 1 held before it was written down.

**And the measured agreement is the empirical core of this file:**

| Measurement | Result | Source |
|---|---|---|
| C++ ↔ WGSL, cloud **density** | max \|Δ\| **1.90 × 10⁻⁵**, mean 4.0 × 10⁻⁷ over 12 288 samples; tolerance 10⁻⁴ → AGREE | [`clouds.md`](clouds.md) State |
| C++ ↔ WGSL, shared **air** | max \|Δ\| **1.19 × 10⁻⁷**, mean 1.17 × 10⁻⁸ over 12 288 samples → AGREE | [`clouds.md`](clouds.md) State |

**Read those two numbers as this file's evidence, not as a success report.** They are *not zero*. Two
transliterations of one formula, with identical constants, on one machine, in one language pair, land
10⁻⁵ apart — and the harness that measures them was correctly built with a **tolerance** rather than an
equality, because equality would never have passed. That is §2 measured at home: 1.9 × 10⁻⁵ is a fine
cloud and a destroyed key. It is also a **lower** bound, since the comparison crosses two languages on
one device and never two vendors.

What else exists and is relevant:

| Piece | State |
|---|---|
| the two specifications' guarantees, as tabulated in §1–§4 | **read and quoted**; this file is that reading |
| `--cloudcheck` — a C++ vs. WGSL differential harness | **built**, and it is the shape §6 needs. What it lacks is a second *device* |
| determinism discipline on the CPU side | **built, and currently without a subject** — the `--threads 1/2/4` fingerprint gate stands; there is no scenario left to run it on |
| `gpu_walk` as the frame oracle | **built**, and it is the natural first of the two backends §6 needs |
| the 8-bit albedo → vegetation template index | **specified only** ([`vegetation.md`](vegetation.md)), and it is the one existence input that is already integer by design |

## Gaps

| Gap | Detail |
|---|---|
| **Rejected, with its measurement: „millimetres suffice" as a quantisation grid** | §3 shows it is wrong by a factor of ~4 at 10 km and by ~500× at ECEF magnitudes. ULP(f32) at 10 km is 0.98 mm. The design that asserted it is retired; the refutation is kept because it is what stops the next round re-deriving the same key |
| **The existence predicate has no design** | GD5 names its five inputs and their integer forms. Fixed-point DEM interpolation and the slope test are unwritten, and the tree-line/water-line comparison is the case most likely to need care |
| **One float claim in `CloudDensity.h` is stronger than §2 permits** | the source says *„cos/sin of that angle are exact in binary floating point, so the two evaluators stay bit-comparable"*. The **constants** are exact; the **operations** consuming them are not, and §2 grants reassociation, fusion and flush-to-zero regardless. The measured 1.9 × 10⁻⁵ is the proof that „bit-comparable" was never achieved. Harmless for a picture — the sentence should say *„comparable to 10⁻⁴"*, which is what the harness actually asserts |
| **The two-backend harness does not exist** | §6's only falsifying test cannot be run today. Until it can, GD1–GD6 are a *reasoned* design, not a *measured* one, and this file says so rather than implying otherwise |
| **A C++ reference of the existence predicate is UB by default** | §1.1: signed overflow, wide shifts and division by zero are undefined in C++17 and defined in WGSL. If any client ever evaluates the predicate on the CPU — for a cut, a spawn or a test oracle — it must be written in `uint32_t` with explicit masks, or it will disagree with the shader on exactly the inputs nobody tests. No such reference exists yet, which is the moment to state the rule rather than to discover it |
| **Conformance is necessary, not sufficient** | the CTS tests operations one at a time. Nothing in it compares a whole shader's output between two vendors, so a passing implementation is still free to differ wherever §2 grants latitude |
| **Subgroup operations are excluded, and that costs performance** | §4: the proposal itself says *„portability cannot be guaranteed"*, and Chrome's own cross-device matrix recorded an NVIDIA device giving **non-deterministic results across multiple runs**. The fastest reduction and compaction idioms are therefore unavailable to the existence predicate. Unmeasured cost |
| **Owed to `visual-target.md`: the raster path is less specified than the shaders** | §4's note. Edge coverage undefined, multisample **resolve** algorithm unspecified, alpha-to-coverage *„platform-dependent and can vary for different pixels"* and not monotone in alpha, per-sample shading collapsible to per-pixel. `visual-target.md` §2 makes anti-aliasing on alpha-cutout foliage **the priority investment**; three of those four land on it. **Not this file's decision** — it belongs in the Spec of [`stages/tonemap.md`](stages/tonemap.md), the pass it lands in, and this row exists so the finding is not lost |
| **Found and not filed: a contradiction inside the WebGPU CRD** | [23.2.5](https://www.w3.org/TR/webgpu/#rasterization) says *„Implementations must use the standard sample pattern"* and lists the 1× and 4× positions; [23.2.5.4](https://www.w3.org/TR/webgpu/#polygon-rasterization) says the locations *„are implementation-defined"*. No gpuweb issue was found for it. Reported here because a design that leans on the first sentence would be leaning on the wrong one |
| **The mod-authored object is untouched** | a chair a scenario *declares* has no birth address, because no generator placed it. Its identity is presumably its declaration site in the mod file — stated as an open question, not a decision. [`../mods.md`](../mods.md) owns it |
| **Rejected: placement on the CPU** | it buys a determinism the birth address already has, and pays with the one measured shortage — 60 GB/s, [`visual-target.md`](visual-target.md) §1. Worse, it does not solve GD5: a CPU predicate and a GPU renderer must then agree about what exists, which is two truths about one world |
| **Rejected: hand-written fixed-point instead of trusting the language** | §1 shows WGSL already defines every integer edge case and the CTS asserts them. Hand-written wrap/shift helpers would add code to re-derive a guarantee that is already normative. Fixed-point is still needed — but only for the quantities that would *otherwise be float* (GD5's DEM and slope), not as a replacement for `+`, `*` or `>>` |
| **Rejected: the quantised birth position as the key** | §3, with its derivation. Kept here with the numbers so nobody re-derives it as a simplification |
| **Rejected: `@invariant` as a portability tool** | §2. It covers only the `position` built-in and only within one implementation |

## Knowledge

- **Why the integer half is stronger than „no undefined behaviour".** WGSL does not merely decline to make
  integer edge cases undefined — it *names a value* for each of them (`e1` for division by zero,
  `0` for remainder by zero, `e` for negating `INT_MIN`, `e2 mod width` for a wide shift). That is a
  deliberate design choice for a web-facing language, where undefined behaviour is a security problem
  rather than an optimisation opportunity, and it is why the integer half is safe to build on.
  [WGSL §8.7, §8.9](https://www.w3.org/TR/WGSL/#arithmetic-expr)
- **ULP(f32) at magnitude x is `2^(floor(log2|x|) − 23)`.** At 10 km that is 0.977 mm; at Earth radius,
  0.5 m. Any „quantise to a fine grid" argument must be checked against this table first (§3), and the
  check is one line of arithmetic.
- **Why quantisation cannot fix a nondeterministic input.** Rounding is discontinuous. A function that is
  discontinuous on its input's error interval maps a bounded error to an unbounded one with probability
  `ε/grid`. Making the grid coarse reduces the probability and never reaches zero; making it fine makes
  it certain. There is no grid size at which a float becomes an integer.
- **Why the position key and the seed key fail identically under a placement change, and why that settles
  it.** Both keys are functions of where the generator puts things. Change the placement rule and both
  move. The position-key design already conceded this (*„except placement"*), so its advertised
  advantage over a seed key does not exist —
  while its disadvantages (float derivation, ±2 147 km range, 96 bits) are real.
- **Why identity must be the generator's input rather than its output.** An output is downstream of every
  approximation in the pipeline; an input is upstream of all of them. That is the whole argument in one
  sentence, and it generalises beyond graphics: it is the same reason
  the run judge defines the verdict in terms of the state vector
  rather than storing it separately.
- **Why an atomic counter's final value is deterministic but its returned values are not.** The final
  value is a sum, and integer addition modulo 2³² is associative and commutative, so order does not reach
  it. Each returned value is an *arrival rank*, which is nothing but the order, so order is all it is.
  The distinction is what lets §5 keep GPU compaction and still have stable identity.
- **The reproducibility ladder, with its measured prices.** Integer/exactly-associative accumulation:
  **free**, reproducible regardless of scheduling ([NVIDIA CCCL](https://raw.githubusercontent.com/NVIDIA/cccl/main/docs/cccl/determinism.rst)).
  A fixed reduction tree instead of atomics: run-to-run only, on one device. Reproducible summation
  (Demmel & Nguyen RFA, [10.1109/TC.2014.2345391](https://doi.org/10.1109/TC.2014.2345391); ReproBLAS;
  ExBLAS): **+20–30 %** in CUB's `gpu_to_gpu`, ~4× single-core in ReproBLAS. Sort-then-reduce:
  **10–24×** at low contention. Kahan summation buys **accuracy, not order-independence** — parallelising
  it splits the accumulator and reintroduces the dependence. GD1 takes the top rung, which is the only one
  whose price a 720p30 budget can pay and the only one that survives unknown hardware.
- **Why no test on one device can settle this question.** Portability is a statement about the set of
  conformant implementations. A run on one implementation is one sample and cannot falsify a universal
  claim, which is why §6's anchor requires two backends and why this file contains no single-device
  measurement dressed up as evidence.

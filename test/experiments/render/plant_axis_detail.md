# Axis detail before meshing — #1

The whole-tree comparison in [native_broadleaf_crowns.md](native_broadleaf_crowns.md)
spends108864extra triangles on1008short terminal axes. A single8×6setting ignores
radius and bend. Choose subdivisions from an explicit local error before creating
vertices. This experiment does not prune axes, move leaves or change recipes.

SIGGRAPH starting point: Cohen et al., *Simplification Envelopes*(1996), local
[paper](../../../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf),
p.1 and Figure1 opened. Keep a stated bidirectional distance constraint; samples
alone are not a Hausdorff proof. The following bound is our direct derivation for
the existing circular/tapered axis model, not that paper's simplification algorithm.

## Parametric model and bound

In local metres, normalized axial parameter t and angle φ define

    P(t,φ) = C(t) + r(t) [cos(φ) e_x + sin(φ) N(t)]
    r(t) = r0 + (r1-r0)t

C is the existing constant-bend circular centreline of length L and angle θ;
N rotates with angular rate |θ|. There is no axial twist. Let R=max(r0,r1) and
D=|r1-r0|. Triangle inequalities give bounds over the whole parameter domain:

    ||P_tt|| ≤ L|θ| + 2D|θ| + Rθ² = Mtt
    ||P_tφ|| ≤ D + R|θ|             = Mtφ
    ||P_φφ|| ≤ R                   = Mφφ

For any barycentric point q in either parameter-space grid triangle, compare
P(q) with the linear interpolation of its three exact corner values. The first
Taylor terms cancel because Σλ(q_i-q)=0. Weighted coordinate variance is at most
one quarter of its squared range: Var(t)≤Δt²/4 and Var(φ)≤Δφ²/4. This follows
from E[(x-a)(b-x)]≥0 and (mean-a)(b-mean)≤(b-a)²/4. Cauchy-Schwarz bounds the
weighted absolute cross product by ΔtΔφ/4. The integral Taylor remainder gives

    E ≤ Mtt Δt²/8 + Mtφ ΔtΔφ/4 + R Δφ²/8
      = A/m² + B/(mn) + C/n²
    A = Mtt/8, B = π Mtφ/2, C = π²R/2

Here m is axial segments and n circumference sides. The parameter triangles
cover the continuous side and their mesh counterparts; the same correspondence
bounds both distance directions. Each planar end disk's polygon sagitta is
R(1-cos(π/n))≤Rπ²/(2n²), already covered by the last term. Thus the bound covers
the ideal closed surface, not only its centreline or samples.

## Bounded choice and scope

The source producer supports2..1024segments and3..128sides. For each side count,
error decreases monotonically with segments; bisection finds the least feasible
integer. Compare2n(m+1)triangles, including caps. Equal triangle count prefers
smaller n, since vertices=triangles/2+2n+2. Once6n cannot beat the current result,
larger side counts cannot win. This is the minimum within this finite uniform
tessellation family and stated bound, not globally optimal geometry or LOD.

The C++ model rounds nonnegative coefficient operations/evaluation upward with
nextafter under the build's normal IEEE64 arithmetic. Zero terms stay zero;
positive underflow is rounded upward. Inputs/counts are validated, and an
unreachable demand fails. Selection/preflight allocate no geometry.

**The bound is local to the ideal parametric tube.** Frame transformation,
Float32 vertex emission, shader arithmetic, normal interpolation, coverage,
lighting, wind and screen projection require their own accounting. This does
not reduce any existing world/terrain LOD allowance or establish a runtime
screen-space bound. No source/distance term is silently replaced by this number.

## Independent experiment

`plant_axis_detail.py --output <system-temp> [--recipe <canonical-json>]` compares
100straight/tapered/curved/signed-bend profiles against the entire integer grid.
All selected tuples agree.409600independent continuous-surface samples exercise
the real triangular interpolation; their largest error/bound ratio is0.967531.
Those samples check implementation examples; the derivation above supplies the
model bound. The recipe path requests axes only, never builds leaf pose tables.

For the actual1105-axis terminal18/55 studies,5mm local parametric demand selects
27022wood triangles instead of119340: (119340−27022)/119340≈77.36%less wood.
The10mm experiment selects21632. Both retain every source axis and leaf.
These are geometry/operation counts, not a frame-time or image-quality result.
Original receipts, axes-only Python timings and the opened paper page remain
privately under `/tmp/outshine-native-plant-recipe-39bf9e07d/axis-detail/`.

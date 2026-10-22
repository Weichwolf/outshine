# Constant coordinates at the shader interface

Owner: [#16](https://github.com/Weichwolf/outshine/issues/16).

## Change and boundary

An existing lit vertex variant with no UV sets always writes zero to both UV
sets, facade bay and facade classification. Its existing fragment variant has
`LIT_TEXTURED=0`. Replace those four interface fields with stage-local zero
values on both sides. Keep original locations for every remaining field.
There are six fewer Float32 components in these variants; this is a logical
interface count, not a measured memory-transfer reduction.

The original shader permutations, pipeline count, vertex streams, resource
bindings, uniform layouts, material values and geometry stay unchanged.
UV-bearing lit and all flat/ground shaders keep their existing interface.
The ground domain requires a UV-bearing lit layout through `DomainPresents`;
it cannot use the zero-UV variant. Current/previous clip fields remain Float32
at locations 7/8. This is not a diagnosis or fix for #900.

## Metal evidence

Native C++ executable: `e2ae80985`. Private source copies and subsequently the
proposed product shaders run through the complete original renderer. CPU code,
asset input, scope, resolution and exact 60-frame/360-degree windows stay fixed.
No extra world frames, omitted spikes or sampling-window shifts.

All 522 product shader artifacts compile. The emitted Metal vertex outputs and
fragment inputs both omit locations 0/1/11/12 only in the intended variants.
Eleven GeometryRenderer cases freshly compile/link with the actual full native
flags and matching production objects; all pass under Metal API validation.
An old diagnostic executable fails identically with original/proposed shaders;
it is not counted as a current test. The freshly linked current test passes.

Four initial API-validated raw/proposed Place pairs complete with identical final
PNGs, followed by normal A/B/B/A windows. Reverse-order repetitions cover Tokyo
and Central Park. Every window retains all 60 GPU samples and complete worlds.
All final PNGs are byte-identical; no unrefined frames.

The final full-product shader build also completes all twelve current Places,
each with exactly 60 GPU samples and no unrefined frames. Eleven PNGs are
byte-identical. Basel differs at 16 of 1280 × 720 pixels, with maximum channel
delta 1/255; those small colour differences remain explicit, not a bit-exact claim.
The viewed result retains the same scene. No reference pin is overwritten.
Exactly the intended 88 shader artifacts differ from the previous build; all
other artifacts match byte-for-byte, as do private and proposed changed artifacts.

One separately profiled pair per dense scene matches each measured submission ID
and elapsed time to its original counter trace using #905. These mean intervals
overlap with other GPU work and cannot be added:

| Interval, ms | Tokyo original | Tokyo proposed | Central Park original | Central Park proposed |
|---|---:|---:|---:|---:|
| Object vertex | 6.2347 | 5.5008 | 4.5472 | 3.8700 |
| Object fragment | 3.4809 | 3.4221 | 2.7948 | 2.7161 |
| Whole submission | 14.0276 | 13.2382 | 14.1149 | 13.2150 |

Normal Tokyo p50 improves in both orders. Central Park has mixed complete-frame
timings; neither scene establishes a general p99 gain or the 16.67 ms budget.
First-use peaks remain included. Profiling/allocation overhead is not isolated.
This is a smaller interface with a local Metal vertex-interval improvement,
not a uniform frame-speedup or a world-movement fix.

Raw compiler commands, shader hashes, initial failed setup, independent test
rebuilds, original/proposed images and per-frame counter receipts are linked in
#16. Private evidence is located through
`/tmp/outshine-motion-integration-844/constant-uv-task.txt`.
Vulkan/Iris Xe counterchecks remain a separate integration requirement.

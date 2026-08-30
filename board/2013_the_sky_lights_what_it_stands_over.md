Type: bug
State: active
Progress: gpu-driven
Area: render
Tags: measured, benchmark

# The sky's irradiance is computed TWICE, and the subject's half is flat

**Benchmark** — Unreal: ONE `SkyLight` captures the sky and every consumer reads that capture --
the surfaces, the exposure, the reflections. RAGE: one ambient probe, read by everything that needs
ambient. **Both agree that a quantity is computed once and read many times**, which is also
CLAUDE.md's own rule that no self-declared primitive stands more than once. Taking that.

## WHAT THIS ITEM SAID FIRST WAS WRONG, and its own control said so

It was filed as *"the sky lights only the exposure meter; the subject gets a declared constant"*,
because `IrradianceBuffer` has exactly one consumer and `SubjectEnvironment` is three doubles. The
control it wrote down was: *if the picture does not change, the constant was already being set from
the same irradiance upstream and this item is withdrawn.*

It is. `src/engine/Live.cpp:400-428` runs `MediumSkyIrradiance` over the same `Render::Medium`,
with the same transmittance and multi-scatter closures the GPU chain uses, and ADDS it to
`environment.RadianceLinear`. The subject is lit by the sky's own irradiance and has been all
along. **The fifth written-down cause this session to fail its own measurement**, and the reason it
was caught is that the item stated the measurement before the work started.

## What is actually wrong, measured

**One.** The same physical quantity is computed in TWO places: `MediumSkyIrradiance` on the CPU per
restand for the subject, and the `Irradiance` stage on the GPU for `AutoExposure`. Same model, same
inputs, two implementations, and nothing holds them to each other. CLAUDE.md forbids exactly this
and asks for a guard that counts it.

**Two.** The subject's half is FLAT: `struct SubjectEnvironment { double RadianceLinear[3]; }` --
one RGB for every normal direction. A wall facing the sun's side of the sky and a wall facing away
receive the same ambient. Unreal's SkyLight is directional (an SH or cubemap), RAGE's probe is
directional; neither ships a hemisphere average, because the alley wall is exactly where it shows.

- [ ] sky irradiance is computed ONCE and both the exposure and the surfaces read that one
- [ ] a guard counts the implementations of it and refuses a second
- [ ] the subject's ambient varies with the surface NORMAL rather than being one value per scene

## WHAT GRADES IT, and Cycles is not it

`test/CORPORA.md` section 9 states it plainly: **Blender cannot be this corpus's oracle, because
Cycles does not model an atmosphere.** So the render corpus's silence about the sky is not a gap in
the cases -- it is a gap in what that oracle can answer, and no threshold over a Blender frame will
ever grade a sky model.

    ebruneton/clear-sky-models input/   BSD   TRUTH   Kider et al.'s MEASURED full-day sky
                                                      irradiance for a real clear sky, plus the
                                                      ASTM G173 reference solar spectrum -- the
                                                      ground truth eight published sky models were
                                                      graded against, one of them this tree's
    precomputed_atmospheric_scattering  BSD   ref     where this tree's constants come from

That second line is a debt as much as an oracle. `RayleighScatteringPerKm = {0.005802f, 0.013558f,
0.033100f}` stands in `ParticipatingMedium.h:18` and `grep` finds it in NO board item, so under
*every number carries its origin* it is undeclared. Citing Bruneton discharges half of it; the
other half is the derivation, which the paper states and this tree does not.

**AND THE COST, carried from board:2057 which was withdrawn as a duplicate of this item:**

    Shibuya's rebuild                 907.8 ms
      the medium's own tables         778.9 ms   86 per cent, on the CPU
      everything else in Live::Stand   10.7 ms

3 276 800 transmittance steps for one ambient radiance: MediumSkyIrradiance sweeps 8x8 directions,
each calling MediumMultiScatterTexel over 8x8 directions at 20 steps, each of those calling
MediumTransmittance at 40. The device builds the same table in a 256x64 texture in a stage that
already runs, which is the second reason the CPU half should not exist.

**THE ATMOSPHERE IS TO BE PURE GPU.** A faster CPU implementation was written and reverted unmade:
speeding up the copy that should not exist is the wrong repair.

**The measurement that would show I am wrong**: if the CPU and GPU paths already agree to within
the noise, the duplicate is harmless and only the flatness remains -- that is a case, comparing
`MediumSkyIrradiance` against a readback of `IrradianceBuffer` for one declared sun elevation.
Negative control for the third predicate: a scene lit by sky alone, no key light, where a flat term
and a directional one cannot look the same. `khronos/glTF` guards it -- those cases declare their
environment, so a change that moves them is wrong.

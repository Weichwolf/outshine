Type: defect
State: active
Area: render
Tags: atmosphere, performance, measured

# The sky's irradiance is computed ONCE, or on the device that already computes it

**Benchmark** — Unreal: `FSkyAtmosphereRenderSceneInfo` builds the transmittance and multi-scatter
LUTs in COMPUTE SHADERS once per sun move, and everything that needs the sky's contribution samples
those textures. RAGE: its sky is a baked table sampled per frame. **They agree**: the integration
runs on the device, once, and is SAMPLED thereafter. This tree does both -- the device stages exist
and run -- and then integrates it a second time on the CPU.

## What was measured

Shibuya's rebuild, isolated with a timer per phase of `Live::Stand`:

    rebuild, in all                              907.8 ms
      INSIDE Build                               802.3 ms
        shaping it a second time                  10.7 ms
        the proxy taking it                        0.018 ms
        placing every part                         0.000 ms
        dressing them                              0.001 ms
        the lamps and the key                      0.000 ms
        THE MEDIUM'S OWN TABLES                  778.9 ms
        sweeping the bounds to frame it            0.000 ms
      shaping what was built                      11.4 ms

**778.9 ms of 907.8 -- 86 per cent of the rebuild -- is `Live::Stand` integrating the atmosphere on
the CPU** to reach ONE ambient radiance and one ground radiance. It is a nested integration:
`MediumSkyIrradiance` calls a second-order term that calls `MediumMultiScatterTexel`, which calls
`MediumTransmittance` at `kTransmittanceSteps` per sample.

**The device already has it.** `Stage::MediumTransmittance` and `Stage::MediumMultiScatter` are
declared stages with `mediumTransmittance.msl` and `mediumMultiScatter.msl` behind them, and they
run every frame at 0.000 ms of encode. So the tree computes the same tables twice, once on the
device where they are cheap and once on the CPU where they cost 0.78 s.

## What was WRONG about board:2056's premise, and this is why it is filed separately

board:2056 says "748.6 ms is one thread packing channels" and cites the same rebuild. That was read
off `rebuild: of that, walking it into the proxy` without asking what the phase CONTAINED, and the
phase contained this. Channel packing is 0.018 ms. The scheduler item stands on its own reasons --
serial generators, four thread groups, no planner -- but not on that number, and board:2056 is
corrected in the same commit as this filing.

**A sixth measure that could not see was found on the way here.** `InsideMs_` was assigned twice in
`Live::Stand`, the second time from a clock that the line above had just reset, so
`rebuild: standing and submitting INSIDE Build` read 0.000 ms for as long as it has existed -- over
the 802 ms it was measuring.

## What will be true

- [ ] the ambient and ground radiance a subject stands under are SAMPLED from the tables the device
      already builds, or computed once and held until the sun moves
- [ ] Shibuya's rebuild falls from 907.8 ms to under 150, and the number is quoted from a run
- [ ] the picture does not move: Shibuya holds `aff2f732` and the other five places hold theirs
- [ ] the negative control: forcing a recompute every rebuild puts the 778.9 ms back

## What this does NOT cover

Whether the CPU integration is CORRECT is not in question here and is not measured by this item --
only that it is computed where it is expensive and already available where it is cheap. If the two
disagree, that is a finding and it belongs in its own item rather than being smoothed over by
picking one.

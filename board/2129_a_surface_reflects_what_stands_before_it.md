Type: feature
State: open
Area: render
Tags: architecture, look, owner

# A surface reflects what stands before it

**Benchmark** -- Unreal: reflection captures (sphere and box probes) blended per pixel, screen-
space reflections over them, planar reflections for water and mirrors, Lumen's reflection ray
where the hardware allows. RAGE: cube-map reflection probes per interior and a global sky probe,
screen-space reflections on wet roads, planar reflection for the sea. **Both agree** on the
ladder: a probe answers what the screen cannot see, the screen answers what the probe has too
coarsely, and a plane answers a plane. Where they differ is only how many rungs the budget buys.

## Where it stands, measured 2026-09-04

```
  grep -rni 'reflection|probe|planar' src/render     0 hits that are a pass
```

The renderer has a sky irradiance stage and a specular lobe that reads the SKY, and nothing that
reflects the WORLD: a wet street shows sky and no lamp, a lake shows sky and no mountain, glass
shows sky and no city. CLAUDE.md names *reflections and mirroring* fifth of the five things the
budget is laid out for.

## What will be true

- [ ] A PROBE: a cube map captured at a declared place, filtered per roughness, read by the
      specular lobe in place of the sky where a probe stands
- [ ] SCREEN-SPACE reflection over the probe, from the depth pyramid the cull already builds
      (`DepthPyramidStage`), so what the screen sees reflects at full detail
- [ ] A PLANE for water: the water generator's surface reflects the scene mirrored about its
      plane, because a lake is the one case where the other two rungs are visibly wrong
- [ ] Each rung has a picture in `make shots` where it is the difference -- a lake at Jura, a wet
      street at Kaiserberg -- looked at before its number is believed
- [ ] The frame holds 16.7 ms at p99 with all three on at the 720p target
- [ ] Negative control: switch the plane off and the lake's picture moves in the water and
      nowhere else

## What will show I was wrong

If the screen-space rung costs more than the probe saves at 720p, the ladder has two rungs on
this target and the item records the measurement that decided it.

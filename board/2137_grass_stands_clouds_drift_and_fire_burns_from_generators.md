Type: feature
State: open
Area: generators, render, engine
Tags: architecture, look, owner
Depends: 2126, 2122, 2128

# Grass stands, clouds drift and fire burns -- from generators, lit by the engine

**Benchmark** -- Unreal: grass is the Landscape grass system (instanced, density from the
class layer, streamed in cells), clouds are Volumetric Clouds (a noise volume ray-marched in
the sky pass), fire and smoke are Niagara particles lit by the scene. RAGE: grass batches per
block, a cloud layer, and a particle system (`ptfx`) with the same three roles. **Both
agree**: three generators -- scatter, volume, particle -- and ONE lighting; the generator owns
the FORM and the renderer owns the LOOK, which is the fifth invariant's own words.

## Where it stands, measured 2026-09-04

```
  grass, clutter      no scatter/ area -- nothing places an instance over an area
  clouds              no cloud/ area -- the sky is a clear atmosphere
  particles           the word does not occur in src/ or include/
  rain, snow, fog     weather is a DATUM the medium obeys; nothing falls
```

CLAUDE.md's first budget line is *high geometry with RECURSIVE generators*, and a world with
no grass, no cloud and no fire is a world a photograph argues with at once. A Fallout without
smoke and a Cyberpunk without rain cannot be declared.

## The solution -- three generator areas, one renderer path each

| area | in | mechanic | out | the renderer's part |
|---|---|---|---|---|
| `scatter/` | class + ground | area -> instances | grass, stones, clutter as instanced pieces per tile (board:2122) | the instance path it has, at the tile's rung |
| `cloud/` | weather | noise -> volume | a density field per sky cell | a VOLUME pass inside the medium: ray-marched, lit by the same transmittance the sky already computes |
| `effect/` | a declared emitter | seed -> particles | quads and volumes per frame, GPU-simulated | a particle pass lit by the clustered lights (board:2128) |

All three are PLACERS or EMITTERS a scenario declares and a client could replace through the
door (board:2126). A cloud's FORM is invention; its scattering is the medium's physics, which
is why the volume pass lives in the renderer beside the sky and not in the generator.

## What will be true

- [ ] `scatter/`, `cloud/`, `effect/` exist with a `reaches` each and no cross-area include,
      which is board:2110's cut extended by three
- [ ] Jura shows grass on its meadows and a cloud over its ridge; Kaiserberg shows rain; a
      declared fire at OldTown shows smoke -- each looked at, each under its digest
- [ ] Each holds 16.7 ms at p99 at the 720p target with the others on
- [ ] Negative control: switch the cloud volume off and only the sky's pixels move

## What will show I was wrong

A cloud that looks right only at one sun angle. Then the volume pass is not reading the
medium's transmittance and has its own lighting, which is the split the invariant forbids.

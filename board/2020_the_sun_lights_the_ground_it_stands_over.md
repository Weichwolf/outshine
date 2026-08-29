Type: bug
State: active
Area: render
Tags: measured

# The sun lights the ground it stands over

**Benchmark** — Unreal: a `UDirectionalLight` drives the atmosphere and the ground through the same transmittance; a `SkyAtmosphere` gets its sun from that one light. RAGE: the timecycle drives sun colour and ground lighting from one curve. **They agree**, so the matter is closed: one sun, one direction, and both the sky and the ground read it.

MEASURED, at flight altitude over the five places. Local times from this machine's clock: Phoenix
10:36, Tokyo 02:36, Berlin 19:36. Shibuya being black is CORRECT at 02:36 -- what is absent there
is a moon, stars and city light, and no item claims those yet.

The Grand Canyon is the finding. At 10:36 in the morning, with the sun about 50 deg up, it renders
as blue dusk: the terrain carries a flat blue that reads as ambient sky alone, with no directional
term and no shadow direction anywhere in the frame, and the sky above the terrain silhouette is a
uniform olive slab rather than a gradient.

Two separable defects and the item does not yet say which is which:

1. the ground takes no directional light from the sun, or takes it at the wrong elevation
2. the sky's own radiance is wrong at a high sun -- olive is not a colour the atmosphere makes

## Measured, off the rendered frame

Channels sampled from `build/places/GrandCanyon.png` at 10:36 local, sun about 50 deg up:

    where                R     G     B
    sky, top row        63    73    52
    terrain             50    72   103   (range 50-74 / 72-98 / 103-113)
    Medium::GroundAlbedo 0.10  0.13  0.07

TWO SEPARATE DEFECTS, and the numbers separate them.

**The sky wears the GROUND's albedo.** Its channel ORDER is G > R > B, which is the order of
`GroundAlbedo` and not of any sky. Rayleigh scattering goes as lambda^-4, so a clear sky is
blue-dominant by a wide margin and no sun elevation, turbidity or exposure can reorder it. The
ratios agree too: measured G/R = 1.16 and R/B = 1.21 against the albedo's 1.30 and 1.43. Bruneton's
model does take a ground albedo for its irradiance term; something is letting that term stand for
the whole view ray.

**The terrain takes no sun.** Its albedo is green-dominant and it renders blue-dominant, so the
only illuminant reaching it is the sky. A directional term at 50 deg of elevation would swamp that.

Shadows are ACTIVE, not absent: `Live.cpp` falls back to `0.5 * sqrt(across)` when a scenario
declares no `ShadowRadiusM`, so the plan carries `lightVisibility` and it runs. That fallback is now
sized by the ring, which the cascade grew to 127 km -- a shadow map covering 127 km across
`kShadowAtlasPx` has texels hundreds of metres wide. So the shadow is drawn, correct in structure
and useless in resolution, and it cannot be judged at all while the ground takes no directional
light. That ordering is why this item is one item: the sun first, the shadow after.

## The measurements that would show I am wrong

1. **The sun's own numbers first.** `SolarAt(36.0616, -112.1076, now)` must read roughly 50 deg of elevation at 10:36 local. If it reads a few degrees, the defect is in the solar term and not in the renderer, and this item is misfiled
2. **The negative control is the terminator.** Render the same place at declared 06:00, 12:00 and 22:00 local: the ground's mean luminance must rise and fall with the sun's elevation. If all three match, nothing about the sun reaches the ground
3. **The sky at a high sun is BLUE at the zenith.** Sample the frame's top row: at 50 deg of solar elevation the zenith must be blue-dominant. Olive means the green channel outruns the blue, which the Bruneton fit cannot do for a clear sky

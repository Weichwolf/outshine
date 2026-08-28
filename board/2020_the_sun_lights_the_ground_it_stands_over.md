Type: bug
State: open
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

## The measurements that would show I am wrong

1. **The sun's own numbers first.** `SolarAt(36.0616, -112.1076, now)` must read roughly 50 deg of elevation at 10:36 local. If it reads a few degrees, the defect is in the solar term and not in the renderer, and this item is misfiled
2. **The negative control is the terminator.** Render the same place at declared 06:00, 12:00 and 22:00 local: the ground's mean luminance must rise and fall with the sun's elevation. If all three match, nothing about the sun reaches the ground
3. **The sky at a high sun is BLUE at the zenith.** Sample the frame's top row: at 50 deg of solar elevation the zenith must be blue-dominant. Olive means the green channel outruns the blue, which the Bruneton fit cannot do for a clear sky

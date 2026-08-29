Type: bug
State: active
Area: render
Tags: measured, places

# The Alps stand on the Jura's horizon

**Benchmark** — Unreal: a `WorldPartition` far-distance HLOD plus `SkyAtmosphere` aerial perspective
carries silhouettes to the horizon, and Unreal ships that as the demonstration of long sight. RAGE:
the LOD pyramid keeps a coarse shell to the draw distance and the timecycle fogs it. **They agree**
that distant relief is DRAWN, coarsely, and faded by the atmosphere rather than dropped. **Taking
that.**

The named expectation, in the goal's own words: from the Jura, across the Mittelland, you must see
the Alps. `build/places/Jura.png` shows a flat green horizon and no mountains.

MEASURED, from `outshine-places-RenderTheAlpsFromTheJura.log`:

    Jura  100 tile(s) over 8 levels, 229247 triangle(s), 15304 m relief, reach 395.3 km

So the ring REACHES far enough -- 395 km against Alps at 100 to 150 km. The tiles are there.

**AND THE RELIEF NUMBER CANNOT BE USED, which is the first thing this item must fix.** Over a 395 km
reach the Earth alone drops `d^2 / 2R` = 395000^2 / (2 * 6371000) = **12 244 m**. Add three-odd km of
real terrain and 15 304 m is what a SPHERE reads, not what a mountain range reads. The instrument
measures the curvature and the relief together and cannot separate them, so it has never been able
to say whether elevation data reached the far tiles at all.

## What must be measured before a cause is written

1. **Relief with the sphere taken out.** Height above the ellipsoid per vertex, not the spread of the
   frame's y. If the far tiles read a few metres of spread, the elevation never arrived and the
   defect is streaming, not drawing
2. **Whether the summits are BEHIND the horizon or MISSING.** Eye height and the geometric horizon:
   at h metres the horizon is `sqrt(2Rh)`. The case eye is 60 m above ground on a Jura ridge; the
   ridge's own altitude decides everything and is not currently published
3. **Whether the far tiles are drawn at all.** The cascade skipped 28 tiles as covered; nothing says
   whether a tile 130 km out survived culling and reached a draw

## The measurements that would show I am wrong

- **The negative control is the same frame with the sight distance cut to 50 km.** The horizon must
  visibly move IN. If the picture does not change, the far tiles are not reaching the frame and no
  amount of elevation data will put the Alps in it
- **A summit's height, read back through `sampleHeight`** at Mont Blanc's coordinates: 4808 m is the
  oracle and it is a TRUTH-grade number, not ours. If `sampleHeight` reads it and the frame does not
  show it, the defect is downstream of the data and the item can stop asking about streaming

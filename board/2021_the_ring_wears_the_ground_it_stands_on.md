Type: feature
State: open
Area: world, render

# The ring wears the ground it stands on

**Benchmark** — Unreal: a landscape carries per-component weightmaps and a material that blends real layers; Cesium for Unreal drapes raster overlays onto the tileset. RAGE: terrain carries authored diffuse. **Neither paints a planet in one flat colour**, so the matter is closed on whether the ring may carry imagery.

**AND THIS TREE ALREADY DID IT.** At `60dda039`, when flightbox was a flight simulator, the terrain
stage sampled an orthophoto composite per tile:

    let base = textureSampleBias(albedo, samp, in.uv, i32(in.layer), gbias).rgb * in.gain;

with a per-tile `gain` lifting the dark low-zoom Esri composite toward the bright orthophoto level,
and a grazing-angle mip bias derived from the view ray rather than the surface normal -- because the
per-tile uv derivatives underestimate the footprint on coarse far tiles. The owner's own note in
that shader records the trade decided by measurement: sharpness over streak-freedom at the horizon
band, residual streaks accepted.

Today `Picturing.cpp` paints the whole ring with `Medium::GroundAlbedo` -- ONE colour,
`{0.10, 0.13, 0.07}`, across 127 km. No imagery, no per-tile variation, no layer. That is why the
places read as coloured relief rather than as places, independent of any lighting defect.

## The owner decided: imagery, OPTIONAL, beside the elevation data

CLAUDE.md lays outshine out for procedural geometry and parameterised materials and says textures
should almost not exist. Ground imagery is where that reads two ways, and the owner settled it:
imagery comes back as an OPTIONAL layer beside elevation, and it is explicitly useful for judging a
place from altitude.

The reason it does not contradict the vision: elevation and OSM are DATA about the Earth that arrive
over the wire, and an orthophoto is the same kind of data. It is not a texture an artist authored,
it is a measurement of the place. Optional is the load-bearing word -- a scenario that declares no
imagery still stands, and the engine's own default fills in, which is the DECLARED-NOT-CODED
invariant rather than an exception to it.

## What will be true

- [ ] imagery is a PROVIDER beside elevation, declared and refusable: a scenario that asks for none still stands, and the engine's default fills in
- [ ] a tile carries its own albedo layer, sampled with a grazing-angle mip bias taken from the VIEW RAY rather than the surface normal -- the old shader's finding, and its reason is that per-tile uv derivatives underestimate the footprint on coarse far tiles
- [ ] a per-tile gain lifts a dark low-zoom composite toward the orthophoto level, because the old tree measured that it must
- [ ] the flat `GroundAlbedo` constant no longer paints the terrain path

## The measurements that would show I am wrong

1. **The flat constant is visible as a constant.** Sample the rendered ring at twenty scattered points: today their hue is identical and only the shading differs. Any answer to this item must break that
2. **A per-tile gain is not cosmetic.** The old shader carried one because the low-zoom composite is measurably darker than the orthophoto; if imagery lands without it, the far field must read darker than the near field at the same sun angle, and that is the number


## THE LAND CLASSES ARE WIRED, AND THEY ARE NOT ENOUGH -- MEASURED

Twenty land classes with an albedo each are loaded at every start from
`world/ground-materials.json`, and `ClassField` already knows which one stands at a point. Nothing
outside that tier could read them, so one flat `GroundAlbedo` painted a continent. They are joined
to the ring now as a VERTEX colour -- per vertex rather than per surface, because a class boundary
runs through a triangle and splitting the ring per class would multiply its parts by twenty. Unreal
blends landscape layers per vertex and per texel for the same reason.

    ring vertices a land class names
    Rothenburg   131 820 of 653 400    20 per cent
    Venice        50 442 of 653 400     7.7
    Grand Canyon   1 998 of 574 992     0.35

That settles what the classes CAN do: OSM land use covers a fifth of a German town's ring and
essentially nothing of a canyon. The Grand Canyon's desert stays green because there is no polygon
over it to say otherwise, and no amount of wiring changes that.

**So imagery is not a preference here, it is the only answer for wilderness**, which is what the
owner already decided. The classes remain worth their wiring where they do cover -- a town's fields,
water margins and woods -- and they are the fallback where imagery has not arrived.

Type: bug
Area: render
Tags: perf, instrument

**No texture in this engine has mipmaps**

`src/render/stages/SubjectDraw.cpp:882` creates every surface texture with **`num_levels = 1`**, and
`:915` sets **`mipmap_mode = NEAREST`**. There is no minification filtering anywhere: a 2048×2048 map on
a subject spanning a few hundred pixels is bilinear-sampled from level 0.

**Found while investigating `board:1126`**, where it is the leading candidate for a shading-normal
disagreement — but it is a defect in its own right and larger than that item, which is why it is filed
separately rather than repaired inside it.

**Two costs, and the second is the one that matters at the target.**

**Aliasing.** Point-sampling a minified map is the textbook definition of it. On a *normal* map it does
not merely shimmer — it changes the shading, which is what `1126` measures: our perturbation stays at
full strength where a filtered one would flatten, by up to **2.33×** in tilt.

**Bandwidth.** This engine's stated budget is **720p60 on five GPU cores**, and sampling full-resolution
textures at high minification is the worst case for a texture cache: every neighbouring screen pixel
reads a distant texel. **An OSM-scale world with hundreds of building types and hundreds of plants will
be texture-bound long before it is triangle-bound**, and no mip chain means no locality.

**The repair is not simply *enable them*.** Averaging unit vectors shortens them, so a mipped normal map
loses perturbation strength with level — the literature normalises after, or carries the lost length as a
roughness term (Toksvig, LEAN). **A round that enables mipmaps without deciding that is trading one
wrong picture for another.**

**Done when** every surface texture carries a mip chain, the normal map's treatment names which of those
answers it takes and why, and **no case moves in the picture bound** — or a case that moves does so with
its move attributed and defended.

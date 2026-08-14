Type: bug
Area: gltf

**glTF's minFilter names two things and the reader carried one**

`minFilter` packs the filter WITHIN a level and the filter BETWEEN levels into one integer. The reader
decoded it with

```cpp
Filter FilterOf(int raw) { return raw == 9728 ? Filter::Nearest : Filter::Linear; }
```

and stated the rule in its comment: *"9728 is NEAREST and everything else in the format's set is a
LINEAR base."* **That is false for the two values that are a nearest base WITH mipmapping**, 9984
`NEAREST_MIPMAP_NEAREST` and 9986 `NEAREST_MIPMAP_LINEAR`.

**Both are in the corpus.** `NormalTangentTest` and `TextureSettingsTest` declare 9986, so two subjects
were being filtered **linearly under minification where their files ask for point sampling** — and
`normal-tangent-mirror` samples its normal map at 1.42 texels per screen pixel, which is minified.

**The mip half was lost outright**, decided instead by a constant at the sampler
(`mipmap_mode = LINEAR`), so a file could not state it at all.

**And an unknown value was accepted silently**, mapped to LINEAR, where a wrap mode glTF does not define
is refused by name. Same field, same reader, two different stances on the same question.

| what it is now | |
|---|---|
| `KnownMagFilter` | two legal values, and 9984..9987 are **refused** — a mip mode has no meaning at magnification |
| `KnownMinFilter` | all six values, both halves, and anything else refused by name |
| absence | linear base, linear levels — exactly what the sampler did unconditionally before, so **only a declaration moves a picture** |

Held by `test/unit/gltf/ASamplerCrossesWithBothHalvesOfItsMinFilter.cpp`, which tables all six values
because **the defect was a rule and not a slip**: a test of one or two values would have been written by
whoever believed the rule and would have agreed with it.

## Comments

**2026-08-14** — Found from the other end, while looking for why 13 cases sit outside the picture bound.
The reader half is fixed and tested here; carrying `Minify` and `Mip` through to the sampler is the
remaining half, and it is what `board:1130` has been arguing about without knowing the file had an
opinion.

## The sampler half, and what it moved: nothing, for a reason worth writing down

`SubjectTexture` gained `Minify` and `Mip`, both filled from the file, and the sampler takes each filter
from its own declaration instead of taking `Magnify` twice.

**And the corpus does not move by one pixel.** Measured before and after on every case that declares
9986 — `normal-tangent` 229.33018, `normal-tangent-mirror` 184.35696, `texture-settings-test`
209.34986 — byte for byte, with 20 of 34 cases within the picture bound either way.

**A control says why, and it is not that the wiring failed.** Forcing the MAGNIFICATION filter to
NEAREST moves the picture — `198856` to `198860` differing pixels — while forcing the MINIFICATION
filter moves nothing at all. A probe in `Upload` confirms six 2048×2048 textures arriving with
`min=NEAREST`. **So the declaration reaches the sampler and the sampler never uses it: this subject is
magnified at every pixel, and `min_filter` is unreachable on it.** Only 4 pixels answer to the
magnification filter at all, which is consistent with a normal map measured at **93.4 % neutral** — a
nearly constant texture filters the same either way.

**One thing found on the way is a correction, not a discovery.** `max_lod = 0` was carried as "no
mipmaps"; it is not. Lambda is clamped to `[min_lod, max_lod]` **before** the magnification test, so
that line forced magnification filtering at every pixel of every texture and made `min_filter`
unreachable by construction. "One level" is now expressed where it belongs — in the texture's LEVEL
COUNT — and the LOD clamp is left open at Vulkan's `VK_LOD_CLAMP_NONE`.

**And it contradicts a number this board carried.** `board:1130` records `normal-tangent-mirror`
sampling its normal map at **1.42 texels per screen pixel** and an LOD histogram with **288 807 px at
≈1.25**. Both are minification, and both are incompatible with a magnification filter deciding every
pixel. One of the two measurements is about something else — most likely a texel ratio computed without
the projection that actually reaches this subject. **It is written here rather than dropped**, and
re-measuring it belongs to `board:1130`, whose whole argument rests on it.

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

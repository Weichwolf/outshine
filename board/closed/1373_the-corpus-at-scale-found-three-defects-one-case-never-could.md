Type: bug
Area: corpus
Tags: khronos, oracle, instrument

**The corpus at scale found three defects one case never could**

**Sixty-seven cases were authored in one pass** from the pinned clone, taking the Khronos corpus from 47
cases to 94. Six of them refused, and the six were three distinct defects — each one invisible until a
subject that had it arrived.

## One: a primitive that names no material was a refusal, and the format says it is not

```
part 98 names no material, so a per-material colour has nothing to key on
```

**glTF 2.0 is explicit**: *when `material` is undefined, the primitive is rendered with the default
material* — metallic 1, roughness 1, base colour white. **Nine models in the index have such a
primitive**, and refusing them cost nine cases that could not run at all.

**The reserved key is `<default>`**, in angle brackets because a glTF material name is an arbitrary
string and the brackets put the format's own word somewhere a file is very unlikely to reach.

## Two: Blender duplicates a shared material, and the count was nearly predicted

[MEASURED] `CompareBaseColor` declares two materials and three meshes naming `[0, 1, 1]`. The scene
Blender builds carries **three** datablocks: `baseColor texture dielectric` and
`baseColor texture dielectric.001` — its duplicate-datablock suffix, because a material bound by two
meshes arrives twice.

**The suffix is READ BACK, not predicted.** A manifest that had to state `.001` would be stating a count
of Blender's copies; stripping a trailing `.NNN` maps the datablock to the name the FILE gives, which is
the key both sides already share. *The alternative — deriving the suffixes when authoring — is the
mistake `board:1362` already names: keying on another program's convention, which drifts at its next
release and then simply colours different bodies.*

## Three: a URL is ASCII on the wire and a model name need not be

```
UnicodeEncodeError: 'ascii' codec can't encode characters in position 92-93
```

`Unicode❤♻Test` is in the pinned index. `http.client` encodes the request line as ASCII, so the fetch
raised **before a byte was sent**. The path is percent-encoded now, with `%` left safe so an
already-escaped triplet is not escaped twice; the digest still pins the same bytes.

## And a fourth thing, which was mine and not the tree's

**Four manifests declared a colour for a material no primitive binds** — and the four are
`StainedGlassLamp`, `SheenChair`, `GlamVelvetSofa` and `DragonAttenuation`. The unbound materials are
`KHR_materials_variants` alternatives: declared by the file, bound by no primitive under the default
variant. **The oracle walks material SLOTS and the authoring pass walked the `materials` ARRAY**, which
are the same list only for a file with no variants.

*The pattern across all four is one sentence: **a corpus of one is a corpus that agrees with whatever
you assumed.** None of these could be found by making the existing cases greener.*

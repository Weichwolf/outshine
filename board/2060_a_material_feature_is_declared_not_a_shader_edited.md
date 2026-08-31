Type: task
State: active
Area: render
Tags: shaders, permutations

# A material feature is DECLARED, and a shader file is not edited

**Benchmark** — Unreal: a material is a graph, and the permutations a scene needs are GENERATED from
it against the vertex factories in play; nobody writes `FLocalVertexFactory_Masked_Textured` by
hand. RAGE: shader packs carry techniques and passes, and a technique names the state rather than
duplicating the program. **They agree that the variant set is DERIVED**, and this tree writes it out.

## What was measured

    19  fragment entry points across the .msl files, by hand
    19  string literals in SubjectDraw.cpp that ask the driver for them, by hand
    38  entry points in all once the vertex arms and the shadow pass are counted

The axes are lit, textured, normal-mapped, and one of opaque / masked / blended / transmissive --
nineteen of the thirty-two the four of them span, chosen because those are the ones somebody needed.
**A fifth axis costs nineteen more of each, written twice in two languages.**

The macros are already there -- `SUBJECT_LIT_ARM`, `SUBJECT_EMITTED_ARM`, `SUBJECT_MAPPED_ARM` --
so the mechanism for a variant exists and is invoked by hand. What is missing is the declaration
they should be invoked FROM.

## What will be true

- [x] the axes and the combinations that stand are ONE table, and both the shader arms and the
      names the renderer asks for are generated from it -- **for the VERTEX arms**
- [ ] the same for the FRAGMENT entries. Nineteen of them, and unlike the vertex arms they are
      hand-written BODIES with different logic per alpha kind rather than macro invocations, so
      generating them means macro-ising the bodies first. That is the larger half and it is what
      remains of this item.
- [ ] a new material feature is a row, and no .msl and no string literal is edited to add it
- [ ] the pictures do not move, because a generated variant set that draws differently is a
      different variant set

## THE VERTEX HALF IS ONE TABLE, and the pictures did not move

`src/render/stages/VertexArms.h` holds the sixteen rows as
`(Layout, Normal, Tangent, Uv, Uv1, Colour)`. Both sides are generated from it:

- the NAME, by the rule `vs` + base + `Textured` + `Two` + `Tinted`, with the one exception the
  hand-written switch already carried -- a `Mapped` arm always has a uv, so its name does not
  spell it;
- the MSL invocation, with the attribute list and the three value expressions.

Two `static_asserts` hold the table to the truth: every `VertexLayout` has a row, and every row
sits at the index its own layout names, so the lookup is not a search. A missing arm now fails to
COMPILE rather than at pipeline creation.

Sixteen `SUBJECT_*_ARM` lines left the three .msl files and a sixteen-case switch left
`SubjectDraw.cpp`. **Heidelberg renders 0da91522 and Shibuya 732bd2de, both unchanged, and
`outshine/places` holds 8 PASS.** A generated variant set that draws differently would be a
different variant set; this one is the same one.

## AND THE GUARD BELOW WENT BLIND TO IT

`entries_vs_shaders.py` reads TEXT -- entry points spelled in an .msl, names spelled as string
literals beside the renderer. It went from `38 define, 37 name` to `23 define, 22 name` and
reported **0 named and undefined**, because BOTH of its inputs lost the same fifteen names at
once. It stayed green through exactly the change it exists to catch, approached from the other
side.

It now prints what it does not cover. The eighth check this session found that could not fail.

## What holds until then

`test/scripts/entries_vs_shaders.py`, in `make lint`: every name the renderer hands the driver is
defined by some .msl, and every entry an .msl defines is asked for by some stage. It reads all of
`src/render` rather than one caller, because the shadow pass asks for the depth-only arm from its
own file and a check that reads one caller calls the other's entries dead -- which it did, on its
first run, until it was widened.

A name asked for and not defined fails at RUN time with a driver's message rather than a compiler's,
which is the whole reason the guard is worth its twenty lines.

## What this does NOT cover

The shader TEXT. Generating the variant set does not make the lobes declarative; `metalRoughBrdf`,
`sheenLobe` and `iridescenceLobe` stay hand-written functions, and that is right -- Unreal's graph
generates the permutation, not the BRDF.

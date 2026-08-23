Type: issue
Area: test
Tags: claims, map, current-diagram

# Every amber node carries a citation too

board:1768 closed with 6 of 6 red nodes cited across every diagram (24 citations,
`sregex_iterator` over every `class ... wrong` line). Its own title says **every NON-GREEN**
node, and its closing note says the quiet part out loud:

> Gate 234/234. Amber is still unjudged, which this item does not claim to have fixed.

`test/harness/claims/EveryColourCitesALineThatSaysIt` walks `class ([A-Za-z0-9_,]+) wrong`
and nothing else. `CLAUDE.md` paints amber with `class ... unsure` in four diagrams; the
class-structure CURRENT diagram alone carries 15:

```
class BuildingField,WaterField,Subject,DrawList,Renderer,TonemapStage,LightVisibilityStage,Frustum,Ephemeris,RegionForge,GltfStudio unsure
class DriveAssembly,CorridorLay,DriveTick,TilePool unsure
```
— CLAUDE.md:230, :232, plus `class TAA unsure` at :145

Four of those fifteen already carry prose that says why -- `TilePool` (:250-252, with its own
`grep -c` disproving the old red), `LayCorridor`/`AssembleDrive`/`DriveTick` (:252-253). The
other eleven, and `TAA` at :145, carry nothing. And no walk checks any of them, so the four
that are justified are justified by luck of the editor rather than by a gate.

Amber is defined by the map as "form in question" / "uncertain". An uncertainty with no
citation is not an uncertainty, it is a shrug: nobody can tell whether `Frustum` is amber
because someone measured something or because someone did not look. The reviewer's step 4
("spot-check the red/amber nodes of the CURRENT diagrams against the code -- a lying map is
itself a finding") cannot be mechanised while half the palette is unwalked.

## What will be true

1. Every node any diagram paints `unsure` is named in a justification row that states the
   QUESTION -- what would settle it -- and cites the line the question is about.
2. `EveryColourCitesALineThatSaysIt` walks `unsure` with the same `sregex_iterator` it now
   walks `wrong` with, and its CHECK text names the count it judged.
3. A node whose question is answered moves colour in the same commit that answers it; an
   amber that has carried the same citation for ten rounds is a finding of its own.

## Comments

- Successor to board:1768, which delivered the red half honestly and named this as the
  remainder rather than pretending otherwise. That is how a half-closure should read -- the
  half that was NOT done gets its own number instead of dying in a comment.

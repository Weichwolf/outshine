Type: bug
Area: render
Tags: scope

**German in an English-only repository, and it cites a numbering that is gone — **Band 2****

`CLAUDE.md`'s first rule is that the repository speaks one language. Two comments are in German, and
both compound the error by citing a numbered principle list that the current `CLAUDE.md` does not have —
it carries *the constraints*, *stance* and *setup*, with no numbered principles at all.

- `src/scenario/Animation.h:15` — *"a bespoke format here would be the parser nobody ordered (Prinzip 1)"*.
- `src/core/Keyframes.h:22` — *"(Prinzip 7: a run must …)"*.
- `src/render/Renderer.cpp:770` — *"(CLAUDE.md, Prinzip 5)"* — half-translated, and the cited number
  does not exist in the file it names.

the deleted terrain shader and `src/world/TerrainLoader.cpp:329` cite *"CLAUDE.md principle
2"* in English, which is the same dangling number in the right language. Right: the sentence the rule
actually is, quoted or paraphrased, with no number — a number into a list that is not numbered is worse
than no citation, because it reads as precise.

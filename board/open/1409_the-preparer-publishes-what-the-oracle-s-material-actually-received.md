Type: bug
Area: corpus
Tags: oracle, instrument

**The preparer publishes what the oracle's material actually received**

`provenance.json` records that the closure Cycles rendered was `BSDF_PRINCIPLED` and **not one of its
socket values**. So when a manifest declares `KHR_materials_clearcoat` and Blender's importer drops it,
the oracle renders a plain metal, this engine renders a coated one, and the case reports a residual --
or, worse, both sides render plain and the case goes **green while proving nothing**.

**That is not hypothetical and the measurement is already on the board.** [MEASURED] `board:1389`:
Blender's importer writes `Thin Film Thickness` **400.0 whatever the file declares**, over five tested
ranges, and has no socket for `iridescenceFactor` at all. A case declaring a 200 nm film would be
compared against a 400 nm one with nothing anywhere saying so.

**What stands in for it today is a hash comparison done by hand.** The three extension cases were
believed only after `oracle.raw` was shown to differ between them and from the plain metal sphere --
`c1c91e77` · `62d99056` · `1ba65209` · `962dbe47`. **That is a check somebody has to remember**, which
is the shape this tree's own rules refuse.

## What must be true

- [ ] **The provenance names every socket the render used**, read off the Principled node after the
  import, so the oracle's material is a published fact and not an assumption
- [ ] **A case declaring an extension the importer did not apply is a REFUSAL**, named, rather than a
  comparison of two pictures neither of which is the declared one
- [ ] **The declared-to-observed mapping is the manifest's**, so an importer that carries a value under
  a different name is a rename in one place and not a special case in the runner
- [ ] **It reads what Blender did and never what Blender should do.** A table of expected mappings kept
  beside the importer would drift the first time Blender changed one

## Why this is a bug and not a feature

**The code claims to do it.** `keep_file_materials` already collects an `observed` list and already
argues, in its own docstring, that what an arm cannot express is *recorded rather than hidden*. It
records the closure and the back-face rule and stops one field short of the values.

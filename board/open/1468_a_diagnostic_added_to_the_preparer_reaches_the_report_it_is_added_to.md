Type: bug
Area: corpus
Tags: instrument

**A diagnostic added to the preparer reaches the report it is added to**

A field added to what `in_blender_render.py` prints appears in the next run's `provenance.json`. The
render's own account is keyed on the preparer that produced it, so a preparer that says something new
is a preparer whose account misses.

## What it is

[MEASURED] a field added to `camera.derivedFrom` did not appear in the provenance of a case prepared
immediately afterwards -- not with `--force`, and not with `--force --no-cache`. The render's products
missed (`cache: miss`, both frames re-rendered), and the ACCOUNT beside them came back without the new
field.

`board:1154` made the render's account a keyed product of the render, stored and served like the bytes
it describes -- which is right, because it carries the pass-index mapping and is not reconstructible
outside a Blender run. **What is wrong is its key**: if it followed the same recipe key the products
follow, a preparer change would have missed it too.

## Why it matters more than a missing field

**An instrument that cannot be extended is an instrument you stop reaching for.** The round that found
this was two hypotheses deep into `board:1467` and could not ask the preparer a new question -- so the
question went unanswered and the item carries a list of things to try instead of an answer.

## What must be true

- [ ] **The provenance document's key covers the preparer**, by the same `render_code_digest` the
  products' key uses -- and by the case's own vendor's steps, which `board:1451` already made per-case
- [ ] **`--no-cache` reaches it**, because a flag that says *do not use the store* and then uses it for
  one document is a flag that lies
- [ ] **A run says which of its outputs came from the store**, per document and not only per product,
  so this is visible next time without a bisection

## Comments

Found while narrowing `board:1467`, and it is the reason that item ends with three things to try rather
than with the measurement it names. The `atEachInstant` evidence it does carry arrived before this
defect was in the way.

Type: feature
Parent: 1538
Area: render
Tags: budget, reference, owner

# The picture bar is Gran Turismo 7 on PS4, expressed as budgets this device is measured against

Owner requirement, 2026-08-24: *"grafisch wollen wir AAA niveau - gran turismo 7 ps4 ist was
grafisch auf dem a18 pro erreichbar sein sollte"*. Landed in CLAUDE.md's target block the same
session.

The value of naming a SHIPPED title on KNOWN silicon is that it is not an adjective. It converts
into a table, and every row of that table is either fetched from the reference or measured on
this device -- never assumed. Today the tree has neither the fetch nor the measurements, so the
bar is a sentence and this item is what makes it a number.

## What the table must hold

| row | fetched from the reference | measured here |
|---|---|---|
| triangles on screen, p50 / p99 | | |
| draws per frame, p50 / p99 | | board:1538's sweep |
| what a draw costs on this device | -- | board:1538's sweep |
| materials per vehicle, and how many are unique | | |
| lights that reach a frame, and how many cast | | |
| shadow map resolution and cascade count | | |
| texture residency, bytes on the device | | |
| what fraction of the frame is geometry, shading, post | -- | |

`GT7 on PS4` runs 1080p60 on 1.84 TFLOPS of GCN; the A18 Pro is a 5-core TBDR part at 720p,
which is 44 % of the pixels. Whether that trade is favourable is exactly the kind of claim this
item refuses to make from memory: **the numbers above are to be fetched, not recalled**, which
is CLAUDE.md's standing rule for references.

## What will be true

- [ ] The reference rows are FETCHED and cited -- a Digital Foundry frame analysis, Polyphony's
      own SIGGRAPH/technical talks, and the PS4 hardware figures -- with the source named beside
      each number, and a row nobody can source stays EMPTY rather than being estimated.
- [ ] The measured rows come from cases in `test/render/outshine/frame/` and
      `test/render/outshine/scenario/`, published p50/p95/p99 over a moving camera.
- [ ] The gap between the two columns is the render queue's order of work, and it is stated as
      such: the largest gap is the next item, not the most interesting one.
- [ ] Proving test: a scenario case that stands up a scene at the declared budgets and holds
      720p60 over a moving camera. Negative control: one budget row doubled -> the floor breaks
      and the case names which row did it.

## Comments

- 2026-08-24 -- filed the hour the requirement was given. Parent is board:1538 because the draw
  budget is the first row either column can hold, and board:1538's sweep is already the
  instrument for it.

Type: feature
Area: clients
Tags: scope

**The UI updates a number without reparsing the world, and script reaches the tree**

The review confirms the markup/style/layout tree as the right declarative game UI (flexbox,
specificity, UA sheet, hit-testing, WPT-scored) -- and names the two gaps between it and a HUD
at 60 Hz: `Live::Compose` re-parses markup and every stylesheet from strings on each
`Redeclare` (`Live.cpp:401-426`), so a speed readout costs a full reparse per frame; and the
script half exists (`Script.cpp`, test262-scored) but nothing wires it to the document tree.
Slate's invalidation-panel lesson: text-node mutation plus dirty-subtree relayout.

- [ ] a text node mutates and only its subtree relays out; the reparse dies
- [ ] script binds to the tree (events in, node mutation out), still declarative at the seam
- [ ] the driver's HUD (speed, route) is declared markup, updated at 60 Hz, measured

Sharpened (review 2026-08-23, round 15): the CURRENT class-structure diagram in CLAUDE.md
does not carry the ui tree at all — Markup → Style → Layout → Paint/Pointer exist in src/ui,
feed Live's overlay and the WPT harness, and appear on no map. When this feature lands, the
chain belongs on the diagram; until then the map under-reports a whole subsystem. The round
also put three concrete defects against the tree it confirms: board:1685 (baseline OOB),
1686 (only the first top-level box paints), 1687 (greedy selector matching refuses valid
trees) — the "confirmed architecture" verdict above stands, the implementation does not yet.

---

Map sharpening repaid (round 15's note): the CURRENT class diagram carries the ui chain --
Markup -> Stylesheet -> Layout -> Painting -> OverlayDraw -> Renderer, green per the round's
own layering verdict. A map that omits a subsystem lies by omission; it no longer does.

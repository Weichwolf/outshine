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

Type: feature
Area: render
Tags: scope

**One compositor interface, and the recomposition trigger is declared data rather than discovered by calling**

- [ ] **One interface, not two.** A static and a per-frame compositor differ in **what invalidates their output**, not in what they are asked. Terrain and city recompose on a **residency** change; forest on the same; traffic on the **clock**; and every one of them on a **view** change — but through the one shared LOD cut of § I.9 and not each on its own. Three inputs, one signature
- [ ] **The trigger is `constexpr` declared data on the compositor**, in exactly the shape `render/plan/RenderCatalogue.h`'s stage rows already use for `Reads`/`Writes`/`Contributes` — so the frame loop calls only the compositors whose declared inputs changed. **That is the difference between a no-op call forty times a second being *avoided* and being *unspellable***, and it is this section's answer to *what moved from forbidden to unspellable*
- [ ] **A compositor that declared no input is refused at compile time**, by the same class of `static_assert` § I.27 already carries: a compositor nothing can invalidate either never runs or runs always, and both are defects with one spelling

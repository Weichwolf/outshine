Type: feature
Area: render
Tags: instrument

**The migration hazard — and it is a requirement so that it cannot be read as a rebaseline**

- [ ] **`verify-still` holds `bec69fea0a4e6837` off today's everything-on path** (`Makefile:384-410`). When stages stop defaulting on, that client renders something different unless its declaration states what it previously got implicitly. **The declaration is written first and must reproduce the sha before any stage becomes optional** — that is the order of work, not a suggestion
- [ ] **A changed still is a finding to bring the owner, never a number to update.** The evidence is a picture pair at **320 × 180**, ranked by what destroys the impression fastest — light, colour, silhouette — with the sha quoted beside it and never in place of it. This line exists because the cheapest possible edit at that moment is a one-token hash change that looks like maintenance
- [ ] **The baseline is keyed by the plan's own digest**, so a plan change does not produce *"the sha changed"* — it produces *"this plan has no baseline"*, and a new baseline is a declared act with a name. That converts the tempting one-character edit into a statement somebody signed, and it is the shape that carries the rule instead of the sentence above carrying it
- [ ] The plan digest covers the stage set, the derived order, the merges, the aliases and the resource formats — everything whose change can move a pixel — and is published on every telemetry row, so two runs are comparable or are provably not (§ I.11). *The digest exists (`render/plan/RenderPlan.cpp:239-254`) and omits the frame extent, the vegetation-table branch and `FB_TAA` — the bug tasks in `board/`*


---

**Closed as stale (2026-08-22).** verify-still and its baseline sha died with the Makefile cut to build/test/clean; the digest-gap half lives in 0027.

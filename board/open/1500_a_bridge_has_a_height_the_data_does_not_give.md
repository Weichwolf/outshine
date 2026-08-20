Type: task
Parent: 1498
Area: world
Tags: scope

**A bridge has a height the data does not give**

**This is where the defects are, and the owner named it.** OSM says `bridge=yes` and `layer=1` and
**nothing about elevation**. A tunnel says `tunnel=yes, layer=-1` and nothing about depth. The terrain
comes from a DEM that knows nothing about either. So a bridge built from raw data runs along the ground
it is supposed to cross, and a tunnel runs through the hill instead of under it.

**"The data is not accurate enough" is not an answer** -- the owner's requirement stands: every road
connects and is drivable. **So the generator INFERS what the data underdetermines, to a declared rule,
and the rule is what a reader can argue with.**

## What must be true

- [ ] **A bridge deck clears what is beneath it by a declared headroom**, and its approaches ramp at a
      gradient no steeper than the road class allows -- so the ramp length is DERIVED from the clearance
      and the gradient rather than chosen
- [ ] **A tunnel's portal meets the surface road with the same continuity a junction has**, and the
      terrain above it is not the surface the car drives on
- [ ] **`layer` orders what crosses what** and a crossing where neither is a bridge is a refusal naming
      both ways -- OSM has those and they are data defects worth reporting rather than smoothing
- [ ] **What was inferred is PUBLISHED per road**, so a drive that crashed on an inferred deck can tell
      that from one that crashed on measured ground. *An inference nobody can see is indistinguishable
      from data*
- [ ] **The inference is DETERMINISTIC**: the same tile inferred twice is the same surface, or a route
      driven twice is two different worlds
- [ ] **Where the inference cannot close** -- a bridge whose ends the DEM puts at different heights than
      its span allows -- **it is a named refusal with the road's id**, and the router routes around it
      rather than the car driving off it

## Comments

**This is the item where the engine earns the sentence *the world is loaded, not modelled*.** Loading a
world that is underdetermined means completing it by rule, and the rules here are ordinary civil
engineering: a headroom, a maximum gradient, a transition length. *Those numbers are declared data and
not constants in the generator* -- a scenario for a mountain road and one for a motorway want different
ones.

Type: task
Parent: 0082
Area: harness
Tags: instrument, perf

**A compile group that can spell the world, and the boundary it must still refuse**

No suite that brings up a device can name a line of the world. `test/run.sh` gives `render` and `frame`
the same set — `-Isrc/core -Isrc/core/io -Isrc/gltf -Isrc/render/* -Isrc/clients` — with **no
`-Isrc/world` and no `-Isrc/generators`**, so **12 498 lines have no spelling in any test with a GPU**.
Until that changes, no world case can link, and this is therefore the first thing between the decision at
`board:1159` and its first measurement.

**IT IS ITS OWN ITEM AND NOT A LINE INSIDE THE CASE, and the reason is that the include sets ARE the
layering proof.** Every layering claim this tree makes is true because a name a layer must not reach has
no spelling in it. A group added as a side effect of getting one case to link is a boundary drawn by
accident, and it will be the boundary every scenario case inherits.

- [ ] **The group is `scenario`, and it is a fifth suite rather than a widening of `render`.** `render`
  and `frame` keep exactly the sets they have — **if the render suite gains the ability to spell
  `src/world`, the strongest proof in the tree is diluted invisibly**, and nothing would report it
- [ ] **What it may name is decided here rather than discovered**: the world, the generators, the
  clients, the renderer and core — the set `Clients::Sim` and a declared mod actually require, and
  nothing beyond it
- [ ] **What it must still NOT name is the point of the exercise.** A scenario declares and measures; it
  is not a second engine. The negative directions already proven by `test/outshine/unit/compile/*/…IsNotReachable`
  must survive unchanged, and the new group must not become the one place where everything is reachable
  because it was easier
- [ ] **No sanitiser on the timed path**, for the reason `board:0058` already states on the frame suite: a
  duration measured through a bounds checker is not the shipping frame. **And the same declaration must
  be runnable under a sanitiser in a separate arm**, or the timed code is code nothing checked — the line
  `0058` names as *the next line, not this one*, and this is where it comes due
- [ ] **The `Makefile` keeps three targets.** A scenario group is a compile group, never a fourth target
- [ ] **A negative test that the boundary holds**, in the shape the tree already uses: a compile-refusal
  case asserting that a name outside the declared set does not resolve from this group. A group whose
  boundary is only in `run.sh` is a rule a checker counts

**Done when** a source under a `scenario` group compiles against the world, the generators and the
renderer at once; `render` and `frame` still cannot spell either; the boundary is held by a compile
failure rather than by a line in a script; and the timed arm carries no sanitiser while a checked arm of
the same declaration exists.

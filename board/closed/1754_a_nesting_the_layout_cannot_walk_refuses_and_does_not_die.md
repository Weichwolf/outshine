Type: bug
Area: ui
Tags: robustness, layout, recursion

**A nesting the layout cannot walk refuses, and does not die**

`Markup::Read` parses arbitrary nesting iteratively (src/ui/Markup.cpp:190, an explicit
`open` stack) and hands the tree on. `Placer::Place` (src/ui/Layout.cpp:847) then walks
that tree by RECURSION -- :438 `y += Place(child, ...)` -- with no depth bound anywhere in
the file (`grep -n 'depth' src/ui/Layout.cpp`: nothing).

Measured (probe, `<div>` nested d deep, one text run, 800x600 viewport, -O2, 8 MiB main
stack):

| depth | markup | layout | process |
|---|---|---|---|
| 4000 | accepted | 4001 boxes | exit 0 |
| 4200 | accepted | -- | SIGSEGV |
| 6000 | accepted | -- | SIGSEGV |
| 50000 | accepted | -- | SIGSEGV |

The break is between 4000 and 4200 levels: about 8 MiB / 4100 = 2.0 KiB of stack per
`Place` frame, derived from the crash point and the default stack.

"A failure is loud" -- a SIGSEGV in a library is the opposite: no refusal, no message, no
`error` string, the host process gone. The tree already holds this exact rule elsewhere:
board:1712 bounded the Fit's Simplify recursion for the same reason, and
`src/core/io/StackProbe.h` exists to measure the resource being spent here.

What will be true:

1. `Layout::Build` carries a declared nesting bound with its derivation (frame size x
   bound < the stack the engine may assume) and REFUSES a deeper tree through the `error`
   it already takes -- or `Place` walks the tree on an explicit stack, as `Markup::Read`
   already does, and the bound moves to the heap.
2. A unit case in `test/unit/ui/` feeds markup one level past the bound and asserts the
   refusal text; today that case kills the runner.
3. The bound is stated in the header beside the type it guards, with its origin, not
   buried in the .cpp.

---

Closed -- Place carries a declared nesting bound with its derivation beside the type it
guards (kDeepestNesting [SET] 128: the walk spends ~2.0 KiB of stack a level, measured at
the crash point of 4100 levels over 8 MiB, and the shallowest thread this engine may run on
holds 512 KiB -- 128 is that budget quartered), the walk stops at the bound instead of
descending, and Build REFUSES through the error it already takes, naming the number and
clearing the boxes so nothing is half-placed. Proven in
unit/ui/ANestingTheLayoutCannotWalkRefuses: 64 levels lay out, 129 refuse naming 128,
20000 refuse with the process still standing to answer. Negative control: lifting the bound
past the stack turns the same test into 2 SIGNAL (plain and sanitised arms) -- which is
exactly the failure the item reported, now unreachable.

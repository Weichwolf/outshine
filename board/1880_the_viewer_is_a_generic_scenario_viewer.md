Type: feature
State: open
Area: apps
Tags: viewer, scenario

# The viewer shows any scenario, and adds nothing to it but its own face

`tools/viewer` moved to `apps/viewer`: it is not development support, it is a PRODUCT — the
second client outshine has, and the one that proves the door twice over.

**What it becomes when every case is a scenario:** a generic scenario viewer. It loads a
scenario the way any client does, and the only thing it contributes is its own UI, integrated
INTO the loaded scenario rather than wrapped around it. No case-specific knowledge, no corpus
vocabulary, no render-plan spelling — a viewer that needs to know which corpus a case came from
is a viewer that has a second door.

That makes it the sharpest test of board:1879 there is: if the viewer can show a Khronos case, a
WPT case and the driver's own drive without branching on which is which, then a case really is a
scenario and the door really is two headers.

## What will be true

- [ ] `apps/viewer` loads any scenario by path and shows it, with `--scenario PATH` and nothing
      case-specific in its arguments.
- [ ] Its own chrome is declared as UI INSIDE the scenario it loads, not composited over it by
      code that knows better.
- [ ] It compiles against `include/` alone, like the driver.
- [ ] Proving test: the viewer shows a corpus case and the driver's drive from the same binary
      with no branch between them.

## Comments

- 2026-08-25 — filed on the owner's instruction when `tools/` was deleted. `tools/host` went with
  it: `DelayedTransport` had no caller left after the unit cases were removed.

Type: issue
Area: clients
Tags: api, scope

**The door reads the traits it assembled**

`include/outshine/Traits.h` and `include/outshine/Assembled.h` stand in the PUBLIC door,
but no public call produces or reads them:

- `Engine::Assemble()` fills the Traits column inside `Engine::State`; neither `Engine`
  nor the public `Store` offers a query that returns a stood instance's resolved traits
  or its interned key table. A pure `include/outshine/` client can DECLARE kinds and
  instances and can never READ what resolved — the 1487 row is write-only through the
  one door. The door proof (`test/render/outshine/client/AClientRunsAScenarioInFourLines.cpp`)
  accordingly never checks a resolved value.
- The only producer of `Assembled` is the free `Assemble` in `src/clients/Assembly.h` —
  internal. `tools/driver/*` reaches around the door (includes `outshine/Assembled.h` but
  calls the src-side function), which is the pattern the one-door TARGET exists to end.
- The CURRENT public-interface diagram in CLAUDE.md omits `Park/Resume/Parked` and the two
  new headers (map currency; the Park row is fixed alongside this filing).

Demand: a public read path — `Engine`-side (`Stood()` returning the handles + a trait
query by interned key) or `Store`-side — and the door proof reads a resolved value through
it. Housekeeping in the same cut: `Traits::Named/Put` are `noexcept`, a `static_assert`
(trivially copyable, size) beside the struct; `Assembled`'s lookups `noexcept`.

---

Closed: the traits READ BACK through the same door they stood through --
Engine::Stood() (the assembled names) and Engine::Resolved() (the traits column) are on the
public handle, and the layered-mod proof reads the overridden 0.7 L from a client that
includes nothing but outshine/. The tools' direct src/ use stands by the tools rule
("tools/ builds ON the library"); the door proof remains the four-lines client. CLAUDE.md's
public-interface diagram carries Park/Resume/Discard/Parked.

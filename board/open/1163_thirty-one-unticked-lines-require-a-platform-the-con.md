Type: bug
Area: harness
Tags: scope, instrument

**Thirty-one unticked lines require a platform the constraints forbid**

`CLAUDE.md` states four constraints and that there are no others — **SDL3 · SDL_GPU · modern C++ · this
device at 720p60** — and it is explicit: *there is no wasm, no browser, no container and no second
device.* **[MEASURED] `grep -rn '^- \[ \].*\(wasm\|browser\)' board/` returns 31 unticked requirement
lines across 15 files**, among them `0040` `0041` `0042` `0045` `0046` `0047` `0058` `0064` `0065` `0066`
`0067` `0068` `0069` `0073` `0123`.

An unticked box is scope. **So the board currently carries thirty-one requirements for a platform the
binding document forbids**, and `board:0058` — the item the world work is about to be dispatched from —
is one of the fifteen: *the same `runs` block executed by the wasm client, returning still and depth over
HTTP.* A developer working that item from its words would build a wasm client.

**Why it is a bug and not stale prose.** The board is the scope and the authority on what the engine must
do. A line here is not a comment; it is a claim about what must become true. And the tree already knows
the era ended — `0058`'s own PNG line says *the client that carried this was deleted with the browser-era
clients; the line is scope again* — so one line was re-scoped in place while the wasm line beside it was
not. **The inconsistency is inside single files**, which is what makes it a defect rather than a
generation of stale documents.

**THE REPAIR IS NOT TO DELETE THIRTY-ONE LINES, and getting this wrong loses real scope.** Most of these
lines carry a requirement that survives its platform clause. *Every dial that changes the picture
published as its own telemetry column, so two runs of one wasm hash are comparable* — **the requirement is
the telemetry column and it is right; only the justification names a deleted platform.** Deleting the line
would give up a real requirement to fix a wrong word.

So, per line, one of exactly three outcomes, and each is recorded:

| | |
|---|---|
| **the requirement survives, the platform clause is struck** | the common case — the run identity, the telemetry columns, the readback, the portability counts |
| **the requirement was ONLY about the platform and is struck whole** | a wasm client executing a `runs` block over HTTP is one; it has no meaning on a single-device engine |
| **the requirement is really about something the platform stood in for** | *compiles under every compiler it ships under* was a portability claim wearing a wasm hat; what it means now is one toolchain and the claim is smaller, not gone |

**Only the owner may take the second outcome**, because it is scope given up; the first and third are
rewordings and are the architect's. **The count is the acceptance**: 31 lines in, and every one of them
resolved into one of the three with its outcome visible in a diff.

**And a query that must not become a test.** *No unticked line names wasm or a browser* is a grep that
would pass the moment a word changed, and the defect is a requirement for an absent platform rather than a
string. The instrument here is one pass by someone reading the lines, and then the number is zero and
stays zero because the constraints are stated in the file everyone reads first.

**Done when** every one of the thirty-one is resolved into one of the three outcomes, the scope-losing
outcome is taken only where the owner has taken it, and `board:0058` in particular carries no requirement
for a client this engine does not have.

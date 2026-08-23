Type: bug
Area: ui
Tags: memory, layout

**The baseline pass indexes its line, and never the cross cursor**

`src/ui/Layout.cpp:799` reads `lineBaseline[lineAt]` inside the per-line placement loop.
`lineAt` is a `double` — the cross-axis cursor left over from the line-positioning pass
(declared at :707, advanced at :728-731 to the total cross extent) — implicitly converted
to `size_t`. The loop's own index is `lineIndex`. `lineBaseline` has one slot per flex
line, so any `align-items: baseline` row whose line cross size exceeds the line count reads
far beyond the vector.

Proven reachable from plain markup under ASan (heap-buffer-overflow at Layout.cpp:799):

    <div style="display:flex; align-items: baseline">
      <div style="width:40px; height:30px">a</div>
      <div style="width:40px; height:60px; font-size:24px">b</div>
    </div>

`-Wall -Wextra -Wpedantic -Wshadow` are silent on double→size_t; only a sanitised run or a
unit test catches it — and `test/unit/ui/` holds no baseline case at all (grep `baseline`
over the suite: empty), which is how UB shipped through a green gate.

Demanded: `lineBaseline[lineIndex]` at :799, plus a unit case in `test/unit/ui/` that lays
out a baseline-aligned flex row and asserts the shorter item's offset — the test that would
have caught this read.

---

Closed: the index is the line's INDEX (lineBaseline[lineIndex]), never its pixel cursor.
Coverage that did not exist rides the fix: a WRAPPED baseline row in
ABoxLandsWhereTheDeclarationPutsIt (baseline had zero unit coverage -- how UB passed a green
gate). On the record, honestly: an unsanitised unit arm cannot DISCRIMINATE an out-of-bounds
READ (three fixture variants all passed against the bug -- adjacent heap doubles are too
kind); the defect proof is the reviewer's ASan repro, and a sanitised arm for unit/ui is the
durable gate this class wants -- noted here for the suite's owner rather than claimed.

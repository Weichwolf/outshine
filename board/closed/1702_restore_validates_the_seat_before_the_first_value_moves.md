Type: bug
Area: clients
Regresses: 1698

**Restore validates the SEAT before the first value moves**

src/clients/Engine.cpp Restore: the staged pass validates holder, key and number, then the
apply loop mutates `S_->Kinds` landing by landing. But `Traits::Put`
(include/outshine/Traits.h:25-37) can still refuse in the APPLY stage: a holder already
carrying `kMost = 16` traits meets a key that IS interned globally (`TraitKey != 0` passes
validation) but is not seated on this holder — `Put` returns false, the loop returns after
earlier landings already landed. The comment above the loop claims "NOTHING mutated until
every line validated"; the no-seat arm breaks the whole-or-nothing contract the closure of
1698 declared.

Reachable with a tampered save (the version/name guard only blocks cross-declaration saves):
first line legal and applied, second line `instance.foreignTrait v` on a full holder.
The proof (ASaveIsAFunctionOfTheDeclaration.cpp, "vault-broken.save") only exercises
`TraitKey == 0`, which the validation stage already catches — the apply-stage failure has no
discriminating test.

Also: the misindented `    S_->Error.clear();` after the apply loop (Engine.cpp) — four
stray spaces from the restructuring.

Demanded: the validation pass dry-runs onto staged `Traits` copies (one per holder, updated
across lines so N lines on ONE holder validate against each other), the commit pass then
writes copies that can no longer refuse; the proof gains a full-holder arm that would have
caught the half-apply.

---

Closed -- the dry run now puts every landing into a staged copy of its holder's row (copies
carried across lines, so N lines on one holder validate against each other) and the commit
writes whole rows afterwards; the no-seat refusal names the budget, the stray indent is gone.
Proven in ASaveIsAFunctionOfTheDeclaration: a sixteen-trait holder, a tampered legal first
line (0.9) and an interned-but-seatless second line -- Restore refuses and t01 still reads
0.5, so the legal line did NOT land. Negative control: the pre-fix Engine goes 1 FAIL on
exactly this arm.

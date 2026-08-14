Type: bug
Area: render
Tags: instrument

**A case no instrument decided prints the same green as a case that passed**

A `REFUSED` line is an instrument declining a domain it has not got, and it does not fail a case. That is
correct and it stays: twenty currently-green cases now print a surface-identity refusal because their
index pass carries one distinct value, and reddening them would fail a case for the shape of its asset.
**The defect is narrower and it is real: nothing distinguishes *every instrument declined* from *every
instrument passed*.** Both print green, and one of them is a case with no verdict at all — the hollow
green this suite exists to make impossible, and the amber state `CLAUDE.md` says there is none of.

**The repair is derived and costs one count.** A case where **no** instrument returned a verdict is red,
and the sentence says so. Nothing needs declaring, nothing needs maintaining: the runner already knows
which instruments spoke, because it already prints the refusals.

**And a per-instrument refusal count is published per case**, so an instrument that silently stops
adjudicating a *subset* it used to cover is a number that moved rather than a line nobody read.

## The larger proposal, refused with its reason, so it is not re-proposed

The proposal on the table was that **a case declares which instruments apply to it**, making a refusal
either expected-and-silent or a failure. It is refused, and the reason is not cost:

- **It creates a second answer to a question the tree already answers.** Whether the surface-identity
  instrument can adjudicate is derivable — *does the index pass carry more than one distinct value over
  the covered region* — and `board:1138` derives it today for **18 of 35 cases**. A declaration beside a
  derivation is two sources of truth, and the first thing that happens is they disagree.
- **The disagreement has no principled winner.** If a case declares *surface identity applies* and the
  pass is vacuous, either the declaration is overruled — and then it decided nothing — or it forces an
  adjudication the data cannot support, which is worse than a refusal.
- **It buys one thing the derived rule does not**: catching a case that *used* to be adjudicated and
  silently stopped. **The published per-instrument count catches that too**, for one number instead of a
  per-case form that has to be filled in and kept true through every corpus change.

**A form that has to be maintained beside a fact that can be derived is the shape this board removed from
its own headers**, and the same argument applies here.

**Done when** a case that no instrument decided fails and says so, and each case publishes how many pixels
each instrument adjudicated and how many it declined.

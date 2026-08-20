Type: bug
Area: harness
Tags: instrument bug

**The runner runs every suite it was named**

`test/run.sh` accepted any number of suite arguments and ran **only the last one**, silently. The
parser assigned rather than accumulated:

    *) SUITE=${1%/}; shift; continue ;;

So `./test/run.sh unit/physics unit/pilot` ran two tests, not seven, and printed a trailer that was
true about what it ran and said nothing about what it had been asked to run.

**This is an instrument defect of the worst kind: it narrows a measurement without narrowing the
report.** It cost a false reading in the round that found it -- `render/outshine/client unit/scenario`
printed `6 tests: 6 PASS`, which is `unit/scenario` alone, and was read as both suites passing.

Now: the arguments accumulate, every named suite is selected, the union is deduplicated, and the
header prints all of them. Naming a single case alongside a second suite is a refusal rather than a
guess, because a case is narrowed under exactly one suite.

Proven by running `unit/physics unit/pilot` in both orders and reading 7 tests, and by the two
refusals: a case beside a second suite, and a name that is under no declared suite.

## Comments

**The tell was the same one as always: a trailer that agrees with itself.** `2 tests: 2 PASS` is
internally consistent and says nothing about the five that were asked for and never built. The only
defence is to know what the count SHOULD be before reading it -- which is why a count is quoted with
its population everywhere else in this tree, and why the runner now prints the suites it selected.

Related to `board:1514`: both are the runner reporting confidently about something it did not do.

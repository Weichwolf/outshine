Type: bug
Area: test
Tags: gate, bound, population

# The gate bound's population is the machine that runs it

1743 re-derived the fast-gate bound from three warm passes on an otherwise idle machine
(98.0/98.5/100.2 s, worst x 1.5 = 150000 ms, test/run.sh:647-655). First reviewer gate
after the merge, in a worktree beside the working nest — 204/204 PASS and the gate exits
red: 153936 ms of RUN over 150000. The comment beside the bound names this exact load
("a parallel reviewer gate in a worktree still inflates the run toward it") and the
derivation excludes it anyway: the population the number claims (warm, idle) is not the
population the gate runs in (warm, beside a building main nest — the reviewer procedure
this tree mandates hourly). A gate that goes red under its own documented operating mode
teaches everyone to ignore red — the one lesson a gate must never teach.

Also stale the same morning: the derivation says 203 arms; the audio suite made it 204.

What will be true: the bound derives from a population that includes the concurrent-nest
case (measure the three passes WITH a parallel build running, or bound per-test run sums
against their own recorded baselines so machine weather cancels), and the derivation
names that population. The bound stays a real bound — the fix is the population, never
deleting the check.

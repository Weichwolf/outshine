Type: bug
Area: harness
Tags: instrument

**A binary is held to every source compiled into it, and not only to the ones its dependency file
happens to name**

`-MMD` with several inputs and one `-o` writes **one** dependency file and fills it from the **last**
translation unit. Every layer that carries extra sources therefore had a freshness check that could
not see the test's own source, and a run over it reported a verdict from a binary compiled before the
edit.

## What was measured

[MEASURED] `viewer`'s dependency file lists `test/harness/shared/render/Parity.cpp` and its headers.
It does **not** list `test/viewer/EveryCaseTheTreeDeclaresConfigures.cpp` — the file the test is.
`touch`ing that source and running the layer left the binary at its previous timestamp and printed a
`PASS` from it; deleting the binary by hand and running again produced a different verdict from the
same tree.

**The population is every layer with extra sources**, and that is `harness/khronos/glTF`,
`harness/outshine/render` and `viewer` — both render corpora and the browser. It is narrower than it
looks only because the two corpus runners are seven lines each and rarely change; the defect is the
same size in all three.

## Why it is board:1403's failure and not a new one

`board:1403` found that `-MMD` records what a *translation unit* read and never the library objects a
binary *links*, and added the object loop that fixed it. **The comment it left says the quiet part:**
*the one failure a freshness check exists to make impossible.* This is the same sentence about the
same mechanism one input further along — the compiler's own list is not the list of what went in.

## What is true now

- [x] `Fresh` takes **every source of the link** as arguments and holds the binary to each with `-nt`,
  beside the `.d` prerequisites and the linked objects it already held
- [x] All three arms pass the same list — plain, sanitised and validated — because a stale sanitised
  binary is a phantom in exactly the same way
- [x] [MEASURED] after the repair, touching the test's own source rebuilds: 08:47 to 08:49 on a run
  that previously left the timestamp alone

## Comments

Found by accident and worth saying how, because the accident is repeatable: a source edit produced no
change in behaviour, and the reflex *the edit must be wrong* was wrong. **The binary's own timestamp
was older than the source's**, which is one `ls` and is the check that should come before re-reading a
diff.

Type: bug
State: active
Area: test
Tags: state, gate, measure

# a proof line names its cases AND may carry its reading, and the walker tells them apart

**Benchmark** — neither Unreal nor RAGE faces this: `board/` and `STATE.md` are outshine's own
apparatus and nothing in either engine corresponds to them. **The choice is mine and the reason
is CLAUDE.md's own**: "a number without its derivation is unfinished", so a proof line that names
a case and then says what the case READ is the format the page asks for. The walker has to fit
the format, not the other way round.

`test/run.sh:1080` takes everything after `proof:` as a comma-separated list of paths and requires
every element to resolve to a file, a directory or an audit flag. board:1985's tick reads

    proof: outshine/geo/ScoreWhenAWaitForATileEnds reads `fetches that ran on a COMPUTE worker:
    0 and 0`; the offline driver finishes 8 frames in seconds ...

so the first element is `outshine/geo/ScoreWhenAWaitForATileEnds reads \`fetches that ran on a
COMPUTE worker:` -- not a file. The tick is reported as unproven and `gpu-driven` is counted one
short, while the case is in the tree and green.

**This is CLAUDE.md's own `a measure that cannot see`.** The count did not miss a file; it read
one that was there and could not recognise it, and then published the failure as the ITEM's.

The rule the walker needs is in the tree already: `test/` has six suite roots -- `geographiclib`
`harness` `khronos` `outshine` `test262` `wpt` -- and an audit flag starts `--audit`. A token is a
PROOF CLAIM if it starts with one of those and prose otherwise. `33/33` is prose; `outshine/door`
is a claim. Every claim must resolve; a tick with no claim at all names no proof, which is the
report this walker exists to make.

- [ ] a ticked predicate whose proof line carries prose is counted HELD when its named cases stand
- [ ] a ticked predicate that names no case at all is still reported

**The measurement that would show I am wrong:** `make` and read STATE.md. `gpu-driven` must gain
the tick and the `Ticked, but the named proof is not in this tree` list must lose its only entry.
Negative control: point that same tick at a case that does not exist and the entry must come back.

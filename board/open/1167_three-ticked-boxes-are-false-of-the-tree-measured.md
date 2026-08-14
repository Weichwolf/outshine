Type: bug
Area: harness
Tags: instrument, scope

**Three ticked boxes are false of the tree, measured on the first six items audited**

`board:1160` measured that 143 ticked boxes cite nothing and named the two possibilities: **work that
happened and never left a marker**, or **a tick that was never true**. The calibration pass over
`Area: scenario` (5 items) and `Area: data` (1) settles that both exist, and it found the second kind at
**three of ten ticks re-measured**.

| tick | measured | verdict |
|---|---|---|
| `0057` *Frame telemetry as a time series with scenario, scene, **wasm hash and browser version** on every line* | `wasm` and `browser` appear in **no** telemetry source | **false** — it names fields of a deleted platform |
| `0057` *Per-pass GPU timestamp pairs — and the published statement that they must not be summed* | nothing matching `gputimer\|timestamp` in `src/render/` | **false, and it is a REGRESSION** |
| `0049` *Request-level timeout; no timeout on the load as a whole* | `timeout` appears nowhere in `src/` but a Python asset script | **false** — the engine has no request timeout |

**The second one is the sharp case and it is a contradiction inside the board, not merely staleness.**
`board:0058` records that `GpuTimer` *was deleted for having no consumer — correctly, by § I.23's
zero-consumer rule — but the consumer was what was missing, not the instrument*, and carries **the frame
clock returns** as an **unticked** box. So one item ticks what another item is open to restore. **Two ids
answer one question and disagree**, which is the ordering defect this board exists to remove, one level
down from where it was expected.

**And the same pass found the opposite defect, which needs a different repair.** `0054`'s first two ticks
were re-measured and **hold exactly** — `ChunkVtx` asserts `offsetof(pos)==0`, `offsetof(uv)==12`,
`offsetof(norm)==20` and its own stride, and `WaterField` declares the `pos3+nrm3` surface. **The work is
done and no `board:0054` marker exists in either file.** That repair is **one line in the source**, not an
untick, and it must never be confused with the three above.

**A third shape appeared too**: `0071 Settings in two tiers` carries **19 unticked boxes and zero ticks**
while `src/assets/world/vegetation.json` and `ground-materials.json` exist and are read by
`world/ClassField.h` and `world/GroundMaterials.cpp`. **An absence that closed silently** is as
untrustworthy as a tick that went false, and it is invisible to any query that looks only at ticks.

**Unticking is scope moving and is not the architect's**, which is why this is filed rather than done.
Each of the three needs its own decision: whether the requirement stands and the tree regressed — then a
new item citing it with `Regresses:` — or whether the requirement went with the platform and the line is
struck, which is `board:1163`'s ruling applied to a ticked line instead of an open one.

**Done when** the three above are each resolved into *regressed and refiled* or *struck with the
platform*, `0054`'s two proven boxes carry markers in the source that satisfies them, and the audit's
verdict vocabulary — **true · true-but-uncited · stale · refuted · silently-closed · unverified** — is
what every later area is reported in.

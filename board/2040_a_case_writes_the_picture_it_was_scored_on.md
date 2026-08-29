Type: bug
State: open
Area: test
Tags: measured

# A case writes the picture it was scored on

**Benchmark** — Unreal: an automation screenshot test writes the EXACT buffer it compares, and the
diff image beside it is generated from that same pair. RAGE: a render regression dumps the frame
that failed. **They agree**, so the matter is closed: the artefact a human looks at is the artefact
the machine judged.

MEASURED, on `khronos/glTF/AlphaBlendModeTest` with the harness the tree carries today:

    picture_p99_delta_code = 1 code, 31 checks, 0 failures       -- the case PASSES
    1-outshine.png beside it                                     -- pure magenta

NAMED EXACTLY, because "a different buffer" is not a finding until it has a name:

    the metric   `ComparePicture(scored, oracle, ...)` where `scored` is derived from
                 `picture.Linear` -- the scene-referred float readback
    the PNG      written from `picture.Rgba` -- the DISPLAYED byte readback

Two readbacks, two buffers, one filename. On the path that passes, `Rgba` carries the clear colour
and nothing else, and the case still scores 1 code because its `Linear` is right. So a reader who follows CLAUDE.md's own rule -- *if it
draws, LOOK AT IT before believing any number* -- looks at a picture that had no part in the
verdict, and reads it with confidence.

**THIS COST A SESSION.** board:2038's colour investigation made five visual readings -- "too dark",
"the framing matches now", "the texture is there", "pure magenta means nothing was drawn" -- and
every one was of this artefact. The numbers were sound because they came from the metric; the
pictures were a second opinion nobody had checked. The instruction to look is worth having ONLY
when what is shown is what was judged.

- [ ] `1-outshine.png` is the linear readback the metric scored, tonemapped once, and nothing else
- [ ] `0-reference.png` is the oracle it was scored against, through the same transfer
- [ ] A case that scores no picture writes none, rather than writing a picture of nothing
      proof: the negative control is a case whose readback is deliberately zeroed -- the written
      picture goes black and the metric goes red TOGETHER, which is what "the same artefact" means

Type: bug
State: active
Area: test
Tags: corpus, gate, measured

# Whether a case is a sequence is DECLARED, and the reader stops counting frames to guess it

`test/harness/shared/corpus/prep/manifest.py:95` decides a product's name from the DECLARATION:

    if self.scene.animation is None: return [None]
    return list(range(int(self.scene.animation["frames"]["value"])))

So a case with `scene.animation` gets `oracle.f0000.exr` whether its grid holds one frame or
forty. `Parity.cpp:126` asked a different question -- `Frames > 1` -- which is a second spelling
of the same fact and disagrees with the first on exactly one input: a declared animation sampled
at a single frame. The reader then asked for `oracle.exr`, the file on disk was
`oracle.f0000.exr`, and the case reported UNPREPARED.

Measured: every generator case with `frames: 1` was exactly the set that stood unprepared --
`Animation_NodeMisc_03` and `Animation_NodeMisc_05`, six arms between them, in every gate run.

`Subject::Sequenced` is set where the manifest's `scene.animation` block is read, and
`ProductFrame` asks it instead of counting.

## What will be true

- [x] The reader's product names agree with the preparer's because both read the same declared
      fact, not two derivations of it.
- [x] Proving test: `harness/khronos/generator`, 102 of 102. Negative control: `ProductFrame`
      back on `Animated()`, and the same suite reports 96 PASS with 6 UNPREPARED.

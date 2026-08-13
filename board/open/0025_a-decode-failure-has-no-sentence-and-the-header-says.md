Type: bug
Area: gltf
Tags: khronos, instrument

**A decode failure has no sentence, and the header says it does**

`src/gltf/Document.h:6` — *"`Error()` is the sentence"*. `Error_` is written only by `Refuse`, which only
`Read` and its helpers reach, so `ReadElements`, `ReadIndices`, `WorldTransform` and `ViewTransform` all
return a bare `false` and leave `Error()` **empty** — measured on a successful read followed by a failed
`ReadElements` (`AFileThatCannotMeanAnythingIsRefusedByName:139-144` exercises exactly that path and
asserts only the `false`). `Camera::Projection` has no channel at all.

**This is a diagnostic defect and not a data one, and the distinction is why it ranks last here:**
`[[nodiscard]]` means no caller can spend the failure without deciding, and a refused `ReadElements`
leaves `out` empty. The caller knows *that* it failed and cannot say *why*, in a reader whose whole
stated contract is naming what was missing.

**Right:** the decode path refuses through the same channel, which means it must be able to write —
`accessor 4 spans [0, 96) of a bufferView of 4 bytes`, `accessor 4 is a VEC3 of floats and an index
accessor must be a scalar unsigned integer`. **Fixed when** the overrun subject in
`AFileThatCannotMeanAnythingIsRefusedByName` asserts a wording, like the other 13 do.

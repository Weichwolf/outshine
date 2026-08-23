Type: bug
Area: gltf
Tags: boundary, emit

**The GLB writer refuses what does not fit its container**

- src/gltf/Emit.cpp:296-300 — `Container()` casts `12 + 8 + text.size() + 8 + Binary_.size()`
  and the two chunk lengths to `uint32_t` with no check. A subject past 4 GiB of vertex data
  emits a silently corrupt GLB whose header lies about its length — the reader on the other
  side then refuses a file the writer claimed was well-formed, or worse walks wrong chunk
  bounds. The format's 32-bit container is a hard cap; the writer must refuse at that cap
  with the byte count in the refusal, in `Admissible()` next to the other refusals.

Demanded: the total and each chunk are checked against `uint32_t` max before the cast; the
unit twin proves the refusal text without allocating 4 GiB (expose the bound or inject the
size).

---

Closed -- GlbFits is the exposed bound (padded chunks + the three headers against the
uint32 ceiling, in the no-underflow form), Run refuses past it naming both byte counts
before Container's casts run. Proven in AGlbCarriesWhatItDeclares without a 4 GiB fixture:
5 GiB of geometry refuses, the rim under the ceiling accounts for the headers, two fitting
halves refuse together.

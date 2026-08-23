Type: bug
Area: gltf
Tags: boundary, ub, overflow

**Every JSON-sourced size lands in a checked integer**

The reader casts hostile doubles straight into `size_t` and does unchecked arithmetic on
them:

- src/gltf/Document.cpp:400, 463-465, 485-486, 513-523 — `static_cast<size_t>(x.Num(0.0))`
  on byteLength, byteOffset, byteStride, count and the sparse fields. A file declaring
  `"count": 1e300` makes the cast itself UB (value unrepresentable in size_t); a negative
  or fractional number is silently truncated instead of refused.
- src/gltf/Document.cpp:1448-1449 — `last = ByteOffset + (Count-1)*stride + element` can
  wrap: with Count near 2^64/stride the product overflows, `last` comes out small, the
  bounds check passes. The OOB read is masked only because line 1453's
  `out.assign(Count*components)` throws bad_alloc first — an UNCAUGHT abort from a crafted
  file, where the house rule demands a refusal that names the number.
- src/gltf/Document.cpp:1493-1494 — the sparse bounds use the same wrappable
  `offset + count * bytes` form.
- src/gltf/Document.cpp:465 with 1421-1430 — byteStride is accepted at any value ≥ element
  size; the spec bounds it to [4, 252], a multiple of 4, and only on vertex views (an index
  view with a stride is an invalid file this reader honours).

Demanded: one helper that turns a Json number into a size or refuses (integer-valued,
non-negative, ≤ a stated cap such as the buffer's byte count), used for every size field;
bound checks in the `a > limit || b > limit - a` form that cannot wrap; byteStride held to
the spec's window. The unit twin feeds the 1e300 count, the wrapping offset and the stride-7
view, and reads the refusal text, not a crash.

---

Closed -- DeclaredSize is the one helper (integer-valued, non-negative, under the GLB
container's own uint32 ceiling [SET]) and every JSON-sourced size lands through it: buffer
byteLength, view buffer/offset/length/stride, accessor offset/count, all three sparse
fields; byteStride holds the spec's multiple-of-4 [4,252] window when declared; the
ReadElements and ApplySparse bounds take the a > limit || b > (limit-a)/stride form that
cannot wrap; the viewless zero-fill is bounded by the same ceiling. Proven in
AFileThatCannotMeanAnythingIsRefusedByName: count 1e300, byteOffset -5 and byteStride 7 all
refuse by name (negative control: the raw-cast reader reverted goes red). The uncaught
bad_alloc route died with the cap.

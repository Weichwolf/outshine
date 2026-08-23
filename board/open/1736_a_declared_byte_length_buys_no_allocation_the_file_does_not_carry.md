Type: bug
Area: gltf
Tags: hostile-input, memory, spec

# A declared byteLength buys no allocation the file does not carry

Two residues of the 1733 repair (commit 7ba5aae1), both in `src/gltf/Document.cpp`.

## 1. The DoS inverted: the declaration now commands the allocation

`src/gltf/Document.cpp:437` — `bytes.resize(declared + 1)` allocates BEFORE any byte is
read. `declared` is gated only by `kMostDeclaredBytes = 4294967295.0` (Document.cpp:403),
so a 60-byte hostile file declaring `"byteLength": 4294967295` next to an empty `.bin`
forces a 4 GiB zeroed allocation on the 8 GB device — per buffer entry, so two such
buffers are an OOM before the refusal at `bytes.size() < declared` ever fires. The old
slurp was bounded by the FILE (a huge file was the bomb); the fix made the DECLARATION the
bomb. The bound must be the minimum of both truths: measure the file first
(`seekg(0, end)` / `tellg`, or `std::filesystem::file_size`) and allocate
`min(declared + 1, measured)` — the `< declared` refusal then fires on the measured count
without a byte of speculative zero-fill. The `gcount()` use itself is correct.

## 2. The viewless fill contradicts the spec and its bound mixes units

`src/gltf/Document.cpp:1558` — `if (!accessor.HasSparse) { return false; }` refuses a
viewless accessor WITHOUT sparse. glTF 2.0 §3.6.2.3: when `bufferView` is undefined the
data MUST be initialized with zeros — sparse or an extension MAY override, neither is
required. The pre-1382 audit (board:1397) claims enumerated conformance; this line is a
new deviation it does not record.

`src/gltf/Document.cpp:1560-1561` — `accessor.Count > carriedBytes` compares an ELEMENT
count against a BYTE sum: no unit, no derivation. The amplification it actually permits is
`carriedBytes * 16 components * sizeof(double)` = 128x, a factor stated nowhere. A legal
file whose accessors are predominantly viewless-sparse (count N, tiny override — the
spec's own morph-target shape) is refused as soon as N exceeds the file's total bytes.
Demand: bound the OUTPUT (`Count * components * sizeof(double)`) by a named multiple of
carried bytes, derivation beside the constant, and a twin case proving the legal
large-count viewless-sparse accessor decodes.

Proving tests: a twin case in `test/unit/gltf/AFileThatCannotMeanAnythingIsRefusedByName.cpp`
that asserts peak allocation stays under the measured file size (Heap tag), and a case in
`ASparseAccessorResolves.cpp` for the plain viewless zero accessor.

Type: bug
Area: core
Tags: allocation, load-path, unbounded-breadth

# The unread walk costs one string per node to name one attribute

`Xml::FirstUnread` builds an owning path string for EVERY node in the document, and holds
all of a level's siblings' strings live at once, in order to report a single attribute.

## Evidence

```cpp
Xml::Unread Xml::FirstUnread() const {
  std::vector<std::pair<uint32_t, std::string>> walk;                       // :374
  if (Root_ != 0) { walk.push_back({Root_, Span(...)}); }
  while (!walk.empty()) {
    const auto [at, path] = walk.back();                                    // :377 — a COPY
    walk.pop_back();
    ...
    for (uint32_t child = node.FirstChild; child != 0; child = Nodes_[child].NextSibling) {
      walk.push_back({child, path + "/" + Span(under.NameOff, under.NameLen)});  // :389
    }
  }
```
— src/core/Xml.cpp:373-393

Three costs, none of them necessary:

1. **One `std::string` per node**, built by concatenation at :389, for a document bounded at
   `kXmlMaxNodes` = 65536 (src/core/Xml.h:16). The stack is not depth-bounded like
   `Grammatical`'s recursion — it is BREADTH-bounded, so a flat scenario with 20 000 rows
   holds 20 000 path strings simultaneously.
2. **A copy of the path on every pop**: `const auto [at, path] = walk.back()` at :377
   binds a structured binding to a copy, then `pop_back()` destroys the original. `auto &&`
   plus a move, or an index, avoids it.
3. **All of it to return at most one `Unread`.** The answer is available without any string:
   `Asked_` is a flat `std::vector<uint8_t>` parallel to `Attributes_`, so the first unread
   attribute is `std::ranges::find(Asked_, 0)` — O(A), zero allocations. Only THEN is a path
   needed, for exactly one node.

The path for that one node cannot be walked upward today (`Node` carries `FirstChild` and
`NextSibling`, no parent — src/core/Xml.h:25), which is the real design question: either a
`Parent` field (4 bytes; `Node` is 32 bytes today and would go to 36 — measure whether 40
after padding is worth it) or a single depth-bounded descent that carries only
`kXmlMaxDepth` = 64 name spans on its stack.

## It is the second walk of the same shape

`Grammatical` (src/scenario/ScenarioRead.cpp:121-148) already recurses the whole document
building `path + "/" + name` per node at :146, with `Known(const std::string &path)` at :114
comparing that string against 71 grammar rows. `FirstUnread` then walks it AGAIN, building
the same strings a second time. The load path constructs the document's full path set twice
and throws both away.

## What will be true

1. `FirstUnread` allocates O(1) strings — the scan is over `Asked_`, the path is built for
   the one node that answers.
2. No structured binding copies a string out of a container it is about to pop.
3. The two path-building walks become one, or the second stops building paths at all.
4. Proof: a unit case over a document at `kXmlMaxNodes` breadth asserts the allocation count
   of `FirstUnread` is bounded by a small constant (the tree already counts allocations —
   `test/unit/scenario/ALayerIsReadOnceAndKeepsWhatItOmits.cpp` does exactly this, "818
   allocations become 10"). Negative control: restore the per-node concatenation and the
   count explodes with the node count.

---

## Partially repaid, still open (review 2026-08-24)

b4e9ce04 added point 3's scan and nothing else:

```cpp
Xml::Unread Xml::FirstUnread() const {
  if (std::ranges::find(Asked_, 0) == Asked_.end()) { return Unread{}; }
```
— src/core/Xml.cpp:376

Correct and worth having: `Asked_` is exactly one byte per real attribute
(`Asked_.assign(Attributes_.size(), 0)` at :371, and unlike `Nodes_` the attribute table has
no sentinel at index 0), so the short-circuit is live rather than dead, and libc++ lowers
`ranges::find` over a byte range to a `memchr`. The NULL case is now O(A) and allocation-free.

But the item is about the REPORTING case -- "one string per node **to name one attribute**" --
and that path is byte-for-byte unchanged:

- `std::vector<std::pair<uint32_t, std::string>> walk;` still holds a level's siblings' path
  strings live at once (src/core/Xml.cpp:378);
- `const auto [at, path] = walk.back();` still binds a structured binding to a COPY before
  `pop_back()` destroys the original (:380-381) -- point 2, untouched;
- `walk.push_back({child, path + "/" + …})` still concatenates per node (:392) -- point 1,
  untouched;
- `Grammatical` still builds the same path set a second time -- point 3, untouched;
- there is still no allocation-count case in `test/unit/core/` -- point 4, untouched, and the
  short-circuit that WAS added has no test either. A document with one unasked attribute and
  one with none are indistinguishable to the mirror today.

A scenario refusal is a load-path event, not a frame-path one, so the priority is right; the
closure is not. Four points were written down and one landed.

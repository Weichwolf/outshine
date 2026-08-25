Type: issue
State: open
Area: src
Tags: cpp23, layout, simd, optimisation

# A field is an mdspan and not an index calculation

`CLAUDE.md` names the form in two places — "`std::mdspan` for field and instance views" in
the language rules, and "demand its tools where they are the better form -- `std::mdspan`
over hand-rolled index maths on fields, tiles and instance streams" in the reviewer's
mechanical bar. The tree uses it zero times:

```
$ grep -rn 'mdspan' src/ include/ | wc -l
0
```

against 8 uses of `std::expected` and a completed `std::span`/`string_view` sweep. Of the
three C++23 forms the map demands, two are being paid and one has never been started.

## Where the demand actually bites

**A rectangle behind a pointer chase.** The one home of every balanced number in a game
stores its rows as a vector of vectors and its column types as a bitset proxy:

```cpp
struct Stood {
  std::vector<std::string> Columns;
  std::vector<bool> Numeric;
  std::vector<std::vector<Cell>> Rows;          // src/scenario/Tables.h:39
  std::unordered_map<std::string, size_t> ByKey;
};
```

A table is `rows × columns` — a 2-D field by definition, bounded at `kMostRows` = 4096
(src/scenario/Tables.h:17). Every cell read is a pointer load then an index; no two cells of
a column are adjacent; `std::vector<bool>` cannot be loaded at all. board:1489's own note
says scripts will "read damage from per tick", so this is the frame path. One flat
`std::vector<Cell>` viewed through `std::mdspan<Cell, dextents<size_t,2>>` puts a row in one
cache line's reach and makes a column a strided span.

**Hand-rolled index maths on the terrain field**, which is the literal wording of the rule:

```cpp
float AtM(uint32_t row, uint32_t col) const { return HeightsM_[(size_t)row * Cols_ + col]; }
void  SetM(uint32_t row, uint32_t col, float m) { HeightsM_[(size_t)row * Cols_ + col] = m; }
```
— src/ground/tiles/TerrainGrid.h:27-28

The storage is already flat and correct; what is missing is the VIEW, so that a consumer can
take a sub-extent (a tile's border, a strip for a SIMD pass) without re-deriving the stride
by hand. `Bilinear(HeightsM_.data(), Cols_, Rows_, gx, gy)` (:33) is the same stride passed
as three loose arguments, which is what `mdspan` exists to bundle.

**Others of the same shape**, to be judged in the same sitting, not necessarily converted:
`src/scenario/Triggers.h:57` (`vector<vector<Standing>>`, one list per door — a ragged field,
so `mdspan` fits only with a fixed per-door stride), `src/gltf/Tangents.cpp:114`
(`run[vertex * width + k]`), `src/gltf/Subject.cpp:1015` (`mine[vertexBase * stride + at]`).

## What will be true

1. The tree has a decision, written here, on where `mdspan` is the form and where a flat
   span plus a named stride is honest — a rule the map states and the code never uses is a
   lying map, and either the code or the map moves.
2. `TerrainField` publishes an `mdspan` view over its heights, and `Bilinear` takes that
   view instead of `(data, cols, rows)`.
3. `TableBook::Stood` holds one flat cell array with a 2-D view, and `Numeric` is a
   `std::vector<uint8_t>` or a bitset that can be read — not `std::vector<bool>`.
4. The proof is a layout test, not a behaviour test: `static_assert` on the extents type
   beside each struct, and a unit case that a column's cells are `stride`-apart in memory.

## Comments

- 2026-08-24, review of 5fb183f0 -- **half of point 2 is delivered, and the half that was the
  point is not.** `TerrainField` now publishes the view:

  ```cpp
  using Postings = std::mdspan<float, std::dextents<size_t, 2>>;
  using ConstPostings = std::mdspan<const float, std::dextents<size_t, 2>>;
  [[nodiscard]] ConstPostings Field() const { return ConstPostings(HeightsM_.data(), Rows_, Cols_); }
  [[nodiscard]] float AtM(uint32_t row, uint32_t col) const { return Field()[row, col]; }
  ```
  — src/ground/tiles/TerrainGrid.h:28-35

  and the very next method still hands the stride around as three loose arguments, which is
  the sentence this item was filed on:

  ```cpp
  return Bilinear(HeightsM_.data(), Cols_, Rows_, gx, gy);
  ```
  — src/ground/tiles/TerrainGrid.h:40

  So the accessor got the view and the only CONSUMER of the layout did not. Point 2 reads
  "and `Bilinear` takes that view instead of `(data, cols, rows)`" and is unmet.

- **Point 4 is untouched.** No `static_assert` stands beside `TerrainField` on the extents
  type or on `sizeof(float)`-tight packing, and `test/unit/ground/tiles/` holds two cases
  (`AStitchedEdgePairsPostingsOfTheSamePlace`, `TheTileCacheEvictsTheLeastRecentlyUsed`),
  neither of which names `Field()`, `Postings` or a stride. A view whose extents nothing
  proves is a typedef.

- **`[[nodiscard]]` was swept over one of three classes in the file it touched.** The same
  header, after the sweep:

  ```cpp
  size_t Bytes() const { return Field_.Bytes(); }                                   // :80
  uint32_t VertexCount() const { return (uint32_t)(PositionsEnuM_.size() / 3); }     // :103
  ```

  `TerrainField` got eight `[[nodiscard]]`s; `TerrainGrid::Bytes` and
  `TerrainMesh::VertexCount` in the same file got none. The house rule is ALWAYS, and a sweep
  that stops at a class boundary inside one header is a sweep that will have to be run again.

- Point 3 (`TableBook::Stood`, src/scenario/Tables.h:39, still
  `std::vector<std::vector<Cell>>` + `std::vector<bool>`) is untouched.

## Comments

- 2026-08-24 -- point 2 is now paid: `Bilinear` takes the view.

```cpp
using Postings = std::mdspan<const float, std::dextents<size_t, 2>>;
static_assert(Postings::rank() == 2,
              "a field is two extents that travel together, never a pointer beside a stride");
[[nodiscard]] inline float Bilinear(Postings field, double gx, double gy);
```

  `TerrainField::AtM/SetM/PostingM` all read through `Field()`; `row * Cols_ + col` is gone
  from the class, and the only caller of `Bilinear` in the tree hands it the view.
- **Proving test**: `test/unit/compile/world/AFieldIsNotAPointerBesideAStride` -- the old
  `Bilinear(data, cols, rows, gx, gy)` spelling does not compile, judged by
  `unit/ground/AGeneratorHasNoSpellingInTheStreamer`.
- **Negative control**: the compile subject IS the control -- it is the defect's own
  spelling, and the suite requires it to be refused for the stated reason.
- `[[nodiscard]]` was pulled over `TerrainField`'s queries in the same pass. Still open in
  this item: the other two classes in that file (`:80 Bytes()`, `:103 VertexCount()`), and
  the extents are not yet `static_assert`ed against the tile edge.
- Gate 234/234.

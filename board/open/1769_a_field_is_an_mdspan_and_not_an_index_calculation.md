Type: issue
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

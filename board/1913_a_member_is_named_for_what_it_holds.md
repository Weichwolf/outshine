Type: issue
State: open
Area: base/spatial
Tags: naming, review

# A member is named for what it holds, because nothing else may say so

`src/base/spatial/Wayfinding.h:66`

    [[nodiscard]] size_t CellsInTheTieIndex() const { return Unindexed_; }

`Unindexed_` once counted the edges the `kMostCellsPerEdge = 64` cap DROPPED from the tie index.
That cap is gone (board:1894) and the member now counts the cells the index HOLDS -- 473 813 of
them on the shipped network. The name says the opposite of the value, and the accessor beside it
says the truth, so a reader of `Wayfinding.cpp:340,347` learns the wrong thing:

    if (Unindexed_ >= kMaxNetworkPoints) { error = "the tie index would hold more than ..."; }
    ++Unindexed_;

Under the no-comments rule the name is the ONLY documentation a member gets, so a stale name is
not cosmetic -- it is the whole of what the tree says about that field, and it is false. This is
the only such mismatch in the file: `WayCount`, `PointCount`, `NodeCount`, `EdgeCount`,
`TiedToEdges`, `CrossingsJoined`, `CrossingsLeftAlone` and `SnapM` all name what they return.

## What will be true

- [ ] The member is `TieCells_` or the accessor is renamed, and the two agree.

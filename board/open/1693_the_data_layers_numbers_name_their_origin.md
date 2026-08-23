Type: bug
Area: data
Tags: numbers

**Every number in the data layer names its origin**

The house rule — every number carries derived · measured · `[SET]` with unit and population
— stops at src/data. Bare: `src/data/TerrariumDem.cpp:24` `TypicalPayloadBytes = 60000`
(measured over which tiles?), `:25` `RetryBudget = 4`; `src/data/VersatilesVector.cpp:23-24`
the same pair; `src/data/ContentStore.cpp:13` `kDefaultCapBytes = 2ull << 30`;
`src/data/WebTileSource.cpp:6` `kDeepestTileZoom = 30` (this one is derivable — `1u << z`
at :17 caps z at 31, and the shift is the reason — but nothing says so). The zoom bounds
15/14 are provider facts and read as such; the rest are decisions or measurements wearing
neither name. Contrast `src/scenario/Tables.h:18`, where the same tree does it right.

Demanded: each named constant carries its marker — `[SET]` where it is a choice,
the measurement population where it was measured, the derivation where it follows.

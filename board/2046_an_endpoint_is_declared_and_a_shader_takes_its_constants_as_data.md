Type: task
State: open
Area: src
Tags: hygiene, config, shaders

# An endpoint is DECLARED, and a shader takes its constants as data

**Benchmark** — Unreal: endpoints live in `.ini` under `Config/`, never in C++, and a shader takes a C++ value through a uniform or a SHADER PERMUTATION rather than by having it printed into its source. RAGE: the same, with its own settings files. **They agree, so the matter is closed** — this item is about doing it, not about deciding it.

## Two findings, both measured, both small

**TWO URLs STAND IN `src/`**, and both are tile endpoints:

    src/world/data/VersatilesVector.cpp:37   https://tiles.versatiles.org/tiles/osm/%d/%u/%u
    src/world/data/TerrariumDem.cpp:40       https://s3.amazonaws.com/elevation-tiles-prod/terrarium/%d/%u/%u.png

A server moving is a RECOMPILE today, and CLAUDE.md already says config is externalised. `SourceDecl`
is the declaration these belong in and it is a plain struct built in code -- no loader, no root, no
refusal path. **The half-step is refused**: moving the string from an `snprintf` into a struct field
reads better and changes nothing, because it is still a recompile. Either `src/assets/world/` gains
a `sources.json` that `RegisterDeclared` reads, with a refusal when it is missing or malformed, or
the URLs stay where they are and this item stays open. A rule half-obeyed is worse than one openly
not yet obeyed.

**NINE GENERATED CONSTANTS ARE PRINTED INTO SHADER SOURCE.** The shaders themselves are assets and
always were -- every stage loads `src/render/shaders/*.msl` through `LoadShaderText`, and there is
no inline MSL anywhere in the tree. What is inline is the PREAMBLE:

    MediumMultiScatterStage.cpp    4    "constant uint kMultiScatterSteps = %uu;\n" ...
    MediumRadianceStage.cpp        3
    MediumTransmittanceStage.cpp   2

A C++ value reaches the shader by being formatted into its text, which means the shader is compiled
per value and the two sides agree only because one printed the other. The two mechanisms that do
this properly are a UNIFORM, which SDL_GPU has, and Metal's FUNCTION CONSTANTS, which SDL_GPU does
not expose -- so the answer here is a uniform, and the question to settle first is whether these
values change often enough to be worth a bind or are fixed enough to belong in the `.msl` file as
literals with their derivation beside them.

## What will be true

- [ ] `grep -rn 'https\?://' src/` comes back empty, and a source's endpoint is read from a declared
      asset with a refusal that names the file when it is absent
- [ ] no `"constant ` appears in `src/`: a value the shader needs arrives as a uniform, or it is a
      literal in the `.msl` with its origin written beside it
- [ ] and the six picture digests do not move, which is what says the change was a MOVE

## What this does NOT cover

**Error text is not config and stays.** 172 refusals are assigned in `src/` and every one of them is
a DEVELOPER DIAGNOSTIC written at the point of failure with the local values interpolated -- "a draw
names surface slot 7 over a table of 4". Unreal externalises user-facing text through `LOCTEXT` and
keeps developer diagnostics as literals; RAGE draws the same line. The distinction is who reads it:
a player or a translator, against the person who broke the build. A string table cannot interpolate
as naturally, and the identifier between a failure and its explanation is exactly the indirection
that lets a message drift from the code it describes. **If outshine ever carries user-facing text,
that goes in a table from the first line** -- it is a different genre and it does not exist yet.

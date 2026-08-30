Type: task
State: open
Area: door
Tags: scenario, serialisation, door

# The scenario IS the public interface written down, and it round-trips

**Benchmark** — Unreal: a level is a `.umap` package, binary, with a TEXT mirror of the same object
graph (`T3D`, and `-textformat` writes whole packages as text for source control); a savegame is a
different CONTAINER over the same `FArchive` property serialisation. RAGE: `.rpf` holds binary
resources built from `#map`/`#typ`, and the text form lives in the toolchain and never ships; the
savegame is its own versioned blob. **They agree on the shape and differ on whether the text form
ships: ONE serialiser, TWO containers.** **Taking Unreal**, because a text form that ships is what
lets a scenario be written by hand, diffed in a commit and read by a stranger -- which is what this
tree's `DECLARED, NOT CODED` invariant is for, and RAGE's answer assumes a toolchain this tree does
not have.

## What is wrong today

The scenario grammar is a hand-kept table (`kGrammar`, `src/scenario/ScenarioRead.cpp`) that names
elements the READER may or may not read, and the two have drifted:

    {"scenario/views/view", "", "id follows person"}   the grammar: no children, three required
    ReadStanding(one.Child("at"), ...)                 the reader: a child `at`, none required

That drift shipped. It was found by trying to WRITE a scenario, not by reading either file, and it
is the second time this tree has paid for a capability no declaration reaches.

**There is no writer.** A format that is only ever read cannot be diffed against what the engine
holds, so a drift like the one above has nothing that can see it.

**The names are their own vocabulary.** `<view follows=>`, `<body assetSpanM=>`, `<drive>` --
none of these is a name the door uses. A client reads `include/`, then has to learn a second
spelling of the same nouns to declare anything.

## What will be true

- [ ] every element and attribute is spelt as the door spells it, and the door's Doxygen is the
      only place a name is defined
- [ ] the grammar is DERIVED from the declaration types rather than kept beside them, so a field
      the reader reads and the grammar forbids cannot exist
- [ ] a scenario is WRITTEN as well as read, and `read -> write -> read` is the identity on every
      case in `test/outshine/places` -- that round trip is the negative control the drift above
      had none of
- [ ] an EMPTY scenario is valid and everything is opted IN: the Cesium Earth, the skybox and the
      OSM features are features a scenario ORDERS, and an engine that was asked for nothing draws
      nothing and says so
- [ ] the binary form and the XML form pass through ONE serialiser over the declaration types --
      binary for speed, XML for a human, and neither knowing anything the other does not
- [ ] a written scenario carries assets BY REFERENCE (uri and digest) and never embeds one

## Why not a savegame that is the same file

A savegame has to carry RUNTIME state -- where a body stands now, what a stream has ingested --
and a scenario carries a DECLARATION. Unreal keeps them apart for that reason and so should this:
the same serialiser over the same types, two containers, and the savegame is the declaration plus
a delta. Making them one file makes every scenario carry fields no author wrote.

## What this does NOT cover

The glTF side. glTF is IMPORT ONLY here and stays that way; an asset is referenced, never
rewritten, and this item does not make the engine an exporter of anybody else's format.

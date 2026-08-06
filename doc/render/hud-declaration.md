# The HUD declaration — a title's glass as a table

> Owner, 2026-08-05: *„daraus folgt auch, dass die `mods/` komplett deklarativ sind."*
> [`../mods.md`](../mods.md) §2 listed *„all four: a per-title HUD"* as an UNDECLARABLE. This file is
> the surface it was missing. The backend that draws the result is [`hud.md`](hud.md); what a given
> title's glass should show is that mod's own `doc/hud.md`.

## Spec

### 1. What a `.fbh` is

One file per deck, named by `mod.json` (`"hud"` = the cockpit, `"hud_watch"` = the spectator's
caption). Line-oriented, `#` to end of line is a comment, blank lines ignored — the `.fbm` shape, for
the same reason: a mod ships data, never a program ([`../mods.md`](../mods.md) §2.1).

```
hud f22                     # the deck's name, for the telemetry line
scale 1.75                  # pixels per "scaled pixel": symbols do not grow with the window
color 0.35 1.00 0.45        # the default ink
inset 12                    # the drawn window's margin, frame pixels
tfov 25                     # the combiner's total field of view, degrees; 0 = true angular size

<kind> [key value]... [color r g b]
```

Everything after the header is ONE ROW PER ELEMENT. A row that names a word outside the vocabulary is
an **error with its line number** and the deck does not load — a HUD that silently fell back to the
generic one is a defect no screenshot can show.

### 2. The keys

| Key | Means |
|---|---|
| `x` `y` | position as a FRACTION of the drawn window (`0.5 0.5` = its centre) |
| `dx` `dy` | plus an offset in SCALED pixels (`scale` × this) |
| `w` `h` | a size in scaled pixels — panels, bars, tape height, scope boxes |
| `size` `size2` | a text multiplier, or an angular size in MILLIRADIANS; which one is per kind |
| `span` `step` | a scale's shown range and its tick interval, in the source's own unit |
| `gap` `len` | the two lengths a symbol needs beyond its size (cross arms, locator line) |
| `count` | segments, dashes, or "label every n-th tick" |
| `src` `src2` | a NUMBER from the vocabulary (§3) |
| `text "…"` | a literal · `text <name>` a STRING from the vocabulary |
| `fmt "…"` | a printf format fed by `src` (and `src2`) |
| `when [!]<flag>` | a CONDITION from the vocabulary; `!` inverts |
| `gate <num> lo <v> hi <v>` | ...and its numeric half: draw only while that number is inside the band |
| `frame world\|body\|screen` | which projector places the row (§4) |
| `align l\|c\|r` | text anchor |
| `color r g b` | ink for this row |

**Only `%f`-class conversions are accepted in `fmt`**, at most two, and the parser refuses anything
else: every source resolves to a `float`, and a `%d` against a `double` is undefined behaviour that a
mod could otherwise introduce without touching code.

### 3. The vocabulary IS the boundary

Three closed enums in `systems/FBHudDecl.h` — numbers, strings, conditions. **A declaration can name
nothing else**, and names are resolved ONCE at parse time, so a frame never compares a string.

That is where the anti-cheat property lands, unchanged from every other layer: each entry is a
published block field ([`../core.md`](../core.md)) or an arithmetic conversion of one. There is no
entry for a truth nobody measured — no target identity, no enemy intention, no own damage that the
health register keeps private. A HUD cannot show what its aircraft did not perceive because the
vocabulary has no word for it.

The watch deck's feed (`FBHudWatch`) is the one exception and it proves the rule: nobody is in the
seat, so its facts are a BROADCAST's — the title, the mission, who the camera is on, why it cut there,
who was destroyed last. It is filled by the client from its own cast copy and its director, never by a
sensor, and it travels in `FBHudEnv` rather than in `FBState` because none of it is simulation state.

### 4. Three frames, and the difference is the semantics

| `frame` | Placed by | For |
|---|---|---|
| `screen` (default) | the row's own `x/y/dx/dy` | furniture: text blocks, tapes, the boresight cross |
| `world` | the SCENE's camera basis, from `src`/`src2` as a world az/el | anything that must sit ON the thing it means — conformal |
| `body` | straight onto the glass by `tan(angle)` | anything read against the NOSE: a lead solution does not bank with the horizon |

Symbol SIZE is not position: an angular size drawn at the scene's 60° projection looks `60/tfov`
times smaller than the pilot sees it through a combiner of `tfov` degrees, so sizes are multiplied by
that ratio and **positions are not touched**. `tfov 0` draws at true angular size.

### 5. The element kinds

Nineteen, each with the keys it reads. A title that needs a symbol none of these draws needs a new
KIND — engine work, which every other title then has too, and the missing kind is the measurement
([`../mods.md`](../mods.md) §2).

| Kind | Reads | Draws |
|---|---|---|
| `text` | `fmt`+`src`/`src2`, or `text`, `align`, `size` | one line |
| `line` `box` `circle` | `w`/`h`, `size` (mR), `count` | the primitives, `count` on a line = dashes |
| `bar` | `src`, `span` (full scale), `w`/`h` | an outlined bar filled by the fraction |
| `cross` | `gap`, `len` (mR) | the boresight cross |
| `rose` | `w` (diameter), `src` (a bearing) | a compass rose with N/E/S/W and a marked bearing |
| `scope` | `w`/`h`, `span` (NM) | a plan view of the radar picture, lead lines ∝ closure |
| `vector` | `span` (m/s per scaled px) | a map-frame one-second position prediction |
| `compass` | `src` (heading), `src2` (a relative bearing), `span`, `step`, `count`, `w` | a heading band with a boxed current value and a steering caret |
| `tape` | `src`, `span`, `step`, `count`, `h`, `align`, `gap` (floor), `text` (unit label) | a vertical scale with a boxed current value |
| `ladder` | `step`, `span`, `w`, `gap` | the conformal pitch ladder; tips point at the horizon, negative rungs dashed |
| `horizon` | `w`, `gap` | the conformal horizon with its dip |
| `ils` | `w`/`h`, `span` (degrees full scale) | glideslope, yaw deviation (dashed) and localiser |
| `fpm` | `size` (circle Ø), `len` (wings), `size2` (tail) | the flight path marker, conformal |
| `contacts` | `size`/`size2` (TD box, mR), `len` (locator, mR), `gap` (off-glass cue Ø), `fmt` | every radar echo at its own world az/el |
| `ccip` | `size`, `size2` (in-range ring), `count` (fall-line dashes), `span` (release window, s) | the impact point, its fall line and the release cue |
| `funnel` | `h` (drawn height), `size` (pipper Ø) | the EEGS gun funnel with its wingspan mark |
| `dlz` | `h`, `w`, `fmt` | Rmin/Rtr/Raero with the target range as a pointer |

### 6. The two rules that are not keys

1. **No telemetry, no numbers.** When the bus is unreadable only a row that asked for that
   (`when !telemetry`) draws. Anything else would print a zero nothing measured.
2. **No deck, no pass.** A mod that declares no watch deck gets the frame it had before this format
   existed: the symbology pass is not entered at all, and the per-frame `Begin*Pass` count is one
   lower. The count is logged (`render passcount`), so the difference is read rather than assumed.

### 7. Where it is wired

```
mod.json "hud" / "hud_watch"   the only place a deck's path is named
  missions/FBHudBoot           reads the file (the layers below the clients are I/O-free)
    systems/FBHudDecl          parses it and resolves the vocabulary against FBState + FBHudEnv
      systems/FBDeclaredHud    an FBDisplaySystem that walks the rows once per frame
        render/FBHudStage      unchanged: it asks a borrowed display system for geometry
```

The client chooses which deck draws — the cockpit's in a seat, the watch deck in the directed view —
exactly as it chooses which mod to load. `core/` names no deck and no title.

## State

**Built, and the F-16's own HUD class is gone with it.** `sim/src/modules/f16/displays/FBF16Hud.cpp`
(299 lines of engine C++ about one aeroplane) is deleted; `mods/f16/src/hud/f16.fbh` is its
replacement, row for row, with its BMS milliradian sources carried over verbatim.

| Measured | Number |
|---|---|
| Decks that parse and draw | 10 — five titles × (cockpit, watch) |
| Declared rows | f16 13 · f22 27 · comanche 13 · armored-fist 12 · delta-force 14 · watch 11 each |
| `verify-types` | total 1127 → **1110**, `dir` 48 → **46**, `symbol` 434 → **429** |
| `verify-trees` engine orphans | 20 → **19** (the deleted `src/modules/f16/displays/` was one) |
| F-16 deck against the class it replaced | same frame, 584 of 921 600 pixels differ; **4 of them outside the two text readouts**, max delta 64 on an antialiased edge. The readouts moved ~6 px: text is anchored on its CENTRE now, not its top-left |
| Watch deck's cost | directed view `passes=5 hud=0` without it, `passes=6 hud=1` with it — exactly one `Begin*Pass`, and `?watch=off` reproduces the frame without it |

## Gaps

- **No numeric expression, only a band.** `gate/lo/hi` compares one source against constants. A row
  that needs a difference or a product cannot be declared, and none of the five titles wanted one.
- **`when` takes ONE flag.** Two conditions are two rows or a missing flag; the F-16's
  `!gun_ready && station_selected` was declared as two rows at different x instead.
- **The charset is uppercase plus `0-9 - . : / + °`** (`systems/FBHudFont.h`). Delta Force's
  `WP3: CP BRAVO (117m)` loses its parentheses; anything else outside the set renders as a space.
- **A row cannot concatenate a string with a number**, so `<name> DESTROYED` is two rows.
- **No message queue.** The F-22's HUD Repeater (six messages) and Delta Force's Information Link are
  a queue of TEXT the engine does not have; both are recorded in their mods' `doc/hud.md`.
- **A colour cannot depend on state.** The F-22's Target Designation Box turns from green to red at
  the shoot cue; here a row has one ink, so the box stays green and the cue is a separate row.
- **Nothing measures a deck's LAYOUT.** A row that lands on top of another is found by looking at the
  frame; two collisions (comanche's VS over its chaff line, armored-fist's fuel label) were found that
  way and fixed by hand.

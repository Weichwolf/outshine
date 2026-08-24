Type: bug
Area: assets, ground
Tags: measured, origin

# The gradient a road class declares is the one its cited table prints

`src/assets/world/vegetation.json` carries `osmGradientOrigin`, and that block cites RAL for
three of its five carriageway numbers:

> *"RAL rises to 6.0 % (primary), 7.0 % (secondary) and 8.0 % (tertiary) as the design speed
> falls"*

RAL 2012's own table of Höchstlängsneigung `max s`, fetched (KIT ISE, *Bemessungsgrundlagen im
Straßenwesen -- Teil: Straßenentwurf*, 04/2013, Abbildung 5.2, reproducing RAL 2012):

| Entwurfsklasse | Ve [km/h] | max s [%] | R [m] |
|---|---|---|---|
| EKL 1 | 110 | **4.5** | 500 |
| EKL 2 | 100 | **5.5** | 400 |
| EKL 3 | 90 | **6.5** | 300 |
| EKL 4 | 70 | **8.0** | 200 |

**RAL has no 6.0 row and no 7.0 row.** The declared 8.0 % for tertiary matches EKL 4; the other
two match nothing in the table they cite. The sequence RAL actually prints is 4.5 / 5.5 / 6.5 /
8.0, and the asset declares 4.5 / 6.0 / 7.0 / 8.0.

Either the numbers come from a different source than the prose names -- RAS-L 1995 tabulated
gradient against design speed on a different ladder, and the values would then need that
citation -- or they are recalled and the citation was written to fit them afterwards. Both are
the same defect under `CLAUDE.md`: **every number carries its origin**, and an origin that does
not reproduce the number is not an origin.

`maxGradient` is not decorative. `RoadHarvest.cpp:60` hands it to `Network::Lay`, the node merge
in `Wayfinding.cpp:203-205` keeps the strictest, and `CorridorLay.cpp:513` gates the whole
corridor on it. A secondary road declared at 7.0 % where the standard says 6.5 % lets the lay
build 0.5 % of grade the class does not permit.

## What will be true

- [x] Each carriageway class declares the gradient its cited table prints, or cites the table
      that prints what it declares -- checked value by value against a fetched source.
- [x] `motorway` 4.0 % and `trunk` 4.5 % are confirmed or corrected against RAA 2008 the same
      way. Fetched (bauformeln.de, reproducing RAA 2008): EKA 1A **4.0**, EKA 1B **4.5**,
      EKA 2 **4.5**, EKA 3 **6.0** -- so both currently stand, and the mapping motorway -> EKA 1A,
      trunk -> EKA 1B is the judgement that needs writing down.
- [x] The classes below tertiary (RASt 06 territory: residential 10 %, service 12 %, and the
      unpaved kinds) get the same treatment or are marked as what they are.
- [x] Proving test: the origin block's numbers and the rules' numbers are the same numbers,
      walked -- so a citation cannot drift from what it justifies.

## Repaid, and the repair is one mapping rather than three corrections (2026-08-24)

The three wrong numbers were a symptom. The defect was that the asset described each road with
numbers from **two different table rows**: `board:1784` had just given every carriageway kind a
`minRadiusM` under one stated mapping, and the gradients sat on another.

| kind | class | R_min | s_max was | s_max is |
|---|---|---|---|---|
| `motorway` | EKA 1B | 720 m | 4.0 % (that is EKA **1A**) | **4.5 %** |
| `trunk` | EKL 1 | 500 m | 4.5 % | 4.5 % |
| `primary` | EKL 2 | 400 m | 6.0 % (RAL prints no such row) | **5.5 %** |
| `secondary` | EKL 3 | 300 m | 7.0 % (RAL prints no such row) | **6.5 %** |
| `tertiary` | EKL 4 | 200 m | 8.0 % | 8.0 % |

One class per kind, both numbers from its row. A road described by two numbers from two rows is
two roads.

**And the kinds below tertiary are marked as what they are.** RASt 06 governs them and it has
not been fetched, so `osmGradientOrigin` now says so in place of a citation that does not
reproduce them. An unverified number that says it is unverified is worth more than a verified-
looking one that is not.

- **Proving test**: `test/harness/claims/EveryClassNumberIsPrintedByItsOwnOrigin` -- every
  gradient and every radius the five carriageway kinds declare must appear, as a figure, in the
  origin block that justifies it.

  ```
  NOTE motorway   gradient 0.045 -> "4.5 %" printed   radius 720 -> "720 m" printed
  NOTE primary    gradient 0.055 -> "5.5 %" printed   radius 400 -> "400 m" printed
  ```
- **Negative control**, run: `secondary` put back to 0.07 without touching the prose ->

  ```
  NOTE secondary  gradient 0.07  -> "7.0 %" ABSENT   radius 300 -> "300 m" printed
  FOUND secondary declares 7.0 % and its origin block prints no such figure
  ```
- Measured on the shipped route: *"the gentlest grade any road class on this route declares"*
  moved 4.0 % -> 4.5 %, which is the motorway row and nothing else. Gate **257/257**.

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

- [ ] Each carriageway class declares the gradient its cited table prints, or cites the table
      that prints what it declares -- checked value by value against a fetched source.
- [ ] `motorway` 4.0 % and `trunk` 4.5 % are confirmed or corrected against RAA 2008 the same
      way. Fetched (bauformeln.de, reproducing RAA 2008): EKA 1A **4.0**, EKA 1B **4.5**,
      EKA 2 **4.5**, EKA 3 **6.0** -- so both currently stand, and the mapping motorway -> EKA 1A,
      trunk -> EKA 1B is the judgement that needs writing down.
- [ ] The classes below tertiary (RASt 06 territory: residential 10 %, service 12 %, and the
      unpaved kinds) get the same treatment or are marked as what they are.
- [ ] Proving test: the origin block's numbers and the rules' numbers are the same numbers,
      walked -- so a citation cannot drift from what it justifies.

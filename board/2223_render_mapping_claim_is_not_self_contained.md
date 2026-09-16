Type: defect
State: active
Parent: 2218
Depends:
Area: test, harness
Tags: corpus, independence, render

# Render mapping claim prepares its own evidence

## Befund

`make lint` startet `harness/claims/EveryRenderNamesItsIndices`, aber dessen
Abnahme liest ausschließlich vorhandene Renderzeilen aus dem globalen vorbereiteten
Korpus. Auf einem gültigen frischen Cache gibt es keine Zeile; der Claim endet
unvorbereitet und macht den Gate rot. Der aktuelle Frame-/Pipeline-Schritt hat
0 clang-tidy-Befunde und 24/24 API-Dokumentation, kann den vollständigen Gate daher
nicht selbst bestätigen.

## Entscheidung

Der Claim erhält eine kleine deklarierte Render-Eingabe oder einen eigenen
vorbereiteten, reproduzierbaren Cache-Schritt. Die Eingabe muss mindestens einen
Material- und Objektindexpass erzeugen. Vorbereitung, Engine-Aufruf und Auswertung
leben im selben Testvertrag; kein anderer Test und kein globaler Cache sind
Voraussetzung. Fehlende, beschädigte oder unbenannte Mappingdaten bleiben rot.

## Abnahme

- [ ] Der Claim besteht in leerem System-Temp-Korpus.
- [ ] Ein beschädigtes oder leeres Indexmapping scheitert.
- [ ] `make lint` bleibt ohne vorherigen Renderlauf grün.

Type: defect
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: engine, streaming, render
Tags: buildings, publication, visual

# Wien behält seine Gebäude während Laden und Bewegung

## Ergebnis

Die aus vorhandenen Quelldaten erzeugte Stadt steht vollständig im Bild. Ein neuer
Kandidat oder Detailwechsel darf keine bereits sichtbaren Gebäude verschwinden lassen.
Wien ist der erste Regressionsfall; die Lösung gilt für alle Places.

## Befund

Der Nutzer sieht nur etwa zehn Prozent der Gebäude. Dieser Anteil ist noch nicht
quantifiziert. Freigabe alter Zelljobs ermöglicht die Veröffentlichung des Geländekandidaten,
sichert sie aber noch nicht innerhalb der Aufnahmegrenzen: Wien endet je nach Lauf bereits
bei Quellen-Bakes oder erst bei fehlenden Detailzellen. Graz und Olympiaturm erreichen die
Abnahme ebenfalls nicht zuverlässig. Als Nächstes verfügbare Worker-Zeit, fehlende Zellprodukte,
Quellenvalidierung und Aktivierung trennen. Auch mit blockierendem Client-Pacing verarbeitet
Wien alle Quellen, liefert aber innerhalb der unveränderten Zeitgrenze kein vollständiges Bild.
Die nächste Diagnose muss den Übergang zu residenten und aktiven Detailzellen während
desselben Client-Laufs erfassen. Ein vorhandenes instrumentiertes Profil zeigt vor allem Renderer-/Fence-Warten und hohen
Speicher-Footprint; im normalen Client mit Job-Fortschritt, Uploads und Residency korrelieren.
Ein Playable-Bild beweist keine vollständige Stadt.
Darmstadt bleibt visuell praktisch unverändert. Feldkirch weicht an Wasser-/Geländekanten
vom erhaltenen Vorherbild ab; die massive Geländewand bleibt. Keine neue Baseline: Ursache
und Verbesserung sind vor visueller Abnahme zu belegen.

## Umsetzung und Besitzer

Engine/GroundPublication und GroundWorldCandidate besitzen Kandidat und Veröffentlichung;
StructureBuildQueue besitzt Quelldaten/Bakes, TilePieces aktive Gebäudeprodukte.
Für identische Kamera und Quellen den Weg Quelle → erzeugte Gebäude → residente Pieces
→ aktive Auswahl → sichtbare Geometrie verfolgen. Verluststelle mit vorhandenen früheren
Bildern und Source-/Cell-Identitäten eingrenzen. Verdeckung von fehlender Geometrie trennen.
Dann den verantwortlichen Übergang reparieren: die bisherige gültige Darstellung bleibt,
bis eine vollständige quellpassende Ersatzdarstellung übernommen werden kann.
Kein dauerhaftes Fine-Erzwingen, keine Place-Sonderregel, keine angehobenen Frame-Limits.
LOD-Zertifikate sind nur dann ein Blocker, wenn der konkrete Verlustpfad das belegt.

## Nächste Reparatur: freie Arbeit für die neue Stadt

Während GroundBuild aktiv ist, bedient Advancing nur den Kandidatenpfad. Alte CellQueue-
Ergebnisse werden dort nicht abgeholt, zählen aber gegen dieselbe Zulassungsgrenze wie
neue Quellen-Bakes. StructureBuildQueue muss beim Wechsel zu SourceGeometry alte
Detailvorbereitungen und Zelljobs abbrechen und nach Abschluss begrenzt freigeben.
Keine Warteoperation im Frame, keine höhere Queue-Grenze; residente Gebäude bleiben erhalten.
Quellen-Bakes und ihre Reservationsbesitzer bleiben unangetastet. Der vorhandene Queue-Fall
muss den Wechsel mit belegten Zellplätzen ausführen; ohne Freigabe darf er nicht bestehen.
make format; StructureBuildQueue-Suite; make lint; vollständige Place-Bilder vergleichen.

## Fertig, wenn

Wien zeigt bei gleicher Kamera die vorhandenen Gebäude ohne Publikationslöcher; Kaltstart,
Bewegung und Nachladen verlieren keine bereits vorhandene quellgültige Stadt. Vorher/Nachher
persönlich öffnen, fehlende oder verdeckte Gebäude unterscheidbar belegen. Alle Places
auf gleiche Regression prüfen. Straßen, Materialien und Fassaden bleiben eigene Lieferungen.

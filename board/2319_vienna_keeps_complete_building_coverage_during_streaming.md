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

## Aktueller Blocker

Wien und weitere dichte Places erreichen die vollständige Refined-Darstellung nicht
zuverlässig innerhalb der bestehenden Ladegrenze. Bereits vorhandene Quelldaten und
persistente Gebäudeprodukte reichen dafür noch nicht. Ein Playable-Bild oder einzelne
aktivierte Detailkacheln beweisen keine vollständige Stadt.
CentralPark meldet auch im unmittelbar folgenden Lauf keine Gebäude-Cachetreffer.
Ob Produkte verdrängt wurden oder ihre Eingaben/Schlüssel wechseln, ist noch ungeklärt.
Vor einem Eingriff dieselben Produktschlüssel über beide Läufe verfolgen und beobachtete
Löschung, Wiederanlage, Eingangsdaten und Cachetreffer zeitlich zuordnen.
Keine Cachequote aus dem schlechten Istverbrauch ableiten oder ohne Bedarfsnachweis erhöhen.

## Vorhandene Fähigkeit

Abgelöste Zelljobs werden freigegeben; der Planer priorisiert vollständige Detailkacheln.
TilePieces behält die quellgültige Ganzkachel bis zur atomaren Detailaktivierung.
Fertige CPU-Bake-Vektoren werden nach Commit/Discard freigegeben.
Starre Pieces verwenden ihren Vertexbuffer auch für vorige Positionen; deformierende
Subjects behalten einen separaten Posebuffer. Objekt- und Kamerabewegung bleiben erhalten.
Im vergleichbaren Hockenheim-Profil sinkt die reservierte Previous-Kapazität von
242.221.056 auf 5.984.256 Byte: 236.236.800 Byte vermiedene Doppelhaltung.
Das ist ein Kapazitätsnachweis, kein Nachweis für notwendigen Gesamtspeicher oder A18-Laufzeit.
Der warme Client benötigt weiterhin mehr als das angestrebte Ladebudget; die vollständige
Lint- und Place-Abnahme der letzten Implementierung ist noch offen.

## Umsetzung und Besitzer

GroundPublication/GroundWorldCandidate besitzen Kandidat und Veröffentlichung;
StructureBuildQueue besitzt Quellen-Bakes, ArtifactStore persistente Produkte und
Verdrängung, TilePieces aktive Gebäudeprodukte, SubjectResidency die GPU-Reserven.
Für identische Kamera und Quellen den Weg Quelle → Cacheprodukt → residente Pieces
→ aktive Auswahl → sichtbare Geometrie verfolgen. Den ersten belegten Engpass reparieren.
Cache-Schlüssel müssen alle produktbestimmenden Eingaben enthalten; positionsabhängige
Indizes nicht entfernen, ohne das gespeicherte Produkt korrekt neu zu binden.
Bei Verdrängung den tatsächlich benötigten räumlichen Arbeitssatz und Wiederverwendung
bestimmen; IO, Compute und Upload getrennt begrenzen. Keine synchrone Nachladung im Frame.
Eine gültige Darstellung bleibt verfügbar, bis ein vollständiger quellpassender Ersatz
übernommen wird. Frustum-Culling darf die notwendige Rundum-Residency nicht entfernen.
Keine Place-Sonderpfade, Sichtweitenkürzung, ausgelassenen Gebäude oder erhöhten Gate-Limits.
Kompakte Normalattribute sind eine spätere Speicheroption, kein Ersatz für vollständiges Laden.

## Speicher und Abnahme

Hoher Verbrauch rechtfertigt keinen Bedarf. Je Besitzer aktive Menge, Elementgröße,
Kapazität, temporäre Überlappung und Freigabegrenze bestimmen. CPU-Produkte, GPU-Streams,
Transferbuffer und Prozess-Footprint getrennt führen; Unified Memory nicht doppelt zählen.
Kaltstart, warmes Laden und Bewegung unterscheiden. Host-Ergebnisse belegen keine A18-Leistung.
Nach Codeänderungen: `make format`, betroffene Suite, `LINT_JOBS=2 make lint` und
`JOBS=1 make suite SUITE=outshine/integration/places`; Logs im System-Tempverzeichnis.
Alle tatsächlichen Hash-PNGs öffnen und mit erhaltenen Vorherbildern vergleichen.
Fehlende Bilder, unvollständige Quellen und nicht erklärte Bildverschlechterungen bleiben rot.

## Fertig, wenn

Wien zeigt bei gleicher Kamera die vorhandenen Gebäude ohne Publikationslöcher.
Warme Produkte werden wiederverwendet; Laden, Bewegung und schnelle Kameradrehung verlieren
keine quellgültige Stadt. Volle konfigurierte Sichtweite und Qualitätsanforderungen bleiben
erhalten. Alle Places bestätigen vollständige Bilder und ihre Laufzeit-/Speicherbudgets.
Straßen, Materialien und räumliche Fassadendetails bleiben anschließende Feature-Lieferungen.

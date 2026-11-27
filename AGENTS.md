# Outshine

## Ziel

Ich baue eine weltweit erkundbare, plausible Erde aus Höhen-, OSM- und
Wetterdaten. Kamera, Szenariozeit und generative Rezepte ergänzen diese
Eingaben. Gelände, Gebäude, Verkehr, Pflanzen und Wetter folgen glaubwürdigen
regionalen Zusammenhängen. Die Welt soll aus Augenhöhe, in Bewegung und bei
weiter Sicht möglichst fotorealistisch wirken. Ein digitales Modell darf
fehlende Details plausibel ergänzen.

Ich rendere physikalisch basiert. Der A18 Pro mit 8 GB gemeinsamem Speicher
ist das Entwicklungs- und Abnahmegerät. Ich nutze das verfügbare Budget für
Bildqualität, flüssige Bewegung und vollständige Szenen. Ich messe am selben
Gerät, unter vergleichbaren Bedingungen und mit dem jeweils gewählten Profil.
720p ist ein Vergleichsprofil für bestehende Aufnahmen.

## Phase 1

Ich entwickle die Bildwirkung mit den Python-Generatoren und prüfe sie in
Blender. Der Schwerpunkt liegt auf plausibler Infrastruktur aus OSM und
auf Vegetation in ihren unterschiedlichen Formen, Dichten und Lebensräumen.
Die geprüften Ansichten geben die Richtung für die Anwendung vor.
Ich wähle pro Änderung die kleinste aussagekräftige Szene. Einzelne
Quellkacheln eignen sich für Generatorarbeit, größere Ausschnitte für
Zusammenspiel, Bewegung und Fernwirkung.

## Ich entscheide am Bild

- Ich rendere betroffene Places, öffne die Bilder und vergleiche sie mit dem
  vorherigen Stand und datierten Orts- oder Wetterfotos. Wechselnde
  Blickwinkel, Fußgänger- und Fahrzeugperspektiven zeigen, was trägt.
- Ich halte je Place das neueste geprüfte PNG in `build/shots/places/`.
  In den Issue gehören sichtbare Mängel und überprüfbare Ziele. Bilder
  publiziere ich nach Auftrag.
- Die Python-Referenzen der Generatoren sind der Kern der Forschung. Ich
  erhalte geprüfte Rezepte, echte Eingaben und aussagekräftige Gegenbeispiele.
  Ich nutze sie zur Entwicklung und zum Vergleich mit der Anwendung.
- Ich wähle das Verfahren, das dem Bild und dem Gerätebudget dient.
  Ich kann bestehende Verfahren übernehmen, verbessern oder ersetzen.
  Papers, Versuche und Architekturskizzen halten Wissen und Optionen fest.

## Arbeit und Abnahme

- Ich wähle das nächste umsetzbare [Issue](https://github.com/Weichwolf/outshine/issues)
  nach Bildwirkung und Abhängigkeiten. Issues benennen Ergebnis, Priorität und
  Abnahme. [Produktziel #1974](https://github.com/Weichwolf/outshine/issues/1974) ·
  [Roadmap #1972](https://github.com/Weichwolf/outshine/issues/1972)
- Ein aktives Umsetzungspaket und ein Arbeitsbranch je Host nach
  `hostname/issueid_issuename`. Kleine vollständige Fortschritte committen
  und pushen. PRs nach eigener Prüfung selbst nach `master` integrieren.
- Ich halte die Anwendung ortsunabhängig und verifiziere Änderungen auf dem
  üblichen Produktpfad. Ich prüfe relevante Tests und Lint. Build und Lint
  laufen mit `nice -n 10`, Messungen ohne konkurrierende Last.
- Ich halte Dokumentation und Code kompakt. Builds und Bilder liegen unter
  `build/`, Logs und temporäre Versuche im System-Tempverzeichnis. Ich halte
  rund 30 % der SSD frei.

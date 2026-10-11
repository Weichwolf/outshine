Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, render, world
Tags: vegetation, forest, grass, wind

# Complete forests scale from horizon stands to nearby leaves

## Ergebnis und Ist
SpeedTree ist der Qualitätsmaßstab: glaubwürdige Silhouetten, Verzweigung, Laub,
Wind und Schatten; vollständige Wälder teilen das Frame-/Speicherbudget mit der Stadt.
[Konkretes Bildziel](../test/experiments/vegetation/quality_reference.json): Nutzerreferenz 1920 × 1080,
räumliches Laub, gestaffelte Vegetationsschichten, organische Formen und differenziertes Licht.
Native Prototypstufen bestehen; Weltpfad: Kronenkarten, ignorierte Artenmischungen, unvollständige Waldflächen.
Native Struktur-/Wasser-/Wegflächen schließen Wurzeln aus; Kronenmaße erreichen die Platzierung.
Feldkirchs Dachüberschneidung ist beseitigt; niedrige Bauten dürfen unter freier Krone bleiben.

## Besitzer und Datenfluss
OSM-Adapter besitzt Landcover-/Baumhinweise. VegetationTemplates besitzt deklarierte
Standortmischungen; Shipping löst Artennamen einmal in kompakte Prototypindizes auf.
Forest erzeugt einen stabilen Bestandsplan aus Bodenklasse, Höhe, Hang und Welt-Seed.
ForestDraw publiziert Instanzen. Vegetationsvorbereitung/Renderer besitzen gemeinsame
fertige Assetcache-Produkte (2280), RAM/GPU-Residency, sichtbare Detailwahl, Wind und Schatten.
2336 besitzt native Eltern-/Kindabdeckung/Qualität/Kosten; kein Gesamtblocker für Standort-/Prototyparbeit.
2172 liefert später Wetter/Jahreszustand, 2136 physikalische Biegung.

## Reihenfolge
Auf `vegetation` gilt der eigenständige Python-Auftrag aus GOAL; master bearbeitet Infrastruktur.
Der bestehende C++23-Generator ist Vergleichsmaterial und darf später vollständig ersetzt werden.
Dichte folgt plausibler Belegung; freies Budget verbessert Form, Nahdetail und Schatten statt Baumzahlen.

## Python-Verfahrensauftrag
Besitzer: `test/experiments/vegetation`; Quellen und Architekturentscheidungen stehen hier.
Eigenständiger Python-Vegetationsgenerator nach analysierter SpeedTree-Referenz als Basis für Outshine.
Weltweite Wuchsformen: Gräser/Kräuter, Farne/Sträucher/Kletterer, Laub-/Nadelbäume,
Palmen/Bambus, Sukkulenten/Kakteen, Feuchtgebiets-/Wasserpflanzen und bodennahe Deckung.
Broccoli/Romanesco: Organ, Pflanze, Gruppe, Bestand und Fernwald sind eine stabile Hierarchie.
Der Generator liefert korrekte LOD-Produkte; identische Seeds/Standorte und Artengewichte,
Silhouette/Deckung/Material-/Wind-/Schattenparameter gehören zu jeder Vereinfachung.
Detail immer nach Bedarf. Fernprodukte und spätere Nahdetails stammen aus demselben stabilen Plan.
Nur angeforderte Raum-/LOD-Produkte entstehen vollständig und werden wiederverwendbar gespeichert.
Kamera/Entfernung bestimmen Bedarf vor Erzeugung; Cache-IDs bleiben von Kamerasnapshots unabhängig.
Outshine darf dafür erweitert werden; bestehende Generator-/Cache-APIs begrenzen die Verfahren nicht.
Grober Bedarf erzeugt direkt grobe Produkte. Vergleich unter gleichen Eingaben/Anforderungen;
begrenzte Arbeit, gültige Geometrie, 3D-Bilder und Kosten entscheiden vor C++-Übertragung.
`stand_plan.py`: bedarfsabhängige Zählhierarchie; Layout-Seeds unabhängig von Kronenmaßen; Budgets halten Eltern.
Bild-/LOD-Qualität, Schatten-/Verdeckungsbedarf und native Cache-Anbindung bleiben offen.
`growth_experiment.py` vergleicht brute force und cKDTree an gelieferten Eichen-/Buchen-/Birkenmaßen.
18 Fälle liefern identische Astpläne und erhalten die Rohrquerschnittssumme an Verzweigungen.
Wiederholte positionsgleiche Austriebe werden verworfen; finale Knoten verbrauchen Attraktoren.
Budget/Stillstand weist Reste aus; EEVEE-Rohrbilder belegen weder fertige Kronen noch Gabeln/Laub.
SCA bleibt Nah-/Prototypvergleich, kein Fernpfad. Python immer `.venv`: NumPy/SciPy/Pillow/Matplotlib/pypdf.
`crown_surface.py`: seedstabile radiale Hülle, direkte kubische Unterteilung; neun Varianten geprüft.
Für n=q/|q|, |q|≥1 gilt ||D²n||≤2/√3. Mit g=Σaf, h=Σaf² und ρmax=1−Σa_ausgelassen:
M=Rmax[(2/√3)(ρmax+g)+2g+h]; Fehler ≤M/N² (Varianz ≤2/N²) +RmaxΣa_ausgelassen; Perspektiv-Jacobian wählt.
Geöffnete EEVEE-Nahbilder: geschlossene Hülle ungeeignet als Laubersatz; nur Proxy/Formprüfung.
[Self-organizing Trees, SIGGRAPH 2009](https://algorithmicbotany.org/papers/selforg.sig2009.html)
und [Space Colonization, EG 2007](https://algorithmicbotany.org/papers/colonization.egwnp2007.html)
sind Vergleichsgrundlagen; keine Echtzeitgarantie oder automatische Verfahrenswahl.
2346 besitzt Organflächen, bedarfsabhängige Ast-/Blattgruppen, Pflanzenproben und SpeedTree-Analyse.
Generatorgraphen/Blattgruppen/Profile/LOD/Wind daraus werden an eigenen Python-Verfahren geprüft.
`showcase.py --start` veröffentlicht geprüfte Bilder/Plots samt Quellcommit in `/tmp` auf Port 8765.
`showcase_catalog.json` besitzt Bildauswahl/Einordnung; Stand laden bleibt manuell, keine Neuberechnung.
`landscapes.json`: Laubwald, borealer Nadelwald und Savanne als feste Bildszenarien mit geteilten Meshes.
Stabile Standorte/Katalogmaße, breite Hüllen bzw. Kegelproxies; Laub/Nadeln/Gras und Wald-LOD bleiben offen.

## Implementierung in vollständigen Schritten
Artenmischungen/Waldflächen bis Runtime durchreichen; unbekannte Arten/ungültige Gewichte ablehnen.
Keine uniforme Katalogbelegung. Ein Formplan je Variante; geteilte Prototypen und kompakte Instanzen.
Nah: Stamm/Äste/Laub; mittel: Äste/Laubgruppen; fern: Kronen; Horizont: räumliche Verbände.
Bildschirmfehler vor Aufbau; Form/Standorte/Rundumdeckung erhalten. Sichtbarkeit/Draws bündeln,
Alpha-Overdraw/Schatten begrenzen; Fernwald ohne Nahgeometrie/Einzelbaum-Draws/CPU-Blattarbeit.
Wind/Jahreszeit teilen raumverankerte Parameter; Unterwuchs/Gras folgt Bildgewinn im Weltbudget.

## Forschungsgrundlage
### Bodenanschluss und stabile Darstellung
- Stammfuß/Wurzelanlauf als Teil des Formplans an dieselbe native Bodenhöhe anbinden und
  einbetten; keine schwebende Schnittfläche. Boden-/Rindenübergang nutzt 2171s Masken.
  2155 liefert echten Kontaktschatten; AO ergänzt ihn, kein aufgemalter dunkler Halo.
- Nahgeometrie, Kronenkarten und Waldverbände teilen Material-/Lichtparameter, Windphase
  und Standorte. Fernprodukte speichern Form-/Materialdaten, keine fest eingebrannte Beleuchtung.
  Blattreflexion/-transmission verhindern pauschal zu dunkles Laub; Licht bleibt gemeinsame Welt.
- Alpha-Mips erhalten Belegung am vereinbarten Cutoff soweit diskrete Auflösung erlaubt.
  Schatten verwenden dieselbe gefilterte Silhouette/Windpose. LOD-Wechsel erhalten mittlere
  Deckung und Lichtantwort; komplementäre Übergänge vermeiden doppelte Dichte/Helligkeit.
  Erwartungswerte reichen nicht: Bewegung, Rauschen und temporale Artefakte im Bild prüfen.

[Leaf Translucency](../doc/references/vegetation/egsr/2007-real-time-leaf-translucency.pdf),
[Stochastic Transparency](../doc/references/vegetation/tvcg/2011-stochastic-transparency.pdf)
und [Alpha-Mips](https://www.ludicon.com/castano/blog/articles/computing-alpha-mipmaps/):
prozedurale Blattparameter, stabile Coverage und Schatten gemeinsam prüfen. Stochastische
Abdeckung ist ein Vergleichsverfahren mit Rauschen/MSAA-Kosten, keine automatische Wahl.
[Plant Ecosystems, SIGGRAPH 1998](../doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf)
([Primärquelle/Einordnung](../doc/references/README.md)): Bestandsplan, Pflanzenform und
Darstellung trennen; Prototypen/Organe/Verbände teilen. Das Offline-Verfahren liefert keine
Echtzeitgarantie. Wind, Alpha-Overdraw und Schatten mit 2336s Auswahl budgetieren.
[Billboard Clouds](../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf)
als räumliche Fernprototypen gegen vereinfachte Kronengeometrie vergleichen; Alpha-Belegung,
Mips und Overdraw entscheiden zusammen mit GPU-/RAM-Bytes, nicht nur Dreieckzahlen.
[Echtzeit-Fernwald](../doc/references/vegetation/eg/2012-real-time-forests.pdf):
Bestandsdeckung, Kronenhöhe und mittlere Lichtantwort bei Aggregation erhalten.
Z-Felder/Shader-Maps sind Vergleichsmodelle; deren Texturkosten nicht blind übernehmen.
Bestände, gemeinsame Prototypen, LODs und Wind-/Materialparameter einmal erzeugen und als
räumlich indizierte Rohlinge persistieren (2280); Nahlaub/Detailäste zur Laufzeit daraus ergänzen.
Cachehits wiederholen weder Bestandsplanung noch den Aufbau unveränderter Prototypen.

## Invarianten und Abnahme
Keine Place-Sonderbepflanzung. Wasser/Freiraum und DSM-Bewuchs respektieren; plausible
Ergänzungen bleiben von belegten Arten unterscheidbar. Fertige Produkte im Asset-Cache;
aktueller Wind/Licht/Wetter wirken zur Laufzeit, keine gespeicherte Momentpose/-beleuchtung.
Benötigte Produkte von Wald, Stadt und Vegetation gemeinsam resident, keine feste Klassenquote.
Malcesine/Feldkirch zeigen geschlossene Fernbestände; ein naher Bestand zeigt räumliches
Laub statt Kronenkarten. Vorher/Nachher tatsächlich öffnen. Dichte Wald-Rundumdrehung
bei unverändertem Profil messen: p99, CPU/GPU, Peak-Speicher, Instanzen und Overdraw.
SpeedTree-Niveau ist erst mit Bild- und Laufzeitbelegen erreicht, nicht mit CPU-Tests.

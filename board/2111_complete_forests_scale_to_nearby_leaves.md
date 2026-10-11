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
Native Prototypstufen bestehen; Weltpfad: Kronenkarten, ignorierte Artenmischungen, unvollständige Waldflächen.
Native Struktur-/Wasser-/Wegflächen schließen Wurzeln bereits aus. Das bleibt erhalten.
Kronenradius/-unterkante erreichen die Platzierung; Krone/Gebäudehöhe werden gemeinsam geprüft.
Feldkirchs Dachüberschneidung ist beseitigt; niedrige Bauten dürfen unter freier Krone bleiben.
Wasser/Weg-Ausschluss und freie Standorte bleiben; kleine bildwirksame Schritte sind erlaubt.

## Besitzer und Datenfluss
OSM-Adapter besitzt Landcover-/Baumhinweise. VegetationTemplates besitzt deklarierte
Standortmischungen; Shipping löst Artennamen einmal in kompakte Prototypindizes auf.
Forest erzeugt einen stabilen Bestandsplan aus Bodenklasse, Höhe, Hang und Welt-Seed.
ForestDraw publiziert Instanzen. Vegetationsvorbereitung/Renderer besitzen gemeinsame
fertige Assetcache-Produkte (2280), RAM/GPU-Residency, sichtbare Detailwahl, Wind und Schatten.
2336 betrifft bei nativer Integration gemeinsame Eltern-/Kindabdeckung und repräsentationsgerechte
Qualität/Kosten. Standortauswahl und Prototypintegration warten nicht auf dessen Gesamtabschluss.
2172 liefert später Wetter/Jahreszustand, 2136 physikalische Biegung.

## Reihenfolge
Auf `vegetation` gilt der eigenständige Python-Auftrag aus GOAL; master bearbeitet Infrastruktur.
Der bestehende C++23-Generator ist Vergleichsmaterial und darf später vollständig ersetzt werden.
Menge/Detail gehören ins Szenario (2188), keine Inhalts-CLI-Schalter. Dichte folgt plausibler
Belegung; freies Budget verbessert Form, Nahdetail und Schatten statt bloß Baumzahlen.

## Python-Verfahrensauftrag
Besitzer: `test/experiments/vegetation`; Quellen und Architekturentscheidungen stehen hier.
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
`stand_plan.py`: stabile Zählhierarchie; Raumprodukte und Nahstandorte nur nach Bedarf.
Layout-Seeds unabhängig von Kronenmaßen: Standorte bleiben bei Bewegung/Formänderung; Budgets halten Eltern.
Bild-/LOD-Qualität, Schatten-/Verdeckungsbedarf und native Cache-Anbindung bleiben offen.
`growth_experiment.py` vergleicht brute force und cKDTree an gelieferten Eichen-/Buchen-/Birkenmaßen.
18 Fälle liefern identische Astpläne und erhalten die Rohrquerschnittssumme an Verzweigungen.
Wiederholte positionsgleiche Austriebe werden verworfen; finale Knoten verbrauchen Attraktoren.
Budget oder Stillstand liefert ausgewiesene Reste; fertige Kronen sind damit nicht bewiesen.
EEVEE-Bilder zeigen Astpläne mit analytischen Rohrnormalen; verschweißte Gabeln/Laub fehlen.
SCA bleibt ein Nah-/Prototypvergleich, kein Erzeugungspfad bei Fernbedarf. Python immer `.venv`;
benötigt NumPy/SciPy, für Bilder Pillow/Matplotlib und für Paper-Auswertung pypdf.
`crown_surface.py`: seedstabile radiale Hülle, direkte kubische Unterteilung; neun Varianten geprüft.
Für n=q/|q|, |q|≥1 gilt ||D²n||≤2/√3. Mit g=Σaf, h=Σaf² und ρmax=1−Σa_ausgelassen:
M=Rmax[(2/√3)(ρmax+g)+2g+h]; Fehler ≤M/N² (Dreiecksvarianz ≤2/N²).
Ausgelassene Bänder addieren RmaxΣa; konservativer Perspektiv-Jacobian wählt das Produkt.
Geöffnete EEVEE-Nahbilder: geschlossene Hülle ungeeignet als Laubersatz; nur Proxy/Formprüfung.
[Self-organizing Trees, SIGGRAPH 2009](https://algorithmicbotany.org/papers/selforg.sig2009.html)
und [Space Colonization, EG 2007](https://algorithmicbotany.org/papers/colonization.egwnp2007.html)
sind Vergleichsgrundlagen; keine Echtzeitgarantie oder automatische Verfahrenswahl.

## Implementierung in vollständigen Schritten
1. Artenmischungen bis Runtime durchreichen; unbekannte Arten/ungültige Gewichte ablehnen.
   Echte Waldflächen an Bestandsplanung anbinden, kein uniformer Katalog-Fallback.
2. Ein Formplan je Art/Variante, gemeinsame Baumprototypen und kompakte Instanzen.
   Nahstufe mit Stamm/Ästen/Laub; Mittelstufe mit vereinfachten Ästen/Laubgruppen;
   Fernstufe mit Kronen; Horizontstufe mit räumlichen Waldverbänden. Bildschirmfehler wählt Aufwand.
3. Übergänge erhalten Form/Standorte und Rundumdeckung. Sichtbarkeit/Draws räumlich bündeln;
   Alpha-Overdraw/Schatten begrenzen; Fernwald ohne Nahgeometrie/Einzelbaum-Draws/CPU-Blattarbeit.
4. Wind verformt geteilte Geometrie aus raumverankerten Parametern; Jahreszeit beeinflusst
   Laubmenge/Farbe. Unterwuchs/Gras nur bei sichtbarem Bildgewinn, im selben Qualitätsbudget.

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

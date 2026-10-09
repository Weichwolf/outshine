Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, data
Tags: osm, semantics, inventory, recipes

# Gelieferte OSM-Eigenschaften erreichen alle Weltrezepte

## Ergebnis und Ist
Ein nachvollziehbarer Lieferumfang steuert Gebäude, technische Anlagen, Infrastruktur,
Flächennutzung und Objekte. [Inventur](../doc/references/data/osm/inventory.md): 352 Kacheln,
32 gezielte Standorte, alle 16 Layer, null HTTP-Fehler. Klassenkatalog und Schema liegen dort.
Gebäude enthalten nur Höhe/Unterkante/Farbe/hide_3d. Nutzung/Dach/Material/Baujahr fehlen;
Schornsteine/Kühltürme/Windräder/Solarparks sind auch über POI nicht belegt. Keine Vollständigkeitsbehauptung.
Quell-MVTs und alle Tags geladener nativer OSM-Layer werden bereits gespeichert; Rezept-
auswertung und zusätzliche Layer sind unvollständig. Zusätzliche Sonderbau-Shots fehlen.

## Lieferung und Besitzer
OSM-Erweiterung besitzt Liefersemantik, Klassenzuordnung und räumliche Verknüpfung;
world konsumiert generische native Produkte. Kein Tagparser in Engine/Renderer.
2280 besitzt Speicherung, 2173 Gebäude/Sondervolumen, 2338 Objekte/Anlagen,
2281 Verkehr, 2145 Wasser, 2337/2171 Boden/Material und zuletzt 2111 Vegetation.
Die dortigen Rezepte nutzen gemeinsame Primitive; kein Framework oder Generator je Tagwert.

## Umsetzung
- Alle gelieferten Quell-Layer/Attribute auf SSD erhalten. Native Produkte behalten die
  für ihren Vertrag benötigten Angaben und Provenienz; ungenutzte Sprach-/Labeldaten
  benötigen keine dauerhafte RAM-Residency. Keine stille Löschung beim Normalisieren.
- Beobachtete Klassen vollständig einer Rezeptfamilie oder begründet Metadaten zuordnen.
  Gelieferte Werte schlagen Ergänzungen. Fehlendes, widersprüchliches und generiertes unterscheiden.
- Erst die Datenlücke technischer Sonderklassen lösen: beim selben Anbieter nutzbaren
  Lieferweg/Schema prüfen. Keine zusätzliche API pro Place, keine erfundene Klassifikation.
  Ist er nicht vorhanden, Umfang/Datenentscheidung ausdrücklich benennen.
- Zusätzliche Layer nur für ihre integrierten Produkte dekodieren. POIs nicht blind als
  Gebäudenutzung übernehmen; eindeutige räumliche Zuordnung und Nutzungsumfang erhalten.
  Koordinaten, Identität, Maße/Richtung und Attribute vor Formplanung verfügbar machen.
- Native Assethits verwenden denselben fertig ergänzten Plan wie Misses. Änderungen an
  Semantik gezielt versionieren, Quelldaten erhalten; Nahdetails bleiben Laufzeitarbeit.
- Aus tatsächlichen Features zusätzliche Places für jede physische Rezeptfamilie ableiten.
  Nah/Fern, Kontakt, Silhouette, Öffnungen, Material und LOD-Übergänge visuell prüfen.

## Abnahme
Jeder gelieferte Klassenwert ist zugeordnet; relevante gelieferte Attribute erreichen sein Rezept.
Eine fehlende Datenklasse bleibt explizit offen. Kein Schornstein mit Wohnfenstern, kein
Windrad aus einem bloßen Industriepolygon. Pflicht-Places und zusätzliche Rezept-Shots nach
AGENTS prüfen; Inventur und Schema allein sind keine optische oder Runtime-Abnahme.

Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators
Tags: buildings, python, esri, cga, modelling

# Python modelling covers the documented CityEngine capabilities

## Ergebnis und Ist
Nutzerauftrag: zuerst die prozeduralen CityEngine-Modellierfähigkeiten vollständig in Python nachbauen,
danach moderne/zeitgenössische Architekturrezepte bis zur vollständigen OSM-Klassifikations-/Kombinationsabdeckung ausbauen.
Einzelne konstruktive Python-Verfahren bestehen; ein allgemeiner vollständiger Modellierkern fehlt.
SDK-Katalog: 72 Operations-, 70 Funktions-, 10 Attribut- und 49 Bibliotheksfunktionsseiten;
Seitenzahlen zählen weder geprüfte Python-Funktionen noch Rezepte. Überladungen/Semantik einzeln inventarisieren.
30 Ghidra-Ziele und fünf verfügbare RPKs sind Referenzen aus [2400](2400_esri_capabilities_and_interior_primitives_have_executable_evidence.md).
Eigene Implementierung, keine Übernahme dekompilierten Fremdcodes; SDK 3.4 / PRT 3.3 ausdrücklich unterscheiden.
Besitzer: Gebäudeagent auf `buildings`, `.venv`, `test/experiments/buildings/`; native Integration bleibt offen.

## Verträge und Umsetzung
- Gemeinsame Python-Geometriefunktionen und direkte ausführbare Python-Rezepte; gemeinsame Form-/Material-/Scope-Typen.
  Keine zusätzliche JSON-Regelschicht erforderlich. Reports/Cache-Speicherung bleiben getrennte Ausgabeverträge.
- Lokale/Weltachsen, metrische Scope-Größen, Polygonringe/Höfe, Bounds, Material/UV/Normalen und Herkunft explizit halten.
  Transformationen, Komponentenzerlegung, Split/Repeat, Instanzen und Insert teilen diesen Vertrag.
- Numerische/String-/Array-/Zufalls-/Geometriefunktionen und Regelattribute anhand tatsächlicher SDK-Semantik abgleichen.
  Zufall deterministisch pro Quelle/Regel; fehlende/ungeeignete Operationen mit konkretem Grund ausweisen.
- Jede dokumentierte Fähigkeit erhält einen tatsächlichen Status und passende Referenz/Gegenfälle;
  API-Name, gelesene Dokumentation, Ghidra-Pfad, PRT-Ergebnis und eigene geprüfte Funktion sind getrennte Belege.
  Weitere nachgewiesene Modellierfähigkeiten erweitern den Katalog; unbekannter Umfang bleibt offen.
- Vorhandene GEOS-/Manifold-/Dach-/Extrusions-/Mesh-Verfahren wiederverwenden; kostenbestimmende Alternativen
  auf denselben echten Eingaben und Qualitätsanforderungen vergleichen. SIGGRAPH/Primärquellen vor neuen Verfahren.
- Durchgängige Python-API: Quelle/Parameter → Modellieroperationen → vollständig ergänzter Rohling → Details/Bild.
  Eingabe-/Cachetrefferkosten trennen, Detailbedarf vor Expansion bestimmen; Logik/Kollision unabhängig vom Render-LOD.
- Für jede tatsächlich gebaute Fähigkeit sichtbare kleine 2D-/2.5D-/3D-Belege und geeignete echte Grundrisse liefern;
  interne Arbeit braucht den fehlenden Feature-Vertrag oder einen gemessenen Engpass.

## Rezeptausbau danach
[2440](2440_building_recipe_families_have_distinct_architecture_and_examples.md) besitzt moderne/zeitgenössische Architektur und passende Zuordnung.
Alle lieferbaren Klassen/Unterklassen und gültigen Kombinationen aus Nutzung, Form, Höhe/Geschossen, Dach,
Material, Parts und räumlichem Kontext müssen mindestens ein passendes Rezept treffen.
Teil-/Geschossnutzung bleibt lokal; gemischte Angaben komponieren passende Programme, widersprüchliche Angaben erhalten ihre Herkunft.
Allgemeiner Unknown-Ersatz zählt nicht als spezifischer Treffer für bekannte Sonderklassen.
Rezeptfunktionen gemeinsam pflegen, Familien/Varianten/Instanzen getrennt zählen; Zuordnung plus optischer Bildbeleg erforderlich.

## Abnahme und nächster Block
Vollständige Fähigkeitszuordnung mit offenen Lücken; ausgeführte Geometrie-/Scope-/Materialverträge gegen SDK/PRT prüfen.
Reproduzierbare kleine Kontrollkörper, echte OSM-Grundrisse, ungültige/degenerierte Gegenfälle und gemessene Kosten.
Nur aktuelle Bildbelege in den vier Showcase-Ansichtsebenen; keine vorweggenommene native LOD-/Geräteabnahme.
Als Nächstes gemeinsame Shape-/Scope-Semantik samt den bereits untersuchten Transform-/Split-/Comp-/Extrusionspfaden abgleichen;
den ersten vollständigen öffentlichen Python-Vertrag an einem sichtbaren Modellierbeispiel integrieren.

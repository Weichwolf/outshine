Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2111
Depends:
Area: generators, render
Tags: vegetation, python, taxonomy, reference

# Worldwide plant forms cover the user-required taxa

## Ergebnis und Besitzer
Nach vollständigem SpeedTree-Funktionsnachbau (2348) auf Nutzerauftrag mindestens alle Pflanzen aus
[List of plants by common name](https://en.wikipedia.org/wiki/List_of_plants_by_common_name)
abdecken; die Liste ist Mindestumfang, kein Ersatz für weltweite plausible Ökologie.
2111 besitzt den Gesamtgenerator, 2346 Organ-/Pflanzenrezepte, 2347 LOD-Produkte.
Wuchsformen weltweit: Bäume, Sträucher, Kräuter/Blütenpflanzen, Gräser/Bambus,
Farne, Palmen, Kletterer, Sukkulenten und Wasserpflanzen einschließlich Mammutbäumen.

## Quellen und Umfang
`plant_targets.json`: 902 Namenseinträge, Revision 1376401028; 903 Quell-IDs geprüft, eine leer.
`plant_scope.py` übernimmt Namenseinträge/Bildunterschriften; Navigation getrennt. Keine Abdeckungsfreigabe.
Datierter Listensnapshot liefert Zielnamen/Zuordnungen; Synonyme und mehrdeutige
Trivialnamen erhalten. Schreibfehler oder strittige Zuordnungen nicht still als Taxonomie übernehmen.
Wissenschaftliche Namen/Gattungen durch passende botanische Primärquellen ergänzen.
Eine Namensliste oder ein generisches Mesh belegt keine abgedeckte Pflanzenform.
Eigene Rezepte dürfen plausible Arteninterpretationen liefern; passende Blätter,
Anschlüsse, Verzweigung, Wuchsform, Silhouette und Materialien bleiben prüfpflichtig.

## Visuelle Abnahme je Pflanze
Jede Pflanzenprobe optisch gegen passende botanische Bildreferenzen prüfen.
Nahdetail: Blattform/-rand/-größe, Blattstellung, Stiele und Nadeln/Blüten bei Bedarf.
Pflanze: Alters-/Wuchsform, Verzweigung, Silhouette und charakteristische Organe.
Gruppen/Bestände/Fernprodukte: Deckung, Licht, Schatten und konsistente Herkunft.
Referenz, Vergleichsbild, Betrachtung und verbleibende Abweichung pro Pflanzenrezept angeben.
SpeedTree darf als Referenz-Oracle dienen; botanische Quellen prüfen die jeweilige Pflanzenform.
Fotos dienen nur dem Vergleich; eigene prozedurale Geometrie/Materialien erzeugen.
Keine Freigabe durch bloß erfolgreiche Erzeugung oder einen lateinischen Namen.

## Aktueller Auftrag und offene Lücke
Quercus/Fagus/Betula besitzen erste junge Formrezepte und getrennte LOD-Proben.
Bildreferenzen und tatsächliche optische Vergleiche vor einer Formfreigabe ergänzen.
Übrige Zielpflanzen, artspezifische Bestände/Fernprodukte, Wind und native Kosten bleiben offen.
Showcase zeigt je LOD mehrere Gattungen mit lateinischen Namen und expliziten Lücken;
immer nur aktuelle Produkte, kein Auto-Reload. Laufzeit-/SpeedTree-Gates bleiben ungeschlossen.

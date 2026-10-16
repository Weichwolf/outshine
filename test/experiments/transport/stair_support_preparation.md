# Gemeinsame Pfeilervorbereitung und direkte Verkehrsdistanzen (#434)

[PR #496](https://github.com/Weichwolf/outshine/pull/496) liefert tatsächliche
Pfeiler, benötigt aber erheblich mehr Aufbau-/Prüfallokationen als fertige
Geometrie. Dieser Versuch erhält dieselben ganzen Quellen, Stationen, Körper,
Kontaktkappen und Darstellungsarrays. Kein neuer sichtbarer Inhalt, keine
weicheren Qualitäts-/Zeit-/Speichergrenzen und keine native/API-/Cacheänderung.

## Vergleich und Entscheidung

`per_source` erhält die frühere Vereinigung aller fremden Verkehrsflächen je
Brückenquelle und je Präzision sowie einen getrennten Konstruktions-Builder je
Brücke. Ein kleiner RegionMask-Adapter führt dieselbe direkte Abstandsanfrage aus;
er ist nur die Vergleichsschnittstelle. Alle bisherigen stationären/verschobenen
Versuche und die unabhängige ganze Abnahme bleiben gleich.

`shared` bereitet tatsächliche Verkehrsdreiecke in Double/Float32 genau einmal
als zwei STRtrees auf. Die Abfrage liefert alle Dreiecke im echten Abstand von
höchstens 0,2 m; erst danach entfernt die unveränderte Quellmaske ausschließlich
eigene Flächen. Ein gemeinsames Face mit einem weiteren Besitzer bleibt fremd.
Damit entspricht die Anfrage dem Abstand zur Vereinigung aller fremden Flächen,
ohne diese vorher geometrisch vereinigen zu müssen. Keine Mittelpunktprobe,
kein polygonaler Puffer und keine Auswahl eines höchsten/nächsten Stockwerks.

Ein Konstruktions-Builder behält Terrain und die einmal ausgewählte gemeinsame
obere Flächenmenge. Gemeinsame Quellfacetten werden über ihre ursprünglichen
Face-Indizes einmal ausgewählt, nicht je Besitzer dupliziert. Unterschiedliche
überlappende Kontaktfelder bleiben mehrdeutig und werden abgelehnt. Die spätere
Abnahme verwendet weiterhin jede ursprüngliche Quell-Unterseite getrennt:
eine Stütze unter einer anderen Quelle kann niemals durch die gemeinsame
Konstruktion genehmigt werden. Kontext/Indices leben nur während des endlichen
Plans; keine neue persistente Cache- oder Rezeptverwaltung.

Erste lokale SIGGRAPH-Referenz bleibt Chen et al. 2008,
[Paper](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
S. 2 §3/Fig. 2 für getrennte Plan-/Geometriestufen, erneut optisch geprüft.
Das Paper liefert keinen behaupteten Pfeiler-/STRtree-Algorithmus. Maßgebliche
Bibliotheksquelle ist die [offizielle Shapely-2.2.0-Dokumentation](https://shapely.readthedocs.io/en/stable/strtree.html#shapely.STRtree.query):
Abfrage über zweidimensionale Bounds und das verlangte geometrische Prädikat,
Z-Werte werden ignoriert. Die bewusste vollständige Projektion fremden Verkehrs
ist unsere konservative Fachentscheidung. Kein Fremdcode kopiert.

## Abnahme und Grenzen

Alte Körper-/Kappenarrays sind feste Vergleichsprodukte, keine neu erzeugten
Orakel. Beide Stationenstrategien müssen sie vollständig exakt reproduzieren;
der gespeicherte Reader prüft außerdem wirkliche Quellenkontakte, Float32,
Fremdverkehr und alte Darstellungspräfixe. Eigene/mehrfach besessene/fremde Faces,
Abstandsgrenze, geänderte Maskeneingaben, unzugeordnete Faces, zu große Indices,
mehrdeutige Ebenen, falsche Quell-Unterseite und leerer Plan besitzen Gegenfälle.

Ein erster Versuch nur mit gemeinsamem Builder und bisherigen Vereinigungen
zeigte kaum Zeitgewinn. Der dominierende vermeidbare Aufwand sind die erneuten
geometrischen Vereinigungen; die räumliche Anfrage vermeidet diese Arbeit.
Das ist keine Herleitung globaler Optimalität. Tragfähigkeit, weitere Quellen,
Änderungs-/Reservationsverträge und native Integration bleiben wie im
[Pfeilervergleich](stair_support_comparison.md) offen. Keine Welt-/Frame-/A18-/RTX-Abnahme.

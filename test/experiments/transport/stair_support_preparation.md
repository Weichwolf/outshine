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


## Eingefrorene Ergebnisse

Messungen auf `ea6edb4bf9d7f011464fb36e13761553ded1d4fc`. Danach ergänzt
`044c8a32b` die Größenprüfung vor temporärer Array-/Geometrievorbereitung;
abschließender Masterabgleich `5e0c0c819a314774067068388bb8cc21cecdb921`.
31 Kontrollen in 0,358 s, terminal OK; frische Erzeugung beider Strategien und
unabhängiger Leser bestanden. Alle vier NPZ-Produkte (beide Körper/Kappen,
Vorher-/Nachherdarstellung) bytegleich zum originalen PR-#496-Stand. 66 feste
und 70 verschobene Pfeiler, sämtliche Quell-/Kontakt-/Float32-/Verkehrsgrenzen
und lokalen Auslassungen erhalten. Größenlimits werden vor Maskenkonversion
und vor Aufbau der Verkehrsdreiecke geprüft, ohne höhere Ressourcenbudgets.

Die vier ursprünglichen tatsächlich geöffneten 720p-Bilder bleiben passende
Referenzen: aktueller kompletter Mesh-Payload bytegleich, alle Kamera-/Licht-/
Profilparameter außer Dateipfaden exakt, ursprüngliche Receipt-/PNG-/Helper-
Hashes erneut geprüft, alle Bilder vollständig decodiert. Beide Nachherbilder
nochmals geöffnet. Kein neuer Renderlauf oder Bildgewinn aus dieser Optimierung
behauptet; die bestehende Galerie wird nicht dupliziert.

Gleiche 70 Pfeiler, je sieben normale vollständige Pläne plus Warmplan, eigener
Worker je Variante und normale Priorität; Geometrie und ganze physische Abnahme
bleiben innerhalb der Zeit. Import/Decode, Packvergleich und Datei-IO außerhalb.
Alle gepackten Körper-/Kappenarrays in jedem Lauf exakt gegen den unveränderten
unabhängigen Export. Keine eigenen Builds/Renderer gleichzeitig; gemeinsamer
Host nicht von fremder Fachlast isoliert. i5-1135G7, zwei physische/vier logische
CPUs, 16 GiB RAM; keine Geräte-/Frameabnahme oder allgemeiner Speedup-Faktor.

| Ganze Planung und explizite Vorbereitung | Je Quelle | Gemeinsam |
|---|---:|---:|
| Gültige Pfeiler |70|70|
| Phase, Median ms |2925,325|1692,716|
| Prozess-Peak B |152596480|125587456|
| Konstruktions-Builder |5|1|
| Deren vorbereitete Terrainfacetten |83550|16710|
| Deren obere Facetten |1201|1192|
| Verkehrsdreieck-Einträge beider Präzisionen |38188|8080|
| Geometrische Verkehrsflächen-Vereinigungen |10|0|
| Geteilte Verkehrsindices |0|2|
| Logischer XY-Payload expliziter Dreiecksindexierung, B |1833024|387840|
| Eigene Maskenarraybytes der Verkehrsindices |0|64640|

Gleiche Stationen/Versuche/physische Kontaktpaarprüfungen und Ausgabebytes.
Neun mehrfach besessene obere Facetten werden nicht erneut aufbereitet;
66840 wiederholte Terrainfacetten und zehn Verkehrsunions entfallen.
Die XY-Bytes bezeichnen den logischen Payload der expliziten NumPy-Auszüge,
nicht alle Kopien oder native Backing-Allocations. Interne Gesamtkopien bleiben
unbeobachtet. Das Darstellungsarray und dessen 7 078 656 B Concatenate-/Cast-
Zielpuffer bleiben unverändert; reine Erzeugungs-/Prüfarbeitsersparnis.

Getrennte Memray-1.20.0-Phasen inklusive vollständiger Vorbereitung, Konstruktion
und physischer Abnahme: alle Arrays erneut exakt, gleiche Zeit-/Speichergrenzen.
Der aktuelle passende Verfahrensvergleich ergibt:
10360760/3800497 beobachtete Allokationsereignisse,
1408656961/523552353 angeforderte B,
46086540/20853436 B getrackter Heap-Peak. Profilierte Phasen 6754,708/2987,916 ms,
Prozess-Peaks 163721216/131969024 B. Python-Allocatorereignisse und native Stack-
Traces aus; native Allokationen bleiben erfasst. Kumulative Anfragen sind keine
lebenden RAM-Bytes und profilierte Phasen keine unverfälschten Laufzeiten.
Die verbleibende physische Prüfung ist weiterhin erhebliche Arbeit; kein
unbelegtes globales Optimum oder Cache-/Weltladegewinn aus diesem kleinen Fall.

Abschließende Produkte/Leser/Bildbindung:
`/tmp/outshine-transport-stair-support-prep-final-044c8a32b/`;
Kosten/Allokationen: `/tmp/outshine-transport-stair-support-prep-cost-ea6edb4bf/`.
Ursprüngliche Körper/Bilder/Profilierungsgegenfälle bleiben unter dem im
Pfeilervergleich angegebenen ursprünglichen Pfad erhalten.

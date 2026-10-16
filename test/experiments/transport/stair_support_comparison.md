# Ganze Pfeilerkontakte unter dem unveränderten Wiener Verbund (#434)

Die langen Rad-/Fußbrückendecks des [Südstands](stair_cycle_south_comparison.md)
haben bisher keine sichtbaren Pfeiler. Derselbe installierbare
[SupportVolumeBuilder](../../../lib/supports.md) ergänzt geometrisch ganze
Stützen zwischen wirklichem Terrain und tatsächlich ausgewählten Brückenunterseiten.
Der vorherige Verkehrs-/Terrainkörper, seine Quellen, Profile und Materialien
bleiben unverändert. Kein SDK, Fremdcode, neue gemeinsame/lib/native/Reservations-API.

## Gestaltung, Gegenfälle und ganze Kontaktprüfung

Alle als Brücke deklarierten Quellen erhalten ab 6 m Innenstationen im 12-m-Rhythmus;
kurze Quellen ohne Innenstation behalten ihren ausdrücklichen Grund. Eine Stütze
misst 0,8 m längs und 0,45 m quer zur tatsächlichen lokalen Achse. Die feste Variante
verwendet nur die Sollstation, die zweite versucht 0/−2/+2/−4/+4 m. Alle tatsächlichen
anderen ausgewählten Verkehrsflächen einschließlich Treppen und Brücken sind
konservativ mit 0,2 m Abstand ausgeschlossen, unabhängig von ihrer Höhe. Das wird
für ursprüngliche und tatsächliche Float32-Geometrie separat geprüft. Weitere
Pfeiler dürfen sich ebenfalls in keiner Präzision berühren. Kein Hindernisnachweis
für die weiterhin nicht zugelassenen übrigen Quellen.

Der Bibliothekskern zerlegt den ganzen Fußabdruck gegen beide ursprünglichen
stückweise affinen Kontaktfelder. Ein Höhenwert am Mittelpunkt oder an den vier
Ecken reicht nicht: ein Innenpeak bleibt echter Gegenfall. Ganze obere/untere
Kappen und senkrechte Randwände müssen exakt zum gespeicherten Körper gehören;
positive Höhe, geschlossene orientierte Kanten, zyklische Vertexlinks, positive
Volumina und unveränderte Flächenorientierung bleiben in Double/Float32 Pflicht.
Ein Double-gültiger schmaler Körper kann nach Float32-Rundung kollabieren und
wird dann abgelehnt. Die Wahl verlangt 0,1001 m Double-Höhe als Reserve; die
Abnahme verlangt mindestens 0,1 m in beiden tatsächlichen Präzisionen.

Der Reader bestimmt den maximalen ganzen Double-Kontaktfehler e mit beiden
vorzeichenbehafteten Flächenabständen. Lokale Originalkontaktflächen müssen den
Fußabdruck samt tatsächlichem XY-Rundungsradius abdecken, damit ihr größter
affiner Gradient L dort eine Lipschitz-Grenze ist. Aus den tatsächlich gerundeten
Vertexkoordinaten folgt derselbe ausreichende Umschlag wie beim Terrain:
`e + dz_cap + dz_reference + L * (dxy_cap + dxy_reference)`.
Die tatsächlichen beiden Float32-Flächenabstände müssen darunter bleiben
(numerischer Vergleichszuschlag 1e−10 m); die ganze tatsächliche Kappe muss
zugleich innerhalb der gerundeten Referenz liegen. Kein exakter Nullkontakt oder
Null-Überlappungsvolumen aus einer Vertex-/Solver-/Prozessmeldung abgeleitet.

SIGGRAPH zuerst: Chen et al. 2008, [lokales Paper](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
S. 2/Fig. 2: Graph, Profile und Geometrie bleiben getrennte Stufen; optisch bereits
im [Support-Vergleich](support_library_comparison.md) geprüft. Ergänzend Galin et
al. 2010, [Paper](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
S. 8/Fig. 17–18, für diesen Versuch erneut optisch geprüft: Terrainübergänge und
getrennte Straßen-/Brückenrezepte. Hier werden keine Tragwerksformel oder ganze
Paper-Pipeline übernommen. Maße, Stationssuche und Rundungsabschätzung sind
explizite eigene Gestaltungs-/Prüfentscheidungen; keine Statik.

## Arbeit, Reproduktion und offene Abnahme

[Aufrufe](README.md). Höchstens 128 Stationen/640 Versuche, kooperative 8 s pro
Strategie einschließlich Vorbereitung/Prüfung; Kern und einzelne Abstandsanfragen
haben eigene kooperative 2-s-Limits, so dass ein bereits laufender Schritt die
äußere Zeit überschreiten kann. Größen-/Paargrenzen gelten zusätzlich; ganze
Prozesse 120 CPU-s/3 GiB. Abbruch erhält ein gültiges Teilprodukt mit ausdrücklichen
Auslassungen. Keine unbegrenzte Wartezeit und kein erneuter Netz-/Terrainfit.

Geometrisch belegt ist nur der gewählte endliche Verbund. Fundamente/Lasten,
Bodenkennwerte, statisch sichere Spannweiten, Fahr-/Gehhüllen, Handläufe, übrige
Quellen und native Übernahme bleiben offen. Stationslücken sind entlang der
Originalachse einschließlich ihrer Enden gemessene Gestaltungsmetriken; Enden
sind damit noch keine nachgewiesenen Widerlager. #434 bleibt aktiv.

# Ganze lokale Fußrampen an tatsächlichen Straßenrändern (#385)

## Ergebnis und verbleibende Lücke

Eine tatsächliche Wiener Rampe verbindet die bisher12cm hohen Straßenränder mit
der vorhandenen unteren Verkehrsfläche: Kernbreite1,20m, Länge5,256098m,
Volumen2,107237m³, ganze seitliche Anschlüsse3,899470m. Beide Kernenden sind
über ihre ganze Breite bündig, maximaler gemessener Double-Höhensprung0m.
Vier ursprüngliche Randkörper37/134/282/362 lokal atomar umgebaut;456 andere
Randkörper unverändert. Straßen/Boden/Schienen/Brücken/Pfeiler/Portale und deren
Beläge/Normalen bleiben exakte Basisprefixe. Keine neue globale Quellfreigabe.

Von drei passenden lokalen Vorschlägen bestehen zwei den vorhandenen Kontakt-
Neuaufbau nicht (`nonmanifold surface edge`). Sie erhalten den vorherigen lokalen
Stand. Der unabhängige Solid-Gegenweg besteht dort ebenfalls die ganze
Kappenzuordnung nicht; daraus wird kein allgemein falscher Backendalgorithmus
behauptet. Abdeckung und Robustheit weiterer Umbauten bleiben ausdrücklich offen.
Dieses Ergebnis belegt keine vollständige Querung, Fußroute, Gebäudekollisionsfreiheit
oder Gebäudezugänge. Plan-/Reservation-/Verbraucher-/native-/LOD-/Geräteabnahme fehlen.

## Quelle, Rezept und örtliche Form

Verwendete explizite Bodenquellen, fast orthogonale befestigte urbane Straßen
und Fußweg-/Fußgänger-/Pfadrezepte bestimmen die Auswahl. Bestehende Zugangs- und
Richtungssemantik wird gelesen, private/ungeklärte Gehzugänge nicht hochgestuft;
Treppen/Innenräume/fremde Ebenen ausgeschlossen. Ohne Zugangstag bleibt die
physische Auswahl eine ausgewiesene Rezeptannahme. Die gültige Fußquelle ist
`openfreemap:14/8938/5680:transportation:50756850:87`, Straßenquelle
`openfreemap:14/8938/5680:transportation:46884580:15`.
Quellattribute und vollständige bisherige Nutzungsbilanz bleiben erhalten.

1.290 geeignete Fußkanten und795 gerade urbane Kanten;4.172 Paar-/Seitenversuche,
drei Kandidaten. Vollständige Quelle-/Paar-/Auslassungslisten im Bericht. Grenzen:
20.000 Versuche,64 Planvorschläge,8 optionale Rampen,60s Umbauphase,
262.144 geänderte Randfaces,1.024 Footprintkoordinaten,Prozess3GiB/240CPU-s.
Ein einzelner begrenzter Kernaufruf kann das Wandzeitlimit überlaufen.

Eigene stückweise affine Flächen: zentraler Aufstieg über1,9m,2,4m breite
seitliche Flügel, flache12cm-Anschlusszone bis zum tatsächlich gemessenen äußeren
Rand plus5cm, danach2,4m Rückführung auf den unteren Fußweg. Bei diesem Fall
endet die flache Zone bei2,856098m; die alte Randfläche läuft um den Querungskorridor
weiter als eine starre2m-Schablone. Mindestens1,8m tatsächlicher Anschluss je Seite,
danach vollständige Prüfung aller entstandenen Interface-Linien. Maximalgrade
des gesamten tatsächlichen oberen Meshes0,0571341 bei Rezeptgrenze0,12.
Kernbreite=80% der gelieferten Wegbreite; keine Mindestbreite ganzer Routen behauptet.
Diese Maße sind prozedurale Parameter, kein Norm-/Vermessungsnachweis.

Tatsächliche vorhandene Verkehrs-/Randgrenzen beschneiden die Schablone; die
gewählte Fußquelle erhält einen um1cm erweiterten Rezeptkorridor für die lokale
Anpassung an gespeicherte Quell-/Meshgrenzen. Andere Verkehrsflächen werden dabei
nicht entfernt. Ganze tatsächliche Straßen-/Fußwegmünder und Bauwerksfreiräume
werden unabhängig geprüft. Keine Punkt-/Breitenschätzung als alleiniger Kontaktbeleg.

## Konstruktion, Gegenwege und unabhängige Prüfung

Zuerst Chen et al., SIGGRAPH2008 Fig2 (Graph/Form), ergänzend Galin2010 Fig17
(Querschnitt). Für den Solid-Gegenweg Zhou et al., SIGGRAPH2016
[Mesh Arrangements](../../../doc/references/buildings/geometry/2016-mesh-arrangements-for-solid-geometry.pdf),
S.2/Fig3–4 tatsächlich optisch geprüft: Solid-/Topologie- und Float-Konvertierung
getrennt prüfen. Kein Anspruch, Manifold implementiere den exakten rationalen Paperweg.

Der bestehende SupportVolumeBuilder erzeugt die konforme Kappenunterteilung mit
einer ausschließlich temporären tieferen Hilfsebene. Diese Hilfsprismengeometrie
wird verworfen. Die tatsächlich exportierte untere Kappe liegt vollständig auf
dem ursprünglichen feinen Boden; obere Kappe trägt das geprüfte Rampenfeld.
Top/Bottom-Koordinaten bei Nullhöhe werden exakt auf gemeinsame Körpervertices
abgebildet; kollabierte Indexfaces entfernt. Keine schwebende Mindestdicke oder
exportierte unterirdische Ersatzunterlage. Bisherige positive Support-API unverändert.

Randumbau als Transaktion: alle betroffenen Körper/ganzen Kontakte vorbereiten,
erst nach vollständigem Bestehen gemeinsam veröffentlichen. Ein injizierter
Fehler nach dem ersten echten Solid-Schnitt erhält nachweislich alle alten Zustände.
Gegenweg verwendet das vorhandene öffentliche Buildings-Primitiv
`subtract_prisms/PrismVolume` samt expliziten Topologiewurzeln und Source-/Cutter-
Zuordnung; dessen API unverändert. Optionales bestehendes `solids`-Paket
Manifold3D3.5.4 nur extern für Forschung/Kontrollen, kein neuer Engine-/SDK-Verbrauch.

Gleiche drei Footprints, ursprüngliche Zustände und vollständige Qualitätsbedingungen:
beide Methoden1/3 gültig. Gültiger Fall Kontakt0,389s/988 geänderte Randfaces,
Solid0,570s/1.028 Randfaces; Rampenkörper jeweils72Vertices/140Faces.
Das sind je einzelne vollständige vorbereitete Edit-/Auditläufe, keine statistische
Geschwindigkeitsgrenze. Gewählt bestehender Kontaktweg: gleiche belegte Abdeckung,
weniger Faces und kein zusätzliches Backend für den tatsächlichen Randumbau.

Unabhängiger Leser konsumiert gespeicherte Kappen/Körper statt erneut mit dem
Rampenbuilder zu bauen. Er prüft alle affinen Überlappungen gegen das analytische
Feld, ganze Bodenkontakte, Cap-/Shellbesitz, geschlossene positive Volumen,
volle tatsächliche geplante Interfaces, Nullhöhenmünder, Neigungen, Float32-Faces,
ursprüngliche unveränderte Körper, ganze lokale Subtraktion und gegenseitige
Überdeckungsfreiheit. XY-Toleranz1e-8m und Height1e-7m bleiben metrisch;
Wandorientierung prüft die normierte Normale, keine mit Länge wachsende Fläche.

Maximalfehler Double: Boden2,8422e-14m, ganzes Rampenfeld1,3652e-14m,
Volumen7,5496e-14m³, Mündungen0m und verbleibende Randinterfaces0m.
Float32 prüft separat keinen kollabierten/verkehrten Bodyface; dieselbe
Kontaktgenauigkeit nach Float32-Konvertierung ist damit nicht belegt.
460 ursprüngliche Randbesitzer vollständig erhalten. Neue Profilgeometrie
48.498Vertices/95.320Faces, gesamte Szene433.426Vertices/782.844Faces:
netto nur+4Vertices/+4Faces gegenüber #372.

85 Quell-/Bibliothekskontrollen zunächst terminal0,0,364s, darunter sieben neue Kontrollen
für bündige geschlossene Körper, gleiche Schweißwege, verborgenen inneren
Feldfehler, ungeteilte Nullhöhen, Bauwerkskonflikt, privaten Zugang und atomaren
Abbruch. Zusätzlich drei tatsächliche Kappeninputs unter
`fixtures/foot-ramp-vienna.json` versioniert; ihr neuer Kappenkontrollfall ersetzt
ausdrücklich nicht die ursprüngliche Ganzflächen-/Rand-/Zugangsabnahme.
Final mit diesem zusätzlichen Kontrollfall86 Kontrollen terminal0,1,178s;
16 tatsächliche Blenderkontrollen terminal0,5,904s. Ruff/Format für14 betroffene
Pythondateien, alle betroffenen Funktionen≤120Zeilen, `git diff --check` und
`make lint-docs` bestanden.

## Kosten und tatsächliche Bilder

Produzent/Leser `65776c4cf80a4b489e7cd6b553bdfa65390d12ab`, Kosten/Renderer
`5e8ba0577` (nur zusätzlicher Kostenworker). Saubere eingefrorene Quellen;
Linux/Python3.13.5,NumPy2.2.4,Shapely2.2.0/GEOS3.14.1,einBLAS-Thread.
Gesamt32,3758s/565,602MiB Peak; tatsächliche Umbauphase0,9072s.
Vorbereitung, Auswahl/Gegenvergleich, Export und Body-Audit sind nicht alleinige
Konstruktionszeit. Keine Frame-/Gerätekosten daraus abgeleitet.

Zusätzlicher begrenzter Schweißvergleich nur auf der einen tatsächlichen
gespeicherten Rampenkappe:78 Eingabevertices/152Faces/3.696B → exakt gleiche
72Vertices/140Faces/3.408 eigene Ergebnisbytes, einschließlich Nullhöhen-Kanten.
Sieben normale Wiederholungen, vorige Ausgabe freigegeben, Memray1.20.0 separat:

| Nur Schweißphase | Vektorisiert | Skalares Dictionary |
|---|---:|---:|
| Normaler Median | 0,411ms | 0,507ms |
| Allokationsereignisse | 531 | 1.861 |
| Kumulativ angeforderte Bytes | 96.453B | 183.405B |
| Getrackter Peak Heap | 24.875B | 33.040B |
| Prozesspeak inkl. Eingaben | 92.229.632B | 91.738.112B |
| Instrumentierte Zeit | 19,150ms | 19,586ms |

Ereignisse enthalten realloc, kumulative Bytes sind kein Live-Speicher. Interne
vollständige Kopier-/Builderallokationen ungemessen; dieser kleine Teilfaktor gilt
nicht für Quellwahl, Körper-/Randaufbau, Kontakte oder Rendering. Explizite neue
Darstellungskopien6.863.040B indexiertesXYZ/3.431.520B Ecknormalen; kompletter
Payload66.766.608→66.766.992B. Keine SDK-/Fremdcode- oder neue Runtime-Abhängigkeit.

Frische regionale1280×720-Vorher/Nachherbilder und lokale1920×1080-Prüfung geöffnet.
Gleiche Paar-Kamera/Licht/Objekt-ID;1100m Bildspanne um den tatsächlichen Umbau,
keine Nahansicht/Überhöhung. Gegenüber #372 ist dies ein anderer regionaler
Ausschnitt, nur das neue Paar ist ein unmittelbarer Bildvergleich. Blender5.2.2/EEVEE,
Mesa llvmpipeLLVM19.1.7,16Samples, bestehender Beton-/Tageslichtwert unverändert.
26/921.600Pixel verändert(0,0028%), maximal22/255, mittlereRGB-Abweichung0,000154/255.
Die Rampe ist in dieser Höhe nur ein kleines Detail. Kein großer Bildgewinn
behauptet; Gesamtbild weiterhin schematisch, endlicher Quellfensterrand sichtbar.
Offline-Render720p19,190→18,960s/Peak2.364.736→2.407.572KiB;
1080p27,825s/2.486.408KiB. Bilder lokal, keine Galeriepublikation.

## Pins und Reproduktion

Originale aus [#364](curb_comparison.md#eingaben-und-frische-belege) und
[#372](sidewalk_comparison.md#eingaben-und-reproduktion) unverändert erhalten.
Ergebnis `/tmp/outshine-transport-ramps-385-result-65776c4cf`, Kosten
`/tmp/outshine-transport-ramps-385-cost-5e8ba0577`; vollständige Quell-/Paargründe,
gewählte Dreiecke/Footprints, alte Besitzer, tatsächliche Pakete, verification.json,
Renderreceipts und before/after/after-1080/shot.png. Originalinputs bleiben nötig.

| Artefakt | SHA256 |
|---|---|
| Tatsächlicher Vergleich | `300f5c890768e50a80919c5897ae6c27dbfd7f99f1c02faf82df94beda1327dd` |
| Feste drei Kappeninputs | `f2f344171e4e790603363f95be1d734d5c75c4cc2a2968a48a594e46316a99a5` |
| Tatsächliche Randkörper | `6e5f14d30318631621df922d150673559b76ef2143d6165cc39aefd4988fa1e7` |
| Tatsächliche Rampe | `b85cb280376075dcff7fa02b1e3d84c22a38cad1455f5ef7396838706215c73f` |
| Ganze neue Szene | `4e4e26d902c5ba0aab66e3d49e4845ad98343c112e44ad9cb2b9a0b239f17273` |
| Vorher720p | `db14e732a40b5b99975549a35f69086e46cbabd731bad68bcf8cd0ac4e48d289` |
| Nachher720p | `0678ef2e85888ddfee5a5518b3cbd323075f33f82d22ecb94bf47408ed453bff` |

[Reproduktionsaufrufe](README.md#lokale-bündige-fußrampen-385) benötigen frische
explizite Tempziele. Logs `/tmp/outshine-transport-ramps-385-*-5e8ba0577.log`,
Generation/Leser mit65776c4cf-Suffix; optionale Pakete nur extern installiert.

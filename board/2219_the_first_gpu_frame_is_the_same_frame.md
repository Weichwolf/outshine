Type: bug
State: active
Parent: 2188
Depends: 2179, 2190
Area: render, SDL3, test
Tags: determinism, backend, measured

# The first GPU frame is the same frame

## Befund

`MipmappedChessRepeatsLinearPixels` misst 467 geänderte lineare Kanäle (maximal
0.00268555) zwischen erstem und allen folgenden Frames; Tiefe und alle späteren
Frames sind bitgenau. Ein verworfener Vorlauf-Frame macht den Test grün, ist aber
keine Reparatur. Mip-Copy-Reihenfolge, `SubjectDraw::CarryFrame` und GLSL `precise`
ändern die Ursache nicht.

`ScoreHowFarTheAirReaches` zeigt denselben Effekt nach zwei identischen
Deklarationen: 262 von 230400 RGBA8-Kanälen weichen ab. Seine Bandwerte wirken nur
gerundet gleich; die exakte Wiederholungsprüfung ist zu Recht rot. Streaming ist nach
`preload` abgeschlossen. Damit ist der Befund backendübergreifend am ersten GPU-Draw
oder an dessen vorheriger Ressourcen-/Pipelinestellung zu behandeln, nicht als
Atmosphären- oder Mip-Ausnahme.

Die Culling-Inspektion hatte zuerst 4 394 688 gegenüber 3 989 952 Indizes bei je
33 Batches gezeigt. Eine verworfene Untersuchung mit Tangenten-Kugelrechteck und
zwei zusätzlichen Pyramidentexeln hielt danach in allen Frames exakt 4 394 688
Indizes, die linearen Frames unterschieden sich dennoch um 404 Kanäle. Eine
vollständige All-Far-Pyramide ohne spätere Pyramiden-Updates ließ 400 Kanäle rot;
festes Jitter, keine History-Fortschreibung und unveränderter Linear-Targetindex
ließen 470 rot. Weder Culling-Ergebnis noch Pyramideninhalt oder Temporal-Fortschritt
sind die Ursache. ABeautifulGame trägt keine Alpha-Modi und `Shape` clustert ohnehin
nur opaque/masked Geometrie.

Temporäre Readbacks am selben Ort zeigen den Unterschied bereits in `SceneHdr`: 471
Kanäle (maximal 0.00268555) weichen ab; `SceneAerial` zeigt 470. Die Fehlerregion
bleibt die Schachgeometrie, während die Tiefe exakt bleibt. Damit liegen Atmosphäre,
Resolve und Display hinter dem ersten fehlerhaften Producer. Als Nächstes sind
Subject-Textur-/Materialresidenz, Uploadreihenfolge und die erste Samplerbindung zu
prüfen.

Die Kausalitätsproben grenzen den Producer weiter ein. Werden die vorhandenen Mappen
durch die regulären 1×1-Ersatztexturen ersetzt, wiederholt das texturierte Unlit-Draw
bitgenau. Beschränkt `SubjectResidency` alle Mappen temporär auf Basis-Mip 0, ist es
ebenfalls exakt. Mipmap-Nearest verringert den Fehler auf 15 Kanäle (maximal
0.0145264), lässt ihn also bestehen; lineare Mip-Interpolation verstärkt ihn nur.
Ein einzelner Transferbuffer/Copy-Pass für die gesamte Kette statt eines Submits je
Mip lässt 470 Kanäle rot. `SDL_WaitForGPUIdle` nach jedem vollständigen Upload ebenso.
Die Kette ist beim ersten Draw daher nicht bloß unfertig. Beide Eingriffe wurden
verworfen: der erste erhöht temporären Speicher ohne Nutzen, der zweite blockiert.

Alle Materialmappen werden gegenwärtig als dekodiertes
`R32G32B32A32_FLOAT` gehalten. Das bewahrt lineare Filterwerte, ist für mobile
Materialresidenz aber weder speicher- noch bandbreitenangemessen und koppelt die
Kette an F32-Samplerverhalten. Ein Formatpfad mit korrekter Transfer-/Mip-Semantik
für Farb- und Datentexturen ist nun der nächste überprüfbare Kandidat; er darf nicht
die Wiederholungsprüfung abschwächen.

Der erste Umstellungsschritt ist umgesetzt: Farbwerte liegen als
`R8G8B8A8_UNORM_SRGB`, Daten- und Normalmappen als `R8G8B8A8_UNORM` auf der GPU.
Die Mip-Kette entsteht weiter aus linearen Werten; Farbwerte werden erst vor dem
Upload sRGB-kodiert. Damit entfällt der F32-Filterfähigkeitsvertrag. Der gezielte
Test ist noch rot, aber nur mit 248 statt 470 Kanälen bei demselben Maximum. Das
belegt einen Teilbeitrag des F32-Pfads, keine Reparatur. `make lint` bestätigt nach
der Umstellung 0 clang-tidy-Befunde und 24/24 dokumentierte öffentliche Header.

Die erneute Nearest-Mip-Probe auf dem kompakten Pfad ergibt wieder genau 15 Kanäle
(maximal 0.0134277). Der verbleibende Basis-Samplingfehler ist damit vom F32-Format
unabhängig; lineare Mip-Interpolation erzeugt die übrigen 233 Abweichungen.
Auch Nearest für Minify und Magnify lässt dieselben 15 Kanäle. Der Rest liegt folglich
nicht in bilinearer oder Mip-Filterung, sondern im ersten texturierten Draw-/Pipelinepfad.
Ein temporärer GLSL-Pfad mit `textureLod(..., 0.0)` für alle sechs Materialmappen
behält die texturierte Variante und Bindungen, ergibt aber 247 Kanäle. Implizite
LOD-/Derivativwahl ist damit ebenfalls nicht ursächlich.
Ein weiterer GLSL-Versuch behielt Variantenschlüssel und alle Sampler, führte die
sechs Abfragen jedoch ausschließlich in einem zur Laufzeit nicht erreichten
Materialzweig aus. 244 Kanäle bleiben rot. Texelwerte und ihre tatsächliche Abfrage
sind damit nicht die Ursache; der erste texturierte Pipeline-/Descriptorpfad bleibt.

Lokaler Referenzstand: `../SDL` fa2c02b (3.4.16) kompiliert MSL über
`newLibraryWithSource(..., options:nil)`; `../SDL_shadercross` 1ff05be bietet für
SPIR-V→MSL nur die Ziel-MSL-Version, keine Präzisions- oder Compileoption. Die
erzeugte GLSL-Variante ist `texture2d<float>.sample`.

`SDL_GPU_DRIVER=vulkan` verweigert dieser Host mit `unsupported`; ein zweites
SDL_GPU-Backend ist hier nicht verfügbar und bleibt als externe Abnahme offen.
`xcrun metal` und `metallib` fehlen ebenfalls; ein lokaler Metallib-Versuch ist
ohne vollständiges Xcode nicht reproduzierbar.

## Lösung und Abnahme

1. Materialtexturen nach Farb- und Datenbedeutung in einen kompakten nativen
   GPU-Formatvertrag überführen. sRGB-Dekodierung, lineare Mipbildung, Filterung,
   Alpha und Normalvektoren müssen je Pfad ausdrücklich stimmen. Speicher-, Upload-
   und Samplingkosten gegen den heutigen F32-Pfad messen; Basis-Mip und mehrstufige
   Mappen getrennt wiederholen.
2. Erst- und Folgeframe müssen pro Stufe dieselben Eingänge, Clear/Load/Store-Zustände
   und Abhängigkeiten haben. Den fehlerhaften Producer mit einem vollständigen
   Ressourcenvertrag reparieren; keine Vorlauf-Frames, Uploadbatches, Idle-Waits oder
   Diagnoseausgaben behalten.
3. Falls Metal die Ursache ist, GLSL als Quelle behalten und einen reproduzierbaren
   backend-spezifischen Buildproduktpfad wählen, der die benötigte Compilersemantik
   ausdrückt. Keine handgeschriebene Vendor-Shaderquelle und keine Testfall-Ausnahme.
4. Dieselbe Wiederholbarkeitsabnahme auf mindestens einem weiteren SDL_GPU-Backend
   ausführen; Backend/Driver/Shaderprodukt protokollieren.

- [ ] Erster, zweiter und erneut deklarierter statischer Frame sind bytegenau;
      Mip-, Atmosphären- und Negativkontrollen bleiben fachlich unverändert.
- [ ] Nachweis trennt native Compiler-/Samplerabweichung von Enginezustand.
- [ ] Startupkosten, GPU-Synchronisation und Shaderprodukt sind gemessen; der
      Echtzeitpfad erhält keinen blockierenden oder versteckten Zusatzframe.

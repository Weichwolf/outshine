Type: bug
State: active
Parent: 2171
Depends:
Area: test, harness, render
Tags: khronos, measured

# Texture filter oracles compare the same pixel integral

## Befund

Beim Mip-Schritt in 2171 wurde der historische Blocker 1130 erneut gelesen. Dessen
ABeautifulGame-Wiederholungsprüfung ist nicht mehr über `make suite` enumeriert:
`make suite SUITE=khronos/glTF/ABeautifulGame` verweigert den vorhandenen Manifestfall.
`make corpus-render CASES=ABeautifulGame` rendert dagegen und vergleicht PNGs.

Baseline af20ca1f ohne Mips: 97.9320 % innerhalb 8 Codes, 735 abweichende Pixel,
Digest 04937f96 (`build/mip-chess-render-before.log`, Exit 2).
Mit flächenintegrierten Mips: 80.7462 %, 6843 Pixel, Digest 76fd0957; Wiederholung
identisch (`build/mip-chess-render-after.log` und `build/mip-corpus-repeat.log`).
Die bestehende 99.99-%-Schranke bleibt unverändert und rot.

ABeautifulGames Manifest beschreibt 1 Sample durch einen 0.01-Pixel-Boxfilter.
SimpleTexture/four-texels-per-pixel beschreibt dagegen 256 Samples über eine ganze
Pixelfläche und begründet ausdrücklich, weshalb Punktabtastung keine Mip-Filterabnahme ist.
Dieser integrierte Fall wird vom heutigen scenario_for mit
`ValueError: unsupported oracle world: factory` abgewiesen. Ein vorhandener Prüfauftrag
ist damit unerreichbar. Nicht durch einen anderen Grauwert oder einen größeren Grenzwert ersetzen.

## Lösung und Nachweis

Unreal/RAGE sind Vergleich für gefilterte Darstellung; die mathematische Referenz hier
ist das Integral des identisch deklarierten Signals. Vorhandene Blender-Provenienz und
Manifest nutzen, keine neue Engine-eigene Soll-PNG als unabhängiges Oracle ausgeben.

1. Factory-World aus der tatsächlich gemessenen Provenienz übertragen; vorhandene
   Default-Konstante nur verwenden, wenn deren Gültigkeit belegt ist. Kamera, Material-
   Closure, Farbraum und Belichtung ebenfalls identisch deklarieren. Diffuse ≠ MR mit
   verbliebener Specular-Lobe; diese Unterscheidung im Übersetzer prüfen.
2. Den vorhandenen four-texels-per-pixel-Fall durch Make ausführbar machen. Pixelintegral
   gegen Mip-Kette prüfen; fehlende Kette und Zweifarben-Rundung als echte Mutationen rot.
3. ABeautifulGame mit entsprechend deklarierter, unabhängig integrierter Referenz
   zusätzlich abnehmen. Die alte Punktreferenz mit ihrem Auftrag erhalten. Erst anhand
   der Messung entscheiden, welche bisherigen Checks tatsächlich falsch spezifiziert sind.
4. Wiederholbarkeit separat auf linearen Pixeln prüfen. Die neue native Konventionsfixture
   deckt Unlit/Farbmaps ab; beleuchtete Normal-/MR-Maps und bewegte Frames ergänzen.

- [ ] Integrierter vorhandener Fall läuft statt Translation-Exception.
- [ ] Gleiche Kamera, Beleuchtung, Closure und Integralpopulation belegt.
- [ ] Positive/negative Filterkontrollen und unabhängig gerenderte PNGs geprüft.
- [ ] Schachbrett wiederholt lineare beleuchtete Frames; verbleibende Fehler ursächlich benannt.

Keine Änderung an Vendor-Manifesten, Oracles oder Akzeptanzschranken im bisherigen Mip-Schritt.

## Native Quantisierung

`NativeMipImagesReachTheRenderer` misst aktuell maximal 0,00244141 gegen die geforderte lineare
Hälfte. Der Mip-Upload speichert Base-Colour korrekt als `R8G8B8A8_SRGB`: `sRGB(0,5) × 255 =
187,516`, somit Code 188. Dessen Rückwandlung ergibt 0,502886 und liegt schon vor der
Renderzielquantisierung 0,002886 von 0,5 entfernt. Die Schranke 1e-5 kann für diesen Vertrag
nicht erfüllt werden. Sie durch ein analytisches Quantisierungsoracle ersetzen: erwarteten
sRGB-Code und erlaubten Speicher-/Readbackfehler herleiten; mutierte Gamma-Mips und falsche
Mipmap-Auswahl müssen weiterhin rot werden. Das ist eine Korrektur der nachweislich falschen
Spezifikation, keine gelockerte Bildabnahme.

Der Native-Mip-Test verwendet nun dieses quantisierte Soll und eine aus dem R16-Readback
abgeleitete Grenze von 1/1024; er besteht. Rohe Gamma-Mittelung oder die Basisebene liegen weit
außerhalb dieses Intervalls. Die weitergehenden Vendor- und Mutation-Abnahmen bleiben offen.


## Eingrenzung des Wiederholungsfehlers

MipmappedChessRepeatsLinearPixels bleibt sporadisch rot: im Gesamtlauf 420 lineare Kanäle,
maximal 0,00268555, bei identischer Tiefe. Der alte Renderer zeigt denselben Fehler;
der neue Piece-Instanzpfad ist keine notwendige Ursache. Keine Wiederholung bis grün.

| temporäre Probe | Ergebnis / Grenze |
|---|---|
| Occlusion aus | Wiederholung gleich; Abschalten ist keine Reparatur |
| feste Mip-Stufe 0 | gleich, aber Integraloracle rot; kein zulässiger Fix |
| textureGrad mit feinen Ableitungen / precise | Fehler bleibt |
| zusätzlicher Surface-ID-Anhang | IDs gleich, Farbfehler bleibt |
| Float32-Ableitungsausgabe | Fehler verschwindet; Instrumentierung beeinflusst ihn |
| Float16-Ableitungsausgabe | Ableitungen gleich quantisiert, Farbfehler bleibt |
| SDL-Shadercross-MSL-Ausgabe | float2-UV und float-Textur, keine erklärte Half-Absenkung |
| verworfener Vorlauf-Frame | alle nachfolgenden Frames bitgenau; nur die erste Submission besitzt abweichende Farbe |
| gemeinsame Mip-Copy-Submission | 464 statt 467 Kanäle abweichend; Probe zurückgenommen, Uploadreihenfolge ist nicht die Ursache |
| SDL-Transferpuffer nach Submit freigegeben | SDL 3.4.16 garantiert sichere verzögerte Freigabe; kein Lifetime-Fehler des Upload-Puffers |
| SubjectDraw::CarryFrame aus | 470 Kanäle abweichend; Framezähler ist nicht die Ursache |
| TemporalResolve vollständig ausgeschlossen | 170 statt 249 Kanäle abweichend, gleiches Maximum 0,00268555; Temporal verstärkt den Erstframe-Effekt, verursacht ihn nicht |
| GPU vor dem ersten Frame vollständig geleert | 248 statt 249 Kanäle abweichend; ausstehende Asset-Uploads sind nicht die Ursache |
| Pending Residency-Crossings vor dem ersten Frame separat kopiert und geleert | 247 statt 249 Kanäle abweichend; Copy-/Renderpass-Bündelung ist nicht die Ursache |
| Erste abweichende Pixelwerte | ausschließlich RGB texturierter Punkte, etwa (673,319): 0,230225/0,197754/0,162842 gegen 0,231201/0,198486/0,163574; Tiefe und Alpha exakt |

Die erste Submission ändert einen Renderzustand, während Tiefe und alle späteren Farben exakt
bleiben. Nächste Prüfung: Erstframe-Tabellen/Culling und implizite Ableitung kausal trennen.
Keine permanente Diagnoseausgabe oder verworfenen Warm-up als Reparatur. Alle Proben sind
zurückgenommen. Exakte Läufe stehen in der Git-Historie.

`SubjectCullStage` setzt `Stood_` erst beim ersten Cull; die Depth-Pyramide entsteht später in
demselben Frame. Der erste Frame hat damit keinen Occlusion-Eingang, der zweite einen. Das erklärt
den Zustandswechsel, nicht die sichtbare Abweichung: Occlusion Culling muss konservativ bleiben
und bei statischer Kamera denselben sichtbaren Output liefern.

Die Eckprojektion wurde durch ein homogenes Clip-Intervall ersetzt: Zeilenlängen der Projektionsmatrix
begrenzen X/Y/Z und W über der Kugel, alle vier Quotientenenden liefern Bildschirmrechteck und
nächste Tiefe. Das ist absichtlich weiter als die Kugel und kann daher keine sichtbare Fläche
wegcullen. `NativeMipImagesReachTheRenderer` bleibt grün. Die ABeautifulGame-Wiederholungsabnahme
ist aktuell unscored, weil Pin `260a21ace17aff8aee322a2ba661d090cf178b57c8e1c855d2134d39aca0586b`
im Referenzcache fehlt; keine Wirksamkeitsbehauptung vor dessen Wiederherstellung.

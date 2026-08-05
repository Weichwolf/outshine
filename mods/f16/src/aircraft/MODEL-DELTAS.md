# Modell-Deltas — was FlightBox' Kopien vom gepinnten Upstream unterscheidet

FlightBox lädt jedes JSBSim-Modell aus dem `aircraft`-Wurzelpfad des Mods (`mods/f16/mod.json`, eine
Wurzel, `sim/src/missions/FBMod.h`). Das gepinnte Submodul `sim/vendor/jsbsim` ist damit kein Ladepfad
mehr, sondern die **Basis**: der Stand, gegen den jede Kopie dort gemessen wird. Diese Liste liegt IN
`aircraft/` und ist der einzige Eintrag darin, der kein Modell ist — `verify_models.py` überspringt
genau die Datei, die es als `--deltas` bekommt, weil ein Dokument sich nicht selbst deklarieren kann.

**Die Regel** (`CLAUDE.md`, Prinzip 1; ausführlich in `doc/architecture.md`): eine Kopie DARF vom Upstream abweichen —
berechtigte Korrekturen und Erweiterungen sind zulässig. Aber jede Abweichung ist ein **benannter
Eintrag in diesem Dokument**, mit Datei, Änderung, Grund und BELEG. Ein besserer Missionsausgang ist
ausdrücklich kein Beleg; Beleg ist eine publizierte Quelle, ein nachweisbarer Fehler im Modell oder ein
fehlendes Element, das eine Erweiterung braucht. Was hier nicht steht, existiert nicht:
`make -C sim verify-models` schlägt fehl, sobald der tatsächliche Diff nicht exakt dieser Liste
entspricht — in beide Richtungen (unerklärtes Byte wie erklärte, aber nicht vorhandene Änderung).

**Warum diese Form.** Der Beleg-Text und der Diff, den er rechtfertigt, stehen in EINER Datei und
können daher nicht auseinanderlaufen. Der Diff-Block ist kein Zitat, sondern der Prüfgegenstand: der
Verifikator (`sim/tools/verify_models.py`) rechnet den kanonischen Unified-Diff (`difflib`, 3 Zeilen
Kontext) zwischen Upstream und Kopie aus und vergleicht ihn Zeichen für Zeichen mit dem hier
hinterlegten. Nicht `patch`/`git apply`: eine Anwendung mit Fuzz könnte eine Abweichung verschlucken,
ein exakter Textvergleich nicht — und bei Fehlschlag kann das Werkzeug den Block ausgeben, der hier
stehen müsste.

## Herkunft

Kopie relativ zu diesem Verzeichnis, Upstream relativ zu `sim/vendor/jsbsim`. Ein Verzeichnis meint
sich selbst und alles darunter. `—` = kein Upstream (FlightBox-eigenes Modell, nichts zu vergleichen).
**Jeder Eintrag in diesem Verzeichnis muss hier stehen** — ein nicht deklariertes Modell ist ein
ungeprüftes Modell und lässt `verify-models` fehlschlagen.

| Kopie | Upstream |
|---|---|
| `f16` | `aircraft/f16` |
| `f16/engine/F100-PW-229.xml` | `engine/F100-PW-229.xml` |
| `f16/engine/direct.xml` | `engine/direct.xml` |
| `mk82` | `aircraft/mk82` |
| `aim120` | — |
| `mig29` | — |
| `aim9` | — |
| `r73` | — |
| `r27r` | — |
| `v750` | — |
| `v601` | — |
| `3m9` | — |
| `9m33` | — |
| `strela2` | — |
| `igla` | — |
| `agm88` | — |
| `mk84` | — |
| `gbu12` | — |
| `cbu87` | — |
| `fab250` | — |
| `fab500` | — |
| `f15c` | — |
| `su27` | — |
| `mig21` | — |
| `mig23` | — |
| `mig25` | — |
| `mig17` | — |
| `su7` | — |
| `su22` | — |
| `mirf1` | — |
| `f5e` | — |
| `k13` | — |
| `r60` | — |
| `r24r` | — |
| `r40r` | — |
| `aim7` | — |
| `s530f` | — |
| `magic1` | — |
| `tank370` | — |

Die **zehn Katalog-Jets** (`f15c` `su27` `mig21` `mig23` `mig25` `mig17` `su7` `su22` `mirf1` `f5e`)
sind FlightBox-eigene Modelle aus EINEM Rezept und werden nicht von Hand geschrieben, sondern
GENERIERT: `sim/tools/gen_air_decks.py` erzeugt sie aus den je acht publizierten Ankern der Zeile
(`doc/modules/air/catalogue.md`) durch geschlossene Inversion. `tools/gen_air_decks.py --check` prueft,
dass die eingecheckten Decks exakt das sind, was das Rezept ausgibt — eine Handaenderung an einem
generierten Deck ist ein Defekt, kein Delta. Verfahren, Analogien, Bänder: `doc/modules/air/flight-model-recipe.md`.

Die **sieben Luft-Luft-Runden der Katalog-Jets** (`k13` `r60` `r24r` `r40r` `aim7` `s530f` `magic1`)
kommen aus demselben Schlankkörper-Rezept wie die sechs Boden-Luft-Runden, generiert von
`sim/tools/gen_air_stores.py`. Der eine Unterschied zur Boden-Variante steht in jedem Banner: dV ist
hier ein DELTA ueber die 250-m/s-Trennungsgeschwindigkeit, denn diese Runden verlassen eine Schiene mit
voller aerodynamischer Autoritaet.

Die sechs Boden-Luft-Runden (`v750` `v601` `3m9` `9m33` `strela2` `igla`) sind FlightBox-eigene Modelle
aus EINEM Rezept: der nicht-dimensionale Schlankkörper-Satz des AIM-120-Decks, je Zeile aus dem
publizierten Durchmesser, der Masse und der Endgeschwindigkeit dimensioniert. Der eine physikalische
Unterschied zum luftgestarteten Rezept steht in jedem Banner: dV IST die ganze Endgeschwindigkeit statt
eines Deltas über eine Abschussgeschwindigkeit, denn diese Runden verlassen eine SCHIENE bei
Fahrt null. Quellen und Vertrauensstufen je Zahl: `doc/modules/ground/catalogue.md`.

Die sechs Luft-Boden-Stores (`agm88` `mk84` `gbu12` `cbu87` `fab250` `fab500`) sind FlightBox-eigene
Modelle aus ZWEI Rezepten. `agm88` folgt dem Schlankkörper-Rezept der Boden-Luft-Runden unverändert,
mit dem EINEN physikalischen Unterschied, dass dV wieder ein Delta über eine Abschussgeschwindigkeit ist
(sie verlässt einen Pylon bei Mach 0,9). Die vier freifallenden Zeilen und die Lenkbombe tragen den
**Mk-82-Satz dieses Baums ohne eine geänderte Ziffer** — er ist nicht-dimensional, also ist ein größerer
Flossenzylinder eine größere Referenzfläche und eine schwerere Masse und sonst nichts; berechnet wird je
Zeile Sw = 2,54 ft² · (d/10,75 in)², Spannweite = Sehne = 0,9 ft · (d/10,75 in), Iyy = 0,84·m·L²/12 und
Ixx = m·(d/2)²/2. Die Trägheit ist die PHYSIKALISCHE Zahl und die des Mk-82-Modells ist es nicht (633
slug·ft² für einen 15,5-slug-Körper von 5,5 ft sind zwei Größenordnungen daneben, stammen aber von
Upstream); die Folge steht im Banner jedes Modells statt in einem Delta an einem Modell, das dieser Baum
nicht besitzt. Die `gbu12` bekommt als einzige zusätzlich eine Steuerfläche (vier Canards, bang-bang,
Ausschlag ±15°) — Quellen und Vertrauensstufen je Zahl: `doc/air-to-ground.md` §§2–3.

Die beiden Engine-XML liegen als `f16/engine/` IM Modellverzeichnis statt in einer geteilten
Engine-Wurzel: das ist JSBSims eigenes Pro-Flugzeug-Layout (`FGPropulsion::FindEngineFullPathname` und
`FGFCS::FindFullPathName` suchen `<aircraft>/engine` bzw. `<aircraft>/Systems` VOR jedem geteilten
Pfad), es hält ein Modell als eine Einheit kopierbar, und die AIM-120 macht es seit jeher so. Die
Dateien selbst sind unverändert — der Ortswechsel ist kein Delta am Inhalt, sondern steht in der
Herkunftstabelle.

## Deltas

### D1 — Flaperon-Mixer: Klappen symmetrisch, Querruder differenziell

- **Datei:** `f16/f16.xml`
- **Änderung:** In `fcs/left-flaperon-norm` tragen jetzt BEIDE Eingänge das positive Vorzeichen
  (`+tef-control`, `+aileron-speed-compensated`) statt beide das negative; `fcs/right-flaperon-norm`
  bleibt unangetastet (`+tef`, `−ail`). Damit ist die Mischung `left = tef + ail`, `right = tef − ail`
  — die Summe `fcs/flaperon-summer` trägt nur noch den KLAPPEN-Anteil (`2·tef`), der Querruder-Anteil
  kürzt sich heraus. Der Verstärker `fcs/flaperon-mix-rad` geht dazu von `1.4324` auf `0.1745329`:
  derselbe Faktor, nur als Kehrwert, denn nach der Umstellung ist der Ausgang das, was sein Name und
  seine beiden Verbraucher verlangen — ein Winkel in RADIANT, nicht ein normiertes Kommando.
- **Grund:** Das Modell kürzt in der jetzigen Form genau die Größe weg, die der Mixer transportieren
  soll, und lässt die durch, die er ausschließen soll. `left + right = (−tef − ail) + (+tef − ail) =
  −2·ail`: `fcs/tef-control` erreicht KEINE einzige Aerofunktion (die beiden Summer sind seine einzigen
  Abnehmer), und `fcs/flaperon-mix-rad` — die Stellgröße von `CLDflaps` (0,35/rad) und `CDDflaps`
  (0,08/rad), seiner einzigen zwei Verbraucher — führt stattdessen das doppelte Querruderkommando.
  Beide Aerofunktionen sind SYMMETRISCHE Kraftbeiwerte (Auftrieb, Widerstand); das Rollmoment eines
  differenziellen Ausschlags läuft im Modell über eine ANDERE Eigenschaft (`fcs/aileron-pos-rad` →
  `Clda`/`Clda_M`/`Cnda`/`Cnda_M`). Aus dieser Verbraucherstruktur allein folgt zwingend, was der
  Mixer liefern muss: den symmetrischen (Klappen-)Anteil zweier Flaperons, in Radiant, ohne
  Querruderanteil. Die Einheit folgt aus derselben Struktur: beide Verbraucher sind „je Radiant"
  angeschriebene Derivate, und alle übrigen Flächen des Modells erreichen die Aerodynamik ebenfalls
  über `-pos-rad`-Eigenschaften (`fcs/aileron-pos-rad`, `fcs/speedbrake-pos-rad`). Der Kanal „Flaps"
  normiert mit `2.864789 = 1/0,349` von Radiant nach Norm; die Umkehrung, angewandt auf den MITTELWERT
  zweier Flächen, ist `1/(2·2,864789) = 0,1745329`. Der Bestandswert 1,4324 ist genau `2,864789/2`,
  also dieselbe Konstante mit vertauschtem Zähler und Nenner.
- **Beleg:** Ein nachweisbarer Fehler im Modell, belegt durch eine physikalische Unmöglichkeit, die es
  erzeugt. Getrimmt bei 350 KCAS / 10.000 ft, Querruder-Sprung ±0,5, direkt an diesem Modell gemessen
  (`forces/fbx-aero-lbs` ist die aerodynamische Körper-x-Kraft; negativ = Widerstand):

  | t (s) | `ail_cmd` | `flaperon-mix-rad` | `Nz` (g) | `fbz-aero` (lbf) | `fbx-aero` (lbf) |
  |---|---|---|---|---|---|
  | 1,97 | 0,0 | −0,0001 | +0,964 | −19.883 | −5.345 |
  | 2,21 | **+0,5** | −1,2797 | **−1,539** | **+31.203** | **+6.420** |
  | 2,21 | **−0,5** | +1,2899 | **+3,455** | −70.696 | −18.413 |

  Ein Rollkommando von einer halben Stickauslenkung kippt den Auftrieb in 0,24 s um 2,5 g (nach rechts)
  bzw. um 2,5 g nach oben (nach links) — dieselbe Eingabe, entgegengesetztes Ergebnis —, und der
  aerodynamische Widerstand wird beim Rechtsrollen mit **+6.420 lbf nach VORN** positiv: eine
  Auftriebsfläche, die das Flugzeug beschleunigt. Nach der Korrektur bleibt `flaperon-mix-rad` bei
  einem reinen Rollkommando exakt 0,0000, `Nz` läuft von +0,964 auf +0,972 (statt auf −1,539) und
  `fbx-aero` bleibt über das ganze Manöver negativ (−5.345 → −5.267 lbf). Umgekehrt wirken die
  Landeklappen erst jetzt überhaupt: bei voll ausgefahrenem TEF (`tef-control = 1`) steht
  `flaperon-mix-rad` auf 0,3490 rad = den 20°, die der Kanal „Flaps" kommandiert (vorher −0,0002 rad),
  also ΔCL = 0,122 und ΔCD = 0,028 statt ΔL = −4 lbf. Der Ausschlag ist damit **deckungsgleich mit dem
  Kommando des Modells selbst** — es wird keine fremde Zahl eingesetzt, nur der bereits deklarierte
  Wert erreicht seinen Verbraucher.

```diff
--- upstream/f16/f16.xml
+++ flightbox/f16/f16.xml
@@ -442,8 +442,8 @@
    </scheduled_gain>
 
    <summer name="fcs/left-flaperon-norm">
-    <input>-fcs/tef-control</input>
-    <input>-fcs/aileron-speed-compensated</input>
+    <input>fcs/tef-control</input>
+    <input>fcs/aileron-speed-compensated</input>
     <clipto>
      <min>-1.0</min>
      <max>1.0</max>
@@ -466,7 +466,7 @@
 
    <pure_gain name="fcs/flaperon-mix-rad">
     <input>fcs/flaperon-summer</input>
-    <gain>1.4324</gain>
+    <gain>0.1745329</gain>
    </pure_gain>
 
    <aerosurface_scale name="fcs/left-aileron-control">
```

<!-- Format eines Eintrags — der Verifikator verlangt alle vier Felder nicht leer und genau einen
     ```diff-Block, dessen Kopfzeilen `--- upstream/<pfad>` / `+++ flightbox/<pfad>` lauten:

### D1 — Kurztitel

- **Datei:** `f16/f16.xml`
- **Änderung:** was genau anders ist, in einem Satz.
- **Grund:** warum das Modell so nicht bleiben kann.
- **Beleg:** die Quelle — publizierte Daten mit Fundstelle, der nachweisbare Fehler, oder das fehlende
  Element und wozu es gebraucht wird.

```diff
--- upstream/f16/f16.xml
+++ flightbox/f16/f16.xml
@@ -1,3 +1,3 @@
 ...
```
-->

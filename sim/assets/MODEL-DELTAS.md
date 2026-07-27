# Modell-Deltas — was FlightBox' Kopien vom gepinnten Upstream unterscheidet

FlightBox lädt jedes JSBSim-Modell aus `sim/assets/aircraft` (eine Wurzel, `app/FBModelRoots.h`). Das
gepinnte Submodul `sim/vendor/jsbsim` ist damit kein Ladepfad mehr, sondern die **Basis**: der Stand,
gegen den jede Kopie dort gemessen wird. Diese Liste steht NEBEN `aircraft/`, nicht darin — das
Verzeichnis wird als Ganzes in den WASM-Build eingebettet und enthält deshalb nur Modelle.

**Die Regel** (`CLAUDE.md`, Prinzip 1; ausführlich in `doc/flightbox/architecture.md`): eine Kopie DARF vom Upstream abweichen —
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

Kopie relativ zu `sim/assets/aircraft`, Upstream relativ zu `sim/vendor/jsbsim`. Ein Verzeichnis meint
sich selbst und alles darunter. `—` = kein Upstream (FlightBox-eigenes Modell, nichts zu vergleichen).
**Jeder Eintrag unter `sim/assets/aircraft` muss hier stehen** — ein nicht deklariertes Modell ist ein
ungeprüftes Modell und lässt `verify-models` fehlschlagen.

| Kopie | Upstream |
|---|---|
| `f16` | `aircraft/f16` |
| `f16/engine/F100-PW-229.xml` | `engine/F100-PW-229.xml` |
| `f16/engine/direct.xml` | `engine/direct.xml` |
| `mk82` | `aircraft/mk82` |
| `aim120` | — |

Die beiden Engine-XML liegen als `f16/engine/` IM Modellverzeichnis statt in einer geteilten
Engine-Wurzel: das ist JSBSims eigenes Pro-Flugzeug-Layout (`FGPropulsion::FindEngineFullPathname` und
`FGFCS::FindFullPathName` suchen `<aircraft>/engine` bzw. `<aircraft>/Systems` VOR jedem geteilten
Pfad), es hält ein Modell als eine Einheit kopierbar, und die AIM-120 macht es seit jeher so. Die
Dateien selbst sind unverändert — der Ortswechsel ist kein Delta am Inhalt, sondern steht in der
Herkunftstabelle.

## Deltas

Aktuell **keine**. Alle Kopien sind byte-identisch mit ihrem Upstream-Pendant.

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

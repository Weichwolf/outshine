# Transport: Arbeitskontext für den Rechnerwechsel

Stand: 2026-10-13. Basis: `a0c59463e` auf `master`. Der Goal ist ausdrücklich **pausiert**.
Diese Datei ist ein Übergabestand; Git, aktuelle Dateien und neue Nutzeranweisungen haben Vorrang.
Zuerst AGENTS.md, #5 und #18–#21 lesen. Kein automatisches Wiederaufnehmen durch den Transfer.

## Goal zum erneuten Setzen

Ich entwickle in Python einen schnellen, headless ausführbaren und visuell prüfbaren Solver für plausible, funktionale OSM-Transportnetze. Zuerst löse ich zusammenhängende Straßen-, Wege- und Schienennetze in 2D; danach ergänze ich in 2,5D Höhenprofile, Rampen, Brücken, Tunnel und Durchgänge. Echte Daten aus Wien, Zürich und Basel bestimmen die Problemfälle. Anschlüsse teilen gemeinsame Randsegmente; Breiten, Verkehrsebenen, Krümmungen, Steigungen und Freiräume bleiben konstruktiv konsistent. Überlappende Bauteile werden gemeinsam gelöst, Verkehrsverbindungen bleiben erhalten, Gebäude und Terrain berücksichtigen den geplanten Verkehrsraum. Ich untersuche jedes sichtbare Defizit, vergleiche etablierte Verfahren und messe Laufzeit, Speicher sowie Skalierung. Der Solver soll durch räumliche Eingrenzung und lokale Lösungen möglichst linear skalieren. Erst wenn die vollständigen Netze in den geprüften 2D- und 2,5D-Renderings funktionieren und ihre Kosten tragbar sind, übertrage ich den bewährten Ansatz nach Outshine. Details, Entscheidungen und offene Lücken pflege ich knapp im Board.

Der bisherige externe Goal-Pfad war `/Users/cosmo/.codex/attachments/e65763fd-8b3f-446e-a52e-0cd7e7205b16/pasted-text-1.txt`.
Auf dem anderen Rechner gilt der obige vollständige Text; der Attachment-Pfad ist nicht portabel.

## Zuständigkeit und Arbeitsweise

- Dieser Agent besitzt Transport auf master. Buildings und Vegetation arbeiten auf getrennten Branches.
  Nicht deren Verfahren oder Ergebnisse überschreiben. Gemeinsame Raumverträge stehen in #18.
- Parent #5 active; #18/#19 active. Profile #20 und Ebenen #21 sind noch open.
- Python-Verfahren mit echten Daten vollständig lösen, dann C++. Keine nativen Gates während dieser Experimente.
  Lokale Prüfungen und `make lint-docs`; volle Engine-Prüfungen erst bei nativer Integration.
- Kein Webauftrag in diesem Arbeitsblock. Bestehende lokale SIGGRAPH-/Primärreferenzen nutzen.
- Keine behauptete globale Optimalität. Kosten, Funktion und Bilder belegen; keine Tests abschwächen.
- Eine lokale gültige Einzelkurve reicht nicht: Nachbarn, alle alten Fahrten und vollständiges Fenster erhalten.
  Abgelehnte Quellen/Fahrten mit Grund erhalten. Geometry ≠ Zugang ≠ körperliche Befahrbarkeit.
- Ein schwerer Prozess zugleich. Einen Timeout nicht als Ende behandeln; denselben Handle weiter prüfen.
- Änderungen erhalten, kleine vollständige Commits. Kurz auf Deutsch berichten und Bilder selbst öffnen.

## Geprüfter Stand und Grenzen

| Ebenen-0-Fenster, Radius 1 km | PKW-Verbindungen | Messcommit | Python-Fensterlauf |
|---|---:|---|---:|
| Wien | 909/909 | 117dea678 | etwa 10,1 s |
| Basel Badischer Bahnhof | 2962/2962 | 5ab84801f | etwa 16,5 s |
| Zürich HB | 4806/4808 | a669779ad | 34,108 s |

Zürich: 35 gekoppelte Produkte, alle früheren Fahrten erhalten. 779812864 Byte Peak-RSS,
also 779812864/1048576 ≈ 744 MiB. Der Fensterlauf misst Laden, Restkurvensuche und Mesh-/Netzprüfung;
vorbereiteter Produktaufbau kommt zusätzlich hinzu. Keine Engine-/Cache-Ladezeit oder A18-Pro-Messung.
Wien/Basel wurden mit den angegebenen früheren Messcommits geprüft, nicht mit dem neuesten gemeinsamen Stand.
Zürcher Restfälle: Modul 1396, Fahrten 6→1 und 2→5. Andere Modi, Verkehrsfluss/Spuren,
2,5D, weltweite Robustheit, automatische Rezeptauswahl und tragbare Gesamtkosten sind offen.

PKW/LKW/Bus-Zugang und Klasseneinbahnregeln werden getrennt übersetzt, auch für gespeicherte Graphen.
`hgv=no`, `motorcar=no`, beschränkte Zufahrt und Busfahrbahnen sind geprüft.
Die beobachteten Anbieterfelder liefern bislang keine spezifischen Fahrzeug-/Größen-/Lasttags.
Fehlende Angaben bleiben markierte Rezeptannahmen, keine Zulassung. Gewichts-/Maß-/Zeitregeln fehlen.
Die bisherigen Hüllenbeweise gelten für das kompakte PKW-Profil, nicht für LKW.

## Letzte vollständige Lieferung

- `a669779ad`: kurze Endübergänge separat passen, lange Gerade als ein Segment, gemeinsam ≤1024 Residuenaufrufe.
  G2, Mindestkurvenradius, ganze Fahrzeughülle, Reservationen und gemeinsame Flächen weiter prüfen.
- Modul 1137: 72/72 plus zwölf Nachbarfahrten; +0,82915 m² gemeinsame Fahrfläche.
  Beide Richtungen brauchen Seeds. Nur die neue Richtung einzubauen verliert eine alte Gegenfahrt.
- Vergleich gleicher lokaler Eingaben/Zugänge: 49 statt 136 Kontrollsegmente,
  79 statt 167 Hüllintervalle pro Richtung. Die gleichmäßige Passung besteht ebenfalls;
  die Aufteilung ist kein Beweis, dass Gleichverteilung allein den früheren Fehler verursachte.
- `a0c59463e`: vollständiges Zürcher Fenster mit diesen Produkten bestätigt, `make lint-docs` bestanden.
  Lokale Split-Prüfungen und erneutes Laden des Produkts ebenfalls terminal erfolgreich.
- `d9e180269`: 15 überholte Diagnose-Skripte entfernt (519 Zeilen), 89 Versuchdateien gelöscht (~208 MiB).
  Aktuelle Eingaben, erfolgreiche Produkte, Referenzen und benötigte Abhängigkeiten erhalten.

Wichtige Quellen: `test/experiments/transport/transport_travel.py`,
`transport_driving_network.py`, `transport_roadbed_window_experiment.py` und `probes/roadbeds/`.
Neue Methode: `split_straight_route_probe.py`; unabhängige Prüfung: `split_straight_route_checks.py`.
Bestehende Rezepte: `inset_turning_core_probe.py` (CCC/CSC), `source_shortcut_probe.py`,
`coupled_turning_court_probe.py`, `steering_roadbed_product_probe.py`, `route_snapshot_probe.py`.

## Konkreter nächster Versuch: 1396

Nahbild und Quellposen geprüft: sehr spitzer Anschluss, halbe Fahrbahnbreite 1,75 m.
6→1 hat 154,44° Richtungsänderung. Einfacher 4,2-m-Bogen braucht
4,2·tan(154,44°/2) ≈ 18,52 m Vorlauf; feste Strahlen treffen nach nur 9,38/9,84 m.
Fünf Meter Schnittversatz verschärfen wegen Quellkrümmung den Winkel auf 169,06°;
der nötige Vorlauf wächst auf etwa 43,87 m. Weiteres blindes Zurücksetzen ist ungeeignet.
Zulässige gemeinsame Anschlussrichtungen sind vorhanden; beispielsweise Eingang 6 bis −30° Änderung.
Beide Richtungen und Nachbarflächen müssen gemeinsam erhalten bleiben, nicht nur eine Kurve optimieren.

Der folgende begrenzte Versuch wurde beim Pausieren mit SIGTERM beendet (Exit 143).
**Kein Erfolg und kein verwendbares neues Produkt belegt.** Auf dem Zielrechner erst Prozesse prüfen.

```sh
python3 -W error test/experiments/transport/probes/roadbeds/fixed_cut_frame_probe.py \
  --place ZuerichHauptbahnhof --module 1396 --pair 6 1 --all-headings \
  --output build/experiments/transport/roadbeds/zurich1396-fixed-directions
```

Wenn Richtungswahl scheitert, Ursachen der Nachbar-/Kurvenablehnung auswerten und einen anderen
begrenzten gemeinsamen Anschlussentwurf vergleichen. Keine Schutzprüfung entfernen.
Danach Rezepte automatisch auswählen, Wege/Rad/Schienen, weltweite Stichproben und 2,5D angehen.

## Lokale Daten: NICHT durch Git übertragen

`build/` ist ignoriert. Dieser Push enthält Quellen und Kontext, nicht Versuchsdaten oder PNGs.
Für direkte Fortsetzung den bisherigen `build/experiments/transport/`-Baum samt benötigten
OpenFreeMap-Quelltiles mitnehmen. Receipts enthalten absolute Quellpfade und SHA-256;
bei anderen Pfaden regulär neu erzeugen. Keine Herkunftshashes umschreiben, um Prüfungen zu umgehen.
Python-Abhängigkeiten: NumPy, SciPy, Shapely 2.x/GEOS und Matplotlib; zuerst lokal prüfen.

Wichtige lokale Dateien relativ zu `build/experiments/transport/`:

- `inputs/`, `directed-graphs/`, `directed-surfaces/`, `window-1000-body/`, `boundary-optimized/`, `cut-setbacks/`.
- `roadbeds/zurich-integrated-a669779ad/{products.json,rebuild-plan.json,window/window-roadbeds.json}`.
  Manifest: 35 Produkte. Wiederaufbauplan: 59 geordnete Jobs; deren Vorläuferdateien ebenfalls benötigt.
- `roadbeds/zurich1137-current/{3-16.json,17-2.json}`, `zurich1137-split-straight/5-candidate.json`,
  `zurich1137-split-reverse/3-candidate.json`, `zurich1137-split-product/{proof.json,mesh.npz,reloaded.json}`.
- `roadbeds/zurich1396-current/`: Quellfälle, setback5-Vergleiche und `space-review.png`.
- Frühere Ergebnisse: `roadbeds/wien-refined-loop-117dea678/` und `basel-complete-motor-5ab84801f/`.

Ausgewählte aktuelle Bilder: `build/shots/experiments/transport/Zuerich-complete-window.png`,
`Zuerich-1137-split-straight.png`, `Zuerich-1565-shared-return.png`. Alle selbst geöffnet/geprüft.
Netzwerkquellen lagen unter `/Users/cosmo/Library/Application Support/outshine/outshine/sources/`.
Logs lagen im System-Tempverzeichnis; der letzte vollständige Lauf hieß `outshine-zurich-window-a669779ad.log`.
Logs sind nicht reproduzierbare Eingaben. Daten/Belege fehlen auf einem frischen Clone: nicht als grün ausgeben.

## Aktualisierte Arbeitsaufteilung
Einmalige Übergabe an den Transport-Agenten nach Rechnerwechsel. Checkout `outshine_transport`, Branch `transport`.
Gemeinsame Aufgaben: https://github.com/Weichwolf/outshine/issues. Lokales Board und Board-Gatetests sind entfernt.
Experimente unter `test/experiments/`, wiederverwendbare Python-Referenzbibliotheken unter `lib/`; native Integration unter `src/` durch den Master-Agenten auf dem anderen Rechner.
Fachagenten dürfen geprüfte Experiment-/Bibliotheks-PRs nach `master` mergen. Gemeinsame Bibliotheksarbeit: https://github.com/Weichwolf/outshine/issues/47.

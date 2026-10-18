# Konsistente endliche Terrainkontakte (#589)

Der reale Wiener 65er Bestand enthält einen gültigen gemeinsamen Kantenkontakt,
den `SurfaceSampler` zuvor als mehrdeutig zurückstellte. Zwei alte Dreiecke teilen
dieselben Quell-Vertex-IDs und liefern identisches Z. Im sehr dünnen Dreieck liegt
die Abfrage zugleich nahe einer anderen Kante; getrennte Argmin-Wahl findet deshalb
nicht dieselbe gemeinsame Kante. Das ist kein Widerspruch der Höhenfläche.
Ein zweiter Gegenfall zeigte eine falsche Zulassung nahe der unendlichen Verlängerung
einer Quellkante, obwohl deren endliches Segment weit entfernt liegt.

Der bisherige Vertrag bleibt: konsistente gemeinsame Quellkanten/-vertices innerhalb
der unveränderten metrischen XY-Toleranz lösen einen Kontakt auf. Echte projizierte
Überlappungen bleiben mehrdeutig, auch bei gleicher Höhe.
Die Kantenwahl verwendet nun Abstand zum endlichen Segment. Bei noch mehrdeutigen,
höhenkonsistenten Treffern prüft ein begrenzter Rückfall die tatsächlich gemeinsamen
Quell-Vertices aller Kandidaten. Er benötigt keine neuen räumlichen Queries.
Niedrigster Faceindex, Höhenkonsistenz, Query-/Kandidatenbudget, Fehlgründe und
unveränderliche Ausgaben bleiben erhalten. Einzeltreffer benötigen keine Kantensuche.
Es gibt keine neue öffentliche oder native Schnittstelle und keine erhöhte Toleranz.

## Tatsächlicher Gegenfall und Kontrollen

Fester kleiner Originalinput: `lib/tests/data/skinny-shared-ground-contact.json`.
Er enthält vier ursprüngliche XYZ-Vertices, beide tatsächlichen Dreiecke, Abfrage,
Frame und Resident-/Elternprodukt-Hashes. Keine Abhängigkeit von privaten Tempdaten.
Lokale ursprüngliche Faces 297/300, gemeinsame Vertex-IDs 361/364.
Abfrage XY=(-681,6068139552688;517,8330826538654) m;
metrische XY-Toleranz unverändert 1e-7 m, erwartetes Z=163,69377139602037 m.

Vier neue Tests prüfen den tatsächlichen Fall in beiden Face-Reihenfolgen/Orientierungen,
geometrisch gleiche, aber nicht quellverbundene Faces, widersprüchliche Höhe,
Budgeterschöpfung und die unendliche Kantenverlängerung. Vorher scheitern zwei
dieser vier Tests; nachher bestehen sie samt den zehn bisherigen Sampler-Kontrollen.
Alle **536 Quell-/536 frisch installierten Bibliothekstests** bestehen terminal OK.
205 Paketmodule sind bytegenau zur Quelle. Die aktuelle vollständige Pinus-/
Habitat-/Körperprobe aus #584 wird mit dem neuen Wheel frisch gelesen: unveränderte
vollständige Form, vier SITE-Produkte und alle 120 örtlichen Zurückstellungen bestätigt.

Die größere Netzerweiterung besitzt [#587](https://github.com/Weichwolf/outshine/issues/587).
Der Fehler wurde bei einer ausdrücklich provisorischen alternativen Junction-Form
sichtbar; die dort gespeicherten alten Terrain-Vertices werden durch diese Form
nicht geändert. Diese Hypothese wird durch den Samplerfix nicht als zulässiges
Netz oder Bildlieferung erklärt. Andere Anschluss-/Geometriefehler bleiben bestehen.

## Gleiche Ausgabe und Kosten

`surface_contact_compare.py` lädt den alten eigenen Sampler exakt aus Git-Commit
`64ce38ddb7ef5c5886541c69f03803f4bbd77ea4`. Kein Fremdcode oder SDK.
Geprüfter neuer Implementierungsstand: `a67f37ff004d7aefbdd2ed05a0a277c2be68a712`.
4096 deterministische normale Innenabfragen auf derselben ebenen Fläche liefern
vorher/nachher exakt dieselben Punkte, Normalen und Faceindizes. In dieser Last
entfallen 4096 Feature-Kandidatenzeilen / 16 Feature-Batches vollständig;
GEOS-Kandidaten und Abfragezahl bleiben gleich. Fünf wechselnde Paare:

| Normalabfrage | Median | Allokationen | Allokierte Bytes | Getrackter Peak |
|---|---:|---:|---:|---:|
| vorher | 18,128 ms | 94832 | 11968532 B | 1045328 B |
| korrigiert | 12,139 ms | 87837 | 7737132 B | 1034552 B |

Host i5-1135G7/Linux x86_64, vier sichtbare logische CPUs, Desktop-/Fremdlast;
teilweise eigener Wheel-Aufbau. Normal priorisierter Messprozess, ein BLAS-/OMP-Thread.
NumPy 2.2.4 / Shapely 2.2.0 / GEOS 3.14.1. Memray 1.20.0 einschließlich
Python-Allokatoren, Konstruktion/Import außerhalb der Timer/Traces.
Eigene Ergebnisarrays: jeweils 229376 B für Punkte/Normalen/Faceindizes;
interne Kopierbytes bleiben ungemessen. Keine Optimalitäts-/Hardware-/Weltladeaussage.

Zusätzliche 512-fache Wiederholungen des tatsächlichen gemeinsamen Kontakts
wechseln korrekt von null zu 512 gültigen Kontakten; die endliche Verlängerungs-
Gegenprobe korrekt von 512 zu null. Das sind geänderte Korrektheitsausgaben,
keine gleichwertigen Laufzeitvergleiche. Nur die obige normale Last ist ausgabegleich.
Der Rückfall darf bei schwierigen Mehrfachkontakten zusätzliche Arbeit benötigen.

## Herkunft und Reproduktion

Private Belege ausschließlich `/tmp/outshine-transport-network-64ce38ddb/`:

| Artefakt | SHA256 |
|---|---|
| `contact-cost/comparison.json` | `04dc8e745b39509694f88e9e83063446a57496115d5ed774a6ae21061945f1a2` |
| frisches Wheel unter `wheel/` | `7574fa7be6332eea6bb58504e9f9ce51e4a901b1f5de6889c97c35371a085043` |
| `plant-consumer-newlib.json` | `03b6c783da163b76714fe95da8022583b62a882a8f79a99b73aea49f931c9ee4` |

```sh
PYTHONPATH=lib/src:lib/tests "$CONTACT_PY" -W error -m unittest test_surface_sampling test_skinny_surface_sampling
PYTHONPATH=lib/src "$OPTIONAL_DEPS_PY" -W error -m unittest discover -s lib/tests -p 'test_*.py' -q
PYTHONPATH=lib/src:test/experiments/transport "$CONTACT_PY" -m surface_contact_compare --baseline-commit 64ce38ddb7ef5c5886541c69f03803f4bbd77ea4 --output "$FRESH_CONTACT_OUTPUT" --profile
```

Frische explizite `/tmp`-Ausgabe; Benchmark CPU 90 s / AS 3 GiB. Memray optional.
Ruff/Format und Funktionen ≤120 Zeilen bestanden; drei alte Ruff-Stilbefunde im
betroffenen Samplermodul mit behoben. Konstruktor-/Fehlerverträge unverändert.
SIGGRAPH-Ausgangspunkt des übergeordneten Versuchs: Chen et al. 2008,
[Interactive Procedural Street Modeling](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
Seite 2/Figur 2 erneut optisch geprüft: Graphvorschlag und geometrische Zulassung
bleiben getrennt. Das Paper ist kein Beleg dieser numerischen Kontaktkorrektur.
Keine Bildänderung oder native Integration durch diesen Fehlerfix allein behauptet.

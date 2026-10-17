# Präzision der Konfliktabfrage (#568)

Stand 2026-10-17: Die konstante `Reservation.conflict_geometry(HeightInterval)`
ist additiv in `lib/` implementiert. Bei positiver Höhenüberschneidung liefert
sie die ursprüngliche Geometrie, sonst ein leeres Polygon. Konstruktoren und
deren Fehlerverhalten bleiben erhalten. `overlaps` prüft den gemeinsamen
Innenraum ohne numerischen Flächenwert; reine XY-/Höhenberührung bleibt frei.
`AffineOccupancy` nutzt diese polymorphe Abfrage, bleibt aber im Experiment.
Der gemeinsame Vertrag wurde von Buildings, Vegetation und master ausdrücklich
bestätigt; Entscheidung/Restabnahme bei [#568](https://github.com/Weichwolf/outshine/issues/568).

## Gegenbeispiele und lokale Fehler

Bei `x=1000` und `x=500000`, P=`[x,x+1]×[0,1]`, L=`100+(X-x)`
und H=`[0,nextafter(100,101)]` ist der Konfliktstreifen exakt
`1/70368744177664` m breit. Die Konstruktion seiner absoluten XY-Koordinaten
kann ihn auf eine Linie runden. Ein diagonaler Dreiecksschnitt zeigt denselben
Fehler trotz gültigem gerundetem Clip-Rechteck. Der alte Pfad meldete jeweils leer.

Vor der Rückgabe einer leeren affinen Maske wird deshalb die Schnittfläche
der ursprünglichen Polygonringe mit beiden Höhenhalbebenen rational integriert.
Der signierte Randintegralansatz berücksichtigt Löcher, MultiPolygone und
getrennte Teilstücke nichtkonvexer Ringe; verbindende Clip-Randstücke heben
sich im Integral auf. Rationalität bezieht sich auf die gelieferten Binärzahlen.
Maximal 16384 Vertexbesuche; positive unrepräsentierbare Konflikte oder
erschöpftes Budget ergeben `OccupancyError`. Nur die betroffene Quelle wird
mit Originalgeometrie/ID/Grund zurückgestellt. Ein anderer lokaler Auftrag läuft weiter.
Eine leere Ringsumme bleibt eine rationale Null, ohne zwischenzeitliche Float-Konversion.

Drei weitere Gegenfälle haben gültige Polygone mit Seitenlänge `1e-170`:
klein/klein, groß/klein und querende dünne Rechtecke. Ihre positive Schnittfläche
unterläuft binary64 auf null. Die Eingaben bleiben zulässig; es gibt keine neue
Mindestflächenbeschränkung. `intersects && !touches` erkennt den gemeinsamen
Innenraum. Auch Normalisierung und Übergabe verwenden Geometrie-Leere statt
`area>0` zur Entscheidung über vorhandene Polygone.

## Unabhängige Prüfung und Grenzen

6503 Maskenfälle: 76 deterministische Kontrollen einschließlich U-Form,
Loch, Gitternetzen und verschobenen/diagonalen Grenzfällen; 441 tatsächliche
Facetten-/Höhenkandidaten aus 28 Wiener Footprints und 128 Bodenproben;
5986 kontrollierte Höhenschnitte durch sämtliche gespeicherten Transportfacetten.
Die 5986 kontrollierten Schnitte sind keine zusätzlichen echten Gebäude.
Referenz: unabhängig aufgezählte Halbebenen-Linienpaare und rationale Fenster
über optionales SFCGAL, ohne Wiederverwendung unseres Clip-Verfahrens.
Bei punktverbundenen gültigen Gittern akzeptiert das native Orakel nicht alle
Ringdarstellungen; die bekannte disjunkte Integerzellenzerlegung bleibt vollständig
im Vergleich. Der echte U-/Lochfall verwendet direkt das ursprüngliche Polygon.

Alle eigenen rationalen Flächen stimmen exakt mit dem unabhängigen Orakel überein.
Die drei unrepräsentierbaren positiven Masken werden ausdrücklich zurückgestellt.
Für zurückgegebene nichtleere Masken beträgt der größte beobachtete absolute
Flächenfehler **5,7531784365969815e-12 m²**. Das ist keine allgemeine XY-/Topologiegrenze.
Insbesondere sind verlorene Teilkomponenten einer insgesamt nichtleeren Maske
und beliebige weitere Footprint-Schnitte noch nicht allgemein zertifiziert.

Weitere drei analytische Unterlaufkontrollen ergeben zusammen **6506 Fälle**.
Replay des gepinnten alten Kerns `62befe71a` samt damaliger Bibliothek:
sechs falsche Leer-/Positiventscheidungen, keine Rückstellung.
Aktueller Kern: **null falsche Entscheidungen, drei explizite Rückstellungen**.
Die drei Unterlaufkonflikte sind positiv; ihre unabhängige geschlossene
Intervallrechnung und native rationale Koordinatenfläche stimmen überein.

Optionales Orakel: PySFCGAL 2.3.0, SFCGAL 2.3.0 / CGAL 6.2 / Boost 1.74.0.
Die native Schnittoperation auf zwei gültigen `1e-170`-Quadraten bricht isoliert
mit **SIGFPE (-8)** ab, nach bestätigter Quellgültigkeit. Das ist eine Orakelgrenze,
kein Leerheitsbeweis. Expliziter Negativtest: CPU 3 s, AS 1 GiB, Wall 6 s, Core 0;
vollständiger Kindprozesscode, Rückgabestatus und stderr bleiben im privaten Beleg.
Der reguläre Orakellauf hat CPU 120 s / AS 3 GiB / Wall 180 s; terminal 0,
25,6242 s und RSS-Peak 197705728 B bei parallelem Bibliothekstest.
Shapely 2.2.0 / GEOS 3.14.1. Keine SDK-/SFCGAL-Runtime-Abhängigkeit ergänzt.

## Kosten, Paket und Bild

447 identische Prädikatfälle: 441 reale Paare, drei Unterläufe, zwei reine
XY-Berührungen und eine leere Maske; jeweils 16 Wiederholungen, 7152 Aufrufe.
Beide Alternativen liefern denselben Booleanhash, gegen das Orakel geprüft.
Drei wechselnde Paare auf dem i5-1135G7 unter Desktop-/Hintergrundlast:

| Prädikat | Median | Allokationen | Allokierte Bytes |
|---|---:|---:|---:|
| DE-9IM `2********` | 84,572 ms | 375034 | 28145686 |
| `intersects && !touches` | 64,059 ms | 211674 | 18535990 |

Gewählt: zweite Variante, 43,56 % weniger Allokationen / 34,14 % weniger
allokierte Bytes; getrackter Peak bei beiden 119928 B. Memray 1.20.0 einschließlich
Python-Allokatortracking, vorbereitete Masken außerhalb der Messung.
Interne kopierte GEOS-Bytes, GPU-/Frame-/Zielgerätezeiten bleiben ungemessen.
Der gesamte aktualisierte 156er Übergabeweg ist auf identischen Eingaben und
Ergebnis-WKB geprüft: Vollscan/Index exponieren 933816/441 Zeilen, Median
93,636/42,762 ms aus drei wechselnden Paaren bei normaler Prozesspriorität.
Indexaufbau getrennt 1,8695 s; RSS-Peak 131923968 B. Memray zählt
514993/205913 Allokationen, 52542635/21622563 allokierte Bytes und
164608/163912 B getrackten Peak. Vollständige interne Kopierbytes bleiben ungemessen.
Ergebnishash unverändert `40b93f14c27822a62f744bec1e7b3fc87491120ec8dc665166e934468f48acae`.
Rohtraces unter `cost-normal/` bleiben privat; vorbereitete Eingaben außerhalb
der Abfragetimer/-traces. Dieser Lauf folgt dem fremden Vegetationsmerge auf
`d934aea512d4cce61bec60818b64d858fe728690`; keine parallelen eigenen Messarbeiter.

532 Bibliothekstests aus Quelle und frischem Wheel bestehen; zusätzlich je
35 fokussierte Quell-/installierte Kontrollen. Alle 205 installierten Pythonmodule
stimmen bytegenau mit der Quelle überein. Ruff/Format für geänderte Pythondateien;
sechs bestehende TRY004-Befunde in alten Bibliothekskonstruktoren bleiben wegen
deren bisherigem ValueError-Vertrag bestehen. Funktionen höchstens 120 Zeilen.
Native Gates folgen erst der Integration.

Frischer installierter Leser prüft alle 5986 Kappen, 65 Quellen, 124 ganzen Stützen,
41 Originalgebäude und 128 Proben einschließlich gespeicherter Ergebnisgeometrien.
Die beiden vollständigen privaten 1280×720-Bilder aus #575 wurden wieder geöffnet;
keine Geometrieänderung, kein neuer Bildgewinn, keine Veröffentlichung.
Tatsächliche Architektur-/Wuchsrezepte und die öffentliche affine API bleiben offen.

## Herkunft und Reproduktion

Geprüfter Implementierungsstand: `12551896f87ea780aa9949cd47694483d4cd1fa9`;
anschließend nur Dokumentation einschließlich eines Moduldocstrings und
unveränderte fremde Vegetationslieferung.
Originaleingaben/Produktkette: [Übergabevergleich](stair_occupancy_comparison.md).
Private Belege ausschließlich `/tmp/outshine-transport-precision-62befe71a/`:

| Artefakt | SHA256 |
|---|---|
| `oracle-final/comparison.json` | `c66e2cb4d706de0369d9d75734ddcf79d8f2074a87407135a81e27e7d1a76834` |
| `oracle-final/cases.json` | `8fab759273c0bfd0bc19469e6cbedf3e7ce60de51d89b166110b821f9929815a` |
| `before-current.json` | `64571a4d591c4b8559986788f3fa779c5ec2f32465eb171a6e9f5f7ac077395e` |
| `after-current.json` | `a4980c1719e58ef2a2ac507c2ec87205b0369e73aab5f9c806e45cfdc7865dba` |
| `reader-final.json` | `cc80de2f2fd520e917af770549e221b63be0dc1bb37ef8dabf2750e6a2cf910e` |
| frisches Wheel unter `wheel-final/` | `6d32ec1a9a2db3051b4ecd17d189b7838949a4481c33355264fafd2df9168fa0` |
| native SFCGAL-Bibliothek | `877f0cb1c32939bba61072db0a3e0f748d934a257c5ced309bcc37724bacd0af` |
| CFFI-Bindung | `97499eda431e62578f1638f1cbe6c274c107b1f7830e329f39204d84ee3ab244` |

`stair_occupancy_precision_experiment`, `stair_occupancy_empty_compare`,
`stair_occupancy_native_probe` und `stair_occupancy_predicate_cost` als Pythonmodule
ausführen, `PYTHONPATH=lib/src:test/experiments/transport`; jeweils `--help`.
Oracle optional in der vorhandenen PySFCGAL-Umgebung; normale Tests benötigen es nicht.
Neue Ausgaben ausdrücklich unter `/tmp` wählen; Replay bindet geladenen Kern
und geerbtes Prädikat an `--kernel-commit`, Modell und vollständiges Fallledger an SHA256.

Zuerst lokale SIGGRAPH-Grundlage geprüft und PDF-Seite geöffnet:
[Interactive and Robust Mesh Booleans (2022)](../../../doc/references/vegetation/siggraph/2022-interactive-robust-mesh-booleans.pdf).
Sie motiviert robuste numerische Entscheidungen, liefert hier keinen übernommenen Code.
Primärquellen: [CGAL FAQ zu Prädikaten/Konstruktionen](https://www.cgal.org/FAQ.html)
und [Shapely DE-9IM](https://shapely.readthedocs.io/en/stable/manual.html#de-9im-relationships).
Die Flächenprüfung ersetzt weder ein allgemeines exaktes Boolean-System noch
den Nachweis eines vollständigen weltweiten Transportgenerators.

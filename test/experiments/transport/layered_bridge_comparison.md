# Verbundene Wiener Autobahn- und Wegbrücken (#419)

Sechs vorhandene Brückenquellen bilden zwei gegenläufige Fahrbahnen über rund
709 m; zusammen1417.793884m Achsen. Neun vorhandene Bodenanschlüsse und eine
38-m-Wegbrücke mit beiden Bodenwegen ergänzen den Versuch. Das feste
[Quellenfixture](fixtures/layered-bridges-vienna.json) enthält18 ausgewählte und
23 Kontextquellen, Originalattribute/-IDs, sechs tatsächlich berührte affine
Bodenzellen und ursprüngliche Entscheidungen. Alle3259 Szenenquellen bleiben
erhalten;2570 statt2563 verwendet. Nur18 gewählte Quellen ändern ihren Höhenbesitz.
Der alte ungenutzte U-Bahn-Tunnel erhält weder erfundene Tiefe noch neues Rezept.

## Verfahren und Grenzen

Chen et al., SIGGRAPH2008, S.2/Fig.2 trennt Graph und Geometrie; Galin et al.,
Eurographics2010, S.8/Fig.17–18 trennt Profil, Bauwerk und Terrain. Diese lokalen
Primärquellen/Abbildungen wurden optisch geprüft; Zuordnung im
[Quellenverzeichnis](../../../doc/references/README.md). Die folgende endliche
Herleitung und Implementierung sind eigene Arbeit. Kein neuer SDK-Orakelaufruf,
keine übernommene Dekompilation oder Runtime-SDK-Abhängigkeit. Einzelne
CityEngine-Geometrieoperationen beweisen keinen vollständigen OSM-Generator.

Jedes Profil hat `z(xy)=B(xy)+r(dot(xy-origin,axis))`. `B` ist das tatsächliche
stückweise affine Bodenfeld. Für jede positive Überlappung aus Quellabschnitt,
dessen ganzer Einflussfläche und Bodendreieck gilt mit Tangente `t`, Normaler `n`
und Gradient `g`: `||(dot(g,t)/L, dot(g,n)/C)|| <= 1`. Einsetzen von
`g=gradient(B)+r'*axis` liefert quadratische Wurzeln für ein zulässiges Intervall
von `r'`. Ganze Einflussflächen einschließlich Gehrungsecken werden gedeckt.
Längs-/Quergrenzen6%/4% oben und12%/4% unten; Baureserve jeweils0.002. Zusätzlich
bleibt `r'` innerhalb ±6% beziehungsweise ±12%. Der Reader prüft die angeforderten
Grenzen an tatsächlichen Double- und Float32-Facetten, ohne die Grenzen zu lockern.

Alle kritischen Intervalle werden nach außen auf Zentimeter gerundet; überlappende
Bedingungen werden geschnitten. Das kann konservativ zusätzliche Fälle verwerfen.
25/79 Stationen und7/26 Ableitungspatches entstehen. Zwei gerichtete Durchläufe
liefern die kleinsten Höhen dieser festen Differenzbedingungskette: ein Rückweg
über dieselbe Kante hat wegen `lower_rate <= upper_rate` keinen positiven Gewinn.
Alle maximalen unteren Bounds kommen somit von einfachen Wegen von links/rechts.
Verletzte obere Bounds weisen deren Unvereinbarkeit nach. Ein unabhängiges
HiGHS-LP mit positiven Trapezgewichten liefert dieselben Höhen. Das ist kein
globales Netz-/Formoptimum und keine beliebige gekrümmte oder C1-Gradiente.

Brückentagwechsel und Kachelränder sind keine Bodenanker. Ganze reale äußere
Anschlussflächen binden `r=0`. Die Knotenanordnung enthält Stationslinien,
Quellränder und echte Bodenfeldkanten; redundante kollineare Höhenknoten werden
entfernt, Dickenwurzeln eingefügt. Unterkappe `B+max(r-Dicke,0)`; obere Dicke0.50m,
untere0.25m. Ganze Fußflächen verjüngen sich ohne versteckte Unterlage zum Boden.
Alte Bodenflächen bleiben Fundament; neue Profile besitzen die aktive Verkehrshöhe.

Die getrennten7-m-Verkehrskerne haben0m² Gegenrichtungsüberlappung. Berührende
äußere Fahrbahnbänder erlauben keine logischen U-Turns; alle15 Autobahnquellen
behalten ihren gerichteten Motorverkehr. Der untere Weg erlaubt Rad/Fuß/Pferd;
deshalb gilt auch über ihm die gemeinsame4.5-m-Hülle. Körper-/Quellenbindung ist
geprüft; vollständige Fahrspur-/Abbiege-/Fahrzeughüllen und Statik sind offen.

## Wirkliche Geometrie und Teilnetz

Oberer Körper172Vertices/364Flächen, Kappen89/99; unten40/76, Kappen22/20.
Positive Volumen10699.994723/17.229890m³; Integralabweichung höchstens3.02e-10m³.
Keine kollabierte Fläche, numerische Verschweißung oder entfernte Dreiecksfläche.
Maximale ganze Feldabweichung5.69e-14m. Double-/Float32-Querschnittsquotient oben
0.966659/0.966689, unten0.554170/0.554190; zulässig bis1.

Tatsächlicher Zwischenebenenabstand Double4.52m, Float32 4.519997168m. Gegen das
alte bis0.12m hohe Randstück Float32 4.519996697m. Die erhaltene native Bodenfläche
wird an6351/232 ursprünglichen Facetten geprüft. Konservative ganze Float32-
Bodenabstände4.519944569/0.109961873m; äußerer Kontaktschritt höchstens0.05544mm.
Alle461 alten Rand-/Rampenkörper und alte Brücken/Portale/Pfeiler bleiben geschützt.

Stützen: Nennrhythmus24m oben/8m unten, maximal36m/12m freie Luftstrecke.
Feste Platzierung oben66Stützen/48m Lücke ist ungültig. Begrenztes Verschieben
(Station0/±3/±6m, seitlich0/±3/±4.5m) liefert72Stützen/27m; unten fünf/8m.
72/105 Versuche und0.270/0.310s oben; unten je5Versuche,0.026/0.022s.
Die pauschalen Quellpuffer blockierten freie Standorte. Innerhalb der Decks werden
deshalb die tatsächlichen alten Fahrbahn-/Randflächen geschützt; außerhalb bleiben
konservative Quellbänder erhalten. Keine Verkehrsfläche wird für eine Stütze gelöscht.

Ganze Kontakte stimmen bis2.85e-14m, tatsächlich quantisiert bis0.02003mm. Profilecken
sind keine Stützen: zusammenhängende Luftintervalle werden vor der Lückenprüfung
vereinigt. Der unabhängige Reader misst die Lücken aus gespeicherten Körpern und
verwirft eine absichtlich fehlende mittlere Stütze sowie einen2-mm-Kontaktversatz.
Stützen sind konstruktive Rezeptkörper ohne Statik-/Gründungsnachweis.

Der frische feste Gegenlauf verwirft nur die obere Variante mit
`support_station_gap_exceeds_recipe`. Unterer Körper/fünfStützen bleiben;2564Quellen
verwendet, alte verwendete Autobahnbodenquellen unverändert. Beide vollständigen
Szenen bestehen den gespeicherten Reader einschließlich ursprünglicher Arraypräfixe,
aller neuen Normale/Farben und vollständigem Quellenledger. Keine neue Quellenhöhe
aus einem bloß erfolgreichen Prozessabschluss.33 lokale Kontrollen bestanden;
der frühere #404-Fall bleibt in allen sechs Mesh-/Tabellenarrays bytegleich.

## Kosten, Bild und Reproduktion

Erzeuger/Reader/Bilder/Kosten eingefroren auf `28fd18a42afb10e6a9c9f453b8526503c6c8ed12`.
Python3.13.5, NumPy2.2.4, SciPy1.18.1/HiGHS1.12.0, Shapely2.2.0/GEOS3.14.1.
Frische Offline-Prozesse nacheinander: vollständig5.835489s/283228KiB Peak-RSS,
Teilnetz6.018049s/283160KiB. Vollszene434602Vertices/784820Flächen; Zuwachs936/1580.
Besitzende Szenenarrays66937488 statt66801264Bytes; NPZ-Decode, Geometrieprüfung,
beide Methoden und Kompression enthalten. Keine Echtzeit-/Native-/Gerätemessung.

Beide Solves zusammen:11 normale Wiederholungen nach warmen Imports/LPs,
OPENBLAS/OMP je1, keine eigenen Renderer mehr aktiv. Median Kette0.628202ms,
LP7.409924ms;204 Relaxationen statt408 Sparse-Nonzeros/5320 Sparse-Bytes.
Identische4128Input-/832Output-Bytes. Kette kopiert explizit832Lower-Bound-Bytes;
keine Totalzählung interner Kopien behauptet. Normale Worker-RSS86212608/86175744B.
Separate Memray1.20.0-Aufnahmen:2982/42787Allokationsereignisse,
155110/3791005angeforderte Bytes,7256/551093B getrackter Heap-Peak.
Instrumentiert23.578/98.337ms; keine Ableitung eines Gesamtgeneratorfaktors.

Beide tatsächlichen Gesamtbilder1280×720 wurden geöffnet; gleiche1450-m-Kamera,
Licht/Farben, Blender5.2.2/EEVEE/llvmpipeLLVM19.1.7,16Samples, zwei Threads/nice10.
Vorher18.471s/2323868KiB, nachher38.610s/2336792KiB, verschiedene Startzustände;
das ist kein Renderleistungsvergleich.11285Pixel geändert,10801 über1/255,
max118/255. Sichtbar ist der neue Brückenschluss; der schlichte Forschungslook
und die geringe regionale Größe der unteren Brücke bleiben. Keine Galerie im Repo.

Kanonische lokale Belege: `/tmp/outshine-transport-layered-419-shifted-28fd18a42`,
Teilnetz `outshine-transport-layered-419-fixed-28fd18a42`, Kosten
`outshine-transport-layered-419-cost-28fd18a42`; JSONs, Original-NPZs, Ledger,
PNGs/Receipts, Pixelbericht und getrennte Allokationstraces. Explizite Pfade im
[README](README.md); bestehende Eingänge/Hashes vollständig im Fixture und JSON.
Fixture-SHA256 `71f76677dee5df5ddd6480f4af0ef5cb48fbcf178e5e8c9e4abf52c4fa4e0fd1`.
Volle Szene `a701c8aaf3b9440197d4e75bdd1e349c8955b6b6c06702b6c579560984bde43d`.
Native Integration, weitere Quellen, gemeinsame Verbraucherbindung und Bibliotheks-
Übernahme eines allgemeineren Profilplaners bleiben getrennte nächste Arbeiten.

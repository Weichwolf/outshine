Type: feature
State: active
Architecture: planned
Priority: P0
Area: engine, world, render, simulation, audio
Tags: sandbox, visual, integration

# Outshine: acht glaubwürdige Webcam-Welten im 720p60-Budget

## Jetzt erreichen

Ich entwickle eine prozedurale Open-World-Sandbox. Der aktuelle Meilenstein ist die
bestmögliche plausible Annäherung an acht Foto-Webcams über Jahres- und Tageszeiten,
Wetter und Kameradrehung. Der Maßstab ist das geöffnete Bild innerhalb des Budgets.
OSM-Originaldaten, DEM, Wetter, Zeit und Kamera liefern die Welt; Fotos nur den Vergleich.
Die Auswahl und Quellen stehen in WI 2324. Physik, Runden, Verkehrssimulation und Spiel
folgen später. Straßenfortschritt erhalten; Brücken und komplexe Anschlüsse reparieren.

## Was die Archivbilder verlangen

Rosenheim zeigt reale Dach-/Fassadenunterschiede, schlanke Sonderbauten, abgestufte Berge,
wechselnde Sichtweite und nachts lokale Lichtinseln. Flensburg zeigt Backstein, Giebel,
einen maßgebenden Kirchturm, Hafen und jahreszeitlich wechselnde Kronen. Koerbersee zeigt
scharfe Grate, Schutt, Schnee nach Exposition, einen reflektierenden See und Wolken im Relief.
Die übrigen fünf Ansichten ergänzen dichte Stadt, Brücken, Kai, Tal und felsige Seeufer.

Geöffnete Outshine-Bilder zeigen dagegen repetitive Fenstergitter, generische Türme,
plastische Geländeformen, flache Wasserflächen und leeren Himmel. Kamerafit ist teilweise
ungeprüft. Das sind konkrete Bildlücken; grüne Tests oder weitere interne Verträge schließen sie nicht.
Archivbefunde: build/shots/reference/webcams/archive-review-20260929/ mit Einzelmetadaten.

## Lieferweg nach Wirkung und echten Abhängigkeiten

| Priorität | Sichtbare Lieferung | WI | Technischer Zusammenhang |
|---|---|---|---|
| P0 | Acht passende Perspektiven und vollständige residente Welt | 2324, 2170, 2319, 2322 | Kamera getrennt von Quell-/LOD-/Publikationsfehlern lösen |
| P0 | Richtige Großformen, durchgehende Straßen, korrekte Wasser-/Hanganschlüsse | 2166, 2280, 2173, 2281, 2145 | Originalsemantik/DEM; bestehende Straßen erhalten |
| P1 | Lesbare Dächer, Baustoffe und räumliche Fassaden | 2171, 2138 | Materialpfad vorhanden; OSM-Parts/Klassen bestimmen Form |
| P1 | Zusammenhängendes Sonnen-/Himmelslicht, Schatten und Belichtung | 2167, 2128, 2155 | Gültige Oberflächen; Kameraantwort ohne fertige Vegetation liefern |
| P1 | Wolken, Dunst und Nebel ändern Himmel UND Weltbeleuchtung | 2172, 2140 | Gemeinsamer Wetter-Snapshot; kein fertiger Regen-/Schneesolver nötig |
| P1 | Wasser reflektiert Berge, Stadt und Himmel mit windabhängiger Oberfläche | 2145, 2129 | Korrekte Wasserfläche; vorhandene Szene als Reflexionsquelle |
| P1 | Schnee, Schmelze und Nässe ändern dieselben Materialien plausibel | 2325 | Wetterhistorie/Zeit und Materialzustand; keine weiße Höhenmaske |
| P1 | Nacht bleibt Stadt: Fenster, Straßenlicht, Reflexion und dunkler Himmel | 2128, 2155, 2213 | Lokale Emission/Beleuchtung und Kameraantwort |
| P2 zuletzt | Vegetation schließt Dichte, Silhouette und Jahreszeiten | 2111, 2176, 2282 | Standorte, gemeinsame LODs und verbleibendes Szenenbudget |

Nicht auf alle Systeme warten: Jede Lieferung verbessert ein sichtbares Teilbild in
derselben Welt. Himmel und Wolken erhalten hohes Gewicht; Mikrodetails ersetzen weder
Großform noch Licht. Kleine ausführbare Reserve und Owner stehen in 2188.

## Dauerhafte Grenzen

Alle acht Places rendern nach Änderungen und persönlich mit erhaltenen Bildern vergleichen.
240 km konfigurierte Sicht und Rundum-Verfügbarkeit erhalten. Höchstens zehn Sekunden
Preload, dann 60 Frames/360° in einer Sekunde und nur das letzte Hash-PNG. p99 <= 1000/60 ms.
Host-Kosten und A18 Pro 8-GB-Nachweis getrennt; Speicherbudget schließt OS-/Treiberreserve ein.
Stadt, Terrain, Wasser, Himmel und später Wald teilen dieses Budget ohne feste Klassenquoten.
Nur Netzwerkquellen persistent cachen; unveränderte Welt bleibt resident.

Es geht um plausible Gesamtwirkung, nicht die exakte Wolke, parkende Autos oder jede
unbekannte Bauzier eines Fotos. Schnee braucht Wettergeschichte; Tagesdatum allein reicht nicht.
Keine Orts-Sondermodelle, Satellitenbilder, Foto-Texturen oder verschleiernde Unschärfe.

## Spätere Sandbox

Nach dem visuellen Meilenstein folgen Gehen/Fahren mit Kontakten, Verkehr und Figuren,
Interaktion, Audio, Aufgaben und persistenter Spielzustand. Vorhandene Fähigkeiten bleiben
erhalten; ihre Weiterentwicklung verdrängt jetzt keine sichtbare Webcam-Annäherung.

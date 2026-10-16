# Östliche Radwegschleife am Wiener Treppenverbund (#434)

Der Versuch erweitert den [18-Quellenstand](stair_outer_comparison.md) um den ganzen
ursprünglichen Radweg `openfreemap:14/8937/5678:transportation:32651280:3`.
Beide bisherigen östlichen Außenprofile von `39881120:34` erhalten ganze gemeinsame
Kappen. Ein Kontakt besitzt einen ursprünglichen MVT-Punkt, der andere ist ein
explizit gewählter Bandkontakt bei 0,130762 m Achsabstand. Die Originalachse wird
nicht auf einen erfundenen Quellknoten gesnappt. Nur Quellkoordinaten zu verbinden
verfehlt diesen tatsächlichen Bandkonflikt und wird abgelehnt.

Alle bisherigen Achsen und Priors bleiben erhalten, einschließlich der schon zuvor
gewählten Faltauffahrt. Die gemeinsamen Höhen werden für das größere Netz neu
geplant; der vorige Körper bleibt eigene unveränderte Regression, kein behauptet
bytegleicher neuer Körper. Drei Treppen und sechs tatsächliche Unterführungen
behalten ihre Bedingungen. Drei frühere und zwei neue äußere Profile bleiben offen;
das Stationsende `indoor=1, level=0` ist kein gewöhnlicher Erdbodenanschluss.
Globale Quellfreigabe bleibt leer, native Übernahme offen.

Die neue Achse reicht in den PBF-Kachelbuffer über den östlichen DEM-Rand.
Zwei ausdrücklich erhaltene reale DEM-Fenster werden nach ihrer tatsächlichen
Kachel ausgewählt. Kein Clamping/Extrapolieren der ursprünglichen Höhenkachel und
kein behaupteter kachelübergreifender OSM-Netznachweis. Die neuen Terrainflächen
werden nach dem [kontinuierlichen Böschungsverfahren](stair_blend_comparison.md)
geplant und mit ganzen ursprünglichen Flächen sowie tatsächlichem Float32 geprüft.
Mengen beziehen sich weiterhin nur auf das affine DEM-Modell.

Verfahren: Chen et al. SIGGRAPH 2008 (Netz/Form trennen), Galin et al. Eurographics
2010 (Profile/Bauwerke/Terrain), Desbrun et al. SIGGRAPH 1999 (Membranenergie),
bereits optisch geprüfte Primärquellen unter [Transportreferenzen](../../../doc/references/transport/README.md).
Eigene Geometrie-/Optimierungszusammenstellung; kein Fremdcode oder SDK in der Runtime.

Die [Planungseingabe](fixtures/stair-cycle-vienna.json) enthält keine gelösten Höhen.
[Aufrufe und Pakete](README.md). Gespeicherte Exporte werden ohne erneuten Solver,
CDT oder Körperbau geprüft. Normale DS-/IPM-Zeiten und separat profilierte
Allokationen müssen dieselben ganzen Qualitätsbedingungen bestehen; ein
Fristabbruch hat keine gültige Ausgabemediane. Kein nativer/Geräte-/Framefaktor.

Die lokale Lieferung wird erst nach frischer Erzeugung, unabhängigem Lesen und
tatsächlicher Bildprüfung im PR als geprüft ausgewiesen. Übrige Quellen/Ebenen,
Fahr-/Gehhüllen, Radien, Statik und allgemeine Selbstschnittfreiheit bleiben offen.

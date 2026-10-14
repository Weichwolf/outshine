# Sonderbauwerke: lokale SDK-Referenz

Untersuchte Installation: `~/Downloads/esri_ce_sdk-3.4.12206`.
Core-SHA256: `850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
Auftrag und eigener Provider-/Python-Vertrag: [#130](https://github.com/Weichwolf/outshine/issues/130).

Der konkrete SDK-Baum enthält null `.cga`, `.cgb`, `.rpk`, `.obj` und `.glb`.
Vier CGA-Beispielregeln liegen separat unter `~/Downloads/cityengine-reference-rules/`;
darunter Candler Building, Extrusion und Translation. Sie belegen keine vollständige
Sonderbau-Rezeptbibliothek. Die CityEngine-Anwendung besitzt eine zusätzliche
Regel-/Assetbibliothek; sie ist in dieser Installation nicht enthalten.

## Modelliermechanismen

Lokale CGA-Dokumentation: `doc/html/cgaref/cgareference/op_primitive.html`,
`op_taper.html`, `op_comp.html`, `op_split.html` und `op_i.html` im SDK.
Zylinder/Kegel erzeugen parametrisierte Grundkörper; `taper` baut verjüngte
Extrusionen. Komponenten, Aufteilung, Transformation und Asset-Einfügung
komponieren Sonderformen. Die Zylinderdokumentation nennt mindestens drei,
standardmäßig 16 Seiten, expliziten Radius/Höhe oder Einpassen in den Scope,
XZ-Zentrierung auf Y=0 bei expliziten Maßen und UVs im ersten Texturkanal.
Zylinder-/Taper-Abbildungen wurden optisch geöffnet.

Das Grundverfahren folgt [CGA Shape, SIGGRAPH2006](../siggraph/2006-procedural-modeling-of-buildings.pdf):
parametrisierte Formen und Regeln statt einer fest verdrahteten Funktion je Bauwerk.
Die eigenen Strukturinterpretationen erzeugen noch keinen solchen Modellierbaum.

## Begrenzte Ghidra-Prüfung

`esri_special_reference.py` untersucht frische Symbolkörper von
`Processor::primitiveCylinder(double,double,double)`,
`Processor::primitiveCone(double,double,double)`, `Processor::taper(double)`
und dem spezialisierten `util::CylinderMesh`-Konstruktor. Vier Decompilierungen
werden auf vollständigen Export und terminalen Headless-Status geprüft.
Explizite Primitive dispatchen Asset-Einfügung; der Zylinderpfad besitzt
Unterteilungs-/Mindestseitenprüfungen. Dies ist eine Mechanismenreferenz,
kein Schornstein-/Antennen-/Wasserturm-Rezeptorakel. Ghidra-Typen, indirekte
Aufrufe und vollständige ABI-/UV-/Normalengleichheit sind nicht bewiesen.
Export, Referenzskript-/Binary-Hashes und Laufumfang liegen im Build-Report;
Fremdcode wird nicht eingecheckt.

## Datenbedingte Grenzen

Outshines bevorzugter OpenFreeMap-Buildinglayer liefert Umriss, aufbereitete
Ober-/Unterkante, Farbe und `hide_3d`. Ein hoher Rundkörper beweist keine
Funk-/Wasser-/Wohnnutzung; Industriegebiet und Form ergänzen plausible Familien.
Der POI-Layer liefert lokale Klassen ohne vollständige ursprüngliche OSM-Tags.
Namen und bekannte Orte wählen ausschließlich Prüffälle. Fehlende Punktobjekte
oder Typen bleiben fehlend. Quelloberkante ist keine vermessene Originalhöhe.

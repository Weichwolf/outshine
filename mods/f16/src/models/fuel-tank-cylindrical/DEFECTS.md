# fuel-tank-cylindrical — offene Defekte

> Der Startpunkt der naechsten Runde. `doc/assets.md` §1: *„The critic's open defects survive"* — was
> hier nicht steht, entdeckt die naechste Runde noch einmal von vorn.
>
> **Runde 1** (Erstbau). Kritiker: noch keiner. Diese Liste ist die Selbstanzeige des Modellierers,
> und sie ist deshalb mit Sicherheit unvollstaendig — ein Kritiker findet Klassen von Fehlern, die
> der Erbauer nicht sehen kann.

## Was in dieser Runde ZU ist

| | Beleg |
|---|---|
| Alle Hauptmasse aus einer nachpruefbaren Quelle | API Std 650 (11. Aufl., law.resource.org), Normgroessentabelle, OSHA 1910.25 — jede Zahl mit Klausel-/Tabellennummer in `fuel_tank_geometry.py` |
| Selbstpruefungen `doc/assets.md` §3.1 im Bauschritt | 125 / 125 / 89 / 34 Koerper, alle gruen: dicht, Windung, Normalen, T-Stoesse, Verschweissung, Merge |
| Bau zweimal bytegleich | drei Laeufe, drei Verzeichnisse, vier `.glb` + `.asset.json` identisch (`cmp`, `sha256`) |
| Silhouette springt nicht | XOR-Tor im Bauskript, alle Uebergaenge unter 2 %: max 1.81 % (L2→L3 front) |
| Umschaltweiten hergeleitet | 13 / 51 / 205 m aus Rundungsfehler und Cauchy-Merkmalsgroesse, nicht gesetzt |
| Drei Fehler, die kein Auge gefunden haette | Randkanten an jedem Polring (falsche Faecherwindung) · zwei Koerper mit Volumen null (Querschnittsebene parallel zum Pfad) · Eck-Normalen genau gegen ihre Flaeche (pauschales Negieren in `orient`) — alle drei von der Selbstpruefung gemeldet, bevor ein Bild existierte |

---

## Offen, gereiht

Reihung = wie stark der Punkt das Bild oder die Richtigkeit aendert.

### 1 — Der Tank steht nackt in der Landschaft

Kein Auffangwall, keine Rohrleitung am Bodenstutzen, kein Schaumrohr, keine Beschriftung, kein
Gefahrgutschild. Ein Treibstofflager ist nie ein einzelner Zylinder auf gruener Wiese — es ist ein
Tank in einer Wanne mit einem Verteiler daneben. **Das ist der groesste Abstand zwischen „richtiges
Objekt" und „glaubwuerdige Szene".**

*Schliessen:* entscheiden, ob Wanne und Verteiler zu diesem Asset gehoeren oder eigene Eintraege der
Inventarliste sind. `doc/asset-inventory.md` kennt sie nicht — also ist das eine Frage an den
Eigner, keine Modellierentscheidung.

### 2 — Keine Textur, und der Shader, der sie ersetzen soll, existiert nicht

Bewusste Entscheidung (`doc/render/visual-target.md` §1: Bandbreite ist der Mangel): Rost,
Nahtverfaerbung, Laufspuren und Aufschrift sind an einen prozeduralen Shader delegiert. **Den gibt
es nicht.** Bis dahin ist der Tank gleichmaessig sauber, und das sieht man.

*Schliessen:* Shader bauen ODER die Entscheidung umkehren und eine 512er Kachel backen. Nicht
liegenlassen — die Entscheidung ist heute eine Schuld, keine Ersparnis.

### 3 — Kein Zerstoerungszustand

Comanche macht diesen Tank in 4 von 10 Missionen zum Ziel (*„destroy the fuel tanks"*,
`mods/comanche/doc/campaign.md`). Es gibt keine zerstoerte Fassung, keinen Brand, keinen Einsturz.

*Schliessen:* zweites Netz aus derselben Quelle — aufgerissene Schale, eingesunkenes Dach. Der
parametrische Aufbau macht das billig, aber es ist eine eigene Runde.

### 4 — Windring: Profil gesetzt, nicht ausgewaehlt

`kWindGirderLeg = 100 mm`, `kWindGirderThk = 8 mm` sind **[SET]**. Das erforderliche
Widerstandsmoment ist gerechnet (Z = 63.3 cm³, API 650 5.9.7.6), aber die Profilauswahl aus
**Tabelle 5-20** wurde nicht durchgefuehrt. LAGE und NOTWENDIGKEIT des Rings sind belegt, seine
GROESSE nicht.

*Schliessen:* Tabelle 5-20 fuer t = 5 mm lesen, kleinstes Profil mit Z ≥ 63.3 cm³ nehmen.

### 5 — Dachentlueftung frei erfunden

`kVentDia = 200 mm`, eine Haube, keine Notentlueftung, kein Peilstutzen. API 650 5.8.5.2 verweist
auf **API Std 2000**; diese Rechnung wurde nicht gefuehrt. Ein Tank mit 1640 m³ braucht bei
realistischer Fuellrate wahrscheinlich mehr oder groessere Ventile.

*Schliessen:* API Std 2000 fuer die Fuell-/Entleerrate rechnen; daraus Zahl und Groesse.

### 6 — Sechs weitere gesetzte Masse

| Konstante | Wert | Warum ungedeckt |
|---|---|---|
| `kRingwallRise` | 0.300 m | API 650 Bild B-1 traegt ein Mass „0.3 m (1 ft)", aber das Bild ist im PDF eine Rastergrafik — der Pfeil laesst sich per Textextraktion keiner Kante zuordnen. **Am Bild pruefen.** |
| `kShellManholeBolts` | 20 | Tabelle 5-5a liefert Lochkreis und Deckel fuer DN 600, nicht die Schraubenzahl; 20 stammt aus der Dachmannloch-Tabelle per Analogie |
| `kNozzleFlangeDia` | 279.4 mm | ASME B16.5 Klasse 150 NPS 6 — B16.5 selbst wurde nicht geholt |
| `kStairTreadThk` `kNosing` `kStringerH/Thk` `kRailTubeDia` `kPostDia` | 30 / 25 / 200 / 10 / 42 / 48 mm | API 650 bindet die GEOMETRIE der Treppe (Steigung, Auftritt, Winkel, Breite, Handlaufhoehe, Pfostenabstand), nicht die Bauteilquerschnitte. API 650 5.8.10 b nennt **PIP STF05501 / STF05520 / STF05521** als Regeldetails; diese Blaetter wurden nicht beschafft |
| Schweissraupe | 5 x 60 mm | keine Quelle; eine echte Rundnaht hat rund 6–10 mm Kronenbreite. **Sie ist zu breit und zu flach** |
| Bodenblech (`bottom_lip()`) | 25 mm Ueberstand, 6 mm dick | API 650 5.4.2 legt den Ueberstand des Bodenblechs ueber der Kehlnaht fest; die Klausel wurde in dieser Runde nicht gelesen |

### 7 — Materialwerte ohne Messung

Alle vier PBR-Saetze sind **[SET]**. Kein Foto, kein Messwert, keine Referenztafel. Der Anstrich
(0.64 Albedo) wirkt im Beweisbild ueberstrahlt.

*Schliessen:* eine Referenzaufnahme eines gestrichenen Lagertanks und eines verzinkten Gelaenders
heranziehen, Albedo und Rauheit daran binden.

### 8 — Umfangslagen willkuerlich

Mannloch 300°, Stutzen 345°, Dachmannloch 30°, Entlueftung 70°, Treppenfuss 130°. Keine Quelle
schreibt sie vor. Der Treppenfuss wurde auf 130° gedreht, **damit die Seitenansicht des
Beweisrenders die Treppe zeigt** — eine ehrliche, aber sachfremde Begruendung.

*Schliessen:* Regel statt Zahl. Real richtet sich der Treppenfuss nach dem Zugangsweg und das
Mannloch liegt bewusst NICHT unter der Treppe. Als Parameter des Platzierers exportieren.

### 9 — Kein Dachtragwerk, kein Innenraum

Das getragene Kegeldach hat weder Sparren noch Mittelstuetze (API 650 5.10.4). Von aussen
unsichtbar — aber das Dachmannloch oeffnet auf nichts, und ein durchschossener Tank zeigt eine
leere Huelle.

### 10 — Kein Koerper fuer die Physik

`doc/body-format.md` §1 will SEGMENT und CONTACT. Die `.glb` traegt nur Sichtgeometrie. Solange das
Format nicht steht, ist das kein Fehler dieses Assets — aber es ist ein Loch.

### 11 — Zwei Luecken IN DER PRUEFUNG selbst

- **T-Stoesse werden nur JE KOERPER geprueft.** Wo zwei Koerper aneinanderstossen (Stufe an Wange,
  Treppe an Schale), prueft nichts. Ein Haarriss zwischen zwei Netzen faende diese Pruefung nicht.
- **Die kumulierte Silhouette L0→L3 liegt in der Frontansicht bei 2.03 %** und damit knapp ueber der
  Grenze, die fuer die EINZELUEBERGAENGE gilt (max. 1.81 %). Ob die Summe ihre eigene, groessere
  Grenze bekommt oder ob L3 groeber sein darf als es ist, ist eine offene Entscheidung — heute steht
  die Zahl nur da.

### 12 — Keine UV-Koordinaten

Bewusst (kein Texturbedarf), aber es heisst: wer je eine Textur will, muss zuerst abwickeln.

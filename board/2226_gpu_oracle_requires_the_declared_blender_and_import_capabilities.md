Type: bug
State: active
Parent: 2094
Area: test, khronos, oracle
Tags: GPU, reproducibility, Blender
Depends: 2094

# GPU oracle requires the declared Blender and import capabilities

## Problem

Drei 5.2.1-Pins lagen nur im flüchtigen Cache; ihre Bytes existierten weder lokal noch
als Git-Objekte. Ein vollständiger GPU-Lauf reproduzierte 337/340 Referenzen. Die drei
neu erzeugten Bilder sind plausibel, aber `SheenWoodLeatherSofa` ändert 2.039 Float-
Komponenten allein mit dem Seed; der deklarierte estimatorfreie Render war unbelegt.
Die lokale glTF-Importgrenze bleibt: Blender
akzeptiert `KHR_node_visibility` nicht und wirft bei `AnimationPointerUVs` einen
`KeyError: animations`.

## Decision

Cycles 5.2.1 LTS auf verifiziertem METAL/GPU ist für die drei erneuerten Bilder der
aktuelle Orakelstand. Nur vorbereitete `oracle.raw`-Produkte mit gesicherter
Provenienz, korrekter Auflösung und visueller Prüfung dürfen über
`reference_from_oracle.py` gepinnt werden; der Cache prüft danach die neuen Bytes.
Jeder deklarierte `seed-shift` wird pro Frame gegen den Default-RAW-Digest geprüft;
Abweichung verweigert die Vorbereitung statt ein zufälliges Bild zu legitimieren.
Keine CPU-Fallbacks und keine automatische Aktualisierung im Normaltest. Die drei
Importerfehler bleiben getrennte offene Fälle: Extension-Adapter oder ein passendes
GPU-Orakel entscheiden erst nach fachlicher Prüfung.

## Proof

- Cycles-Provenienz der drei erneuerten Aufnahmen: Blender 5.2.1 LTS, METAL, GPU
  `Apple A18 Pro (GPU - 5 cores)`; Bilder visuell geprüft.
- Alle 340 deklarierten Referenzaufnahmen validieren Digest, Auflösung und Framezeit.
  `make test-reference-cache` ist grün; normale Tests erzeugen keine Pins.
- Positiv-/Negativkontrolle des Seed-Vergleichs grün; Sofa wird mit beiden beobachteten
  RAW-Digests ausdrücklich verweigert.
- Ein verändertes Byte und eine fehlende Cache-Datei bleiben Negativkontrollen.
- `CubeVisibility`, `LightVisibility` und `AnimationPointerUVs` besitzen weiterhin
  bewusst keine Pins, bis ein GPU-Orakel sie korrekt importiert.

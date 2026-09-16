Type: bug
State: active
Parent: 2094
Area: test, khronos, oracle
Tags: GPU, reproducibility, Blender
Depends: 2094

# GPU oracle requires the declared Blender and import capabilities

## Problem

Drei 5.2.0-Pins lagen nicht mehr im externen Cache; ihre Bytes existieren weder lokal
noch als Git-Objekte. Blender 5.2.1/Cycles METAL erzeugt für `DirectionalLight`,
`PointLightIntensityTest` und `SheenWoodLeatherSofa` andere, vollständige GPU-Bilder.
Der alte Vertrag verlangte damit eine nicht verfügbare historische Toolversion statt
eines prüfbaren unabhängigen Orakels. Die lokale glTF-Importgrenze bleibt: Blender
akzeptiert `KHR_node_visibility` nicht und wirft bei `AnimationPointerUVs` einen
`KeyError: animations`.

## Decision

Cycles 5.2.1 LTS auf verifiziertem METAL/GPU ist für die drei erneuerten Bilder der
aktuelle Orakelstand. Nur vorbereitete `oracle.raw`-Produkte mit gesicherter
Provenienz, korrekter Auflösung und visueller Prüfung dürfen über
`reference_from_oracle.py` gepinnt werden; der Cache prüft danach die neuen Bytes.
Keine CPU-Fallbacks und keine automatische Aktualisierung im Normaltest. Die drei
Importerfehler bleiben getrennte offene Fälle: Extension-Adapter oder ein passendes
GPU-Orakel entscheiden erst nach fachlicher Prüfung.

## Proof

- Cycles-Provenienz der drei erneuerten Aufnahmen: Blender 5.2.1 LTS, METAL, GPU
  `Apple A18 Pro (GPU - 5 cores)`; Bilder visuell geprüft.
- Alle 340 deklarierten Referenzaufnahmen validieren Digest, Auflösung und Framezeit.
  `make test-reference-cache` ist grün; normale Tests erzeugen keine Pins.
- Ein verändertes Byte und eine fehlende Cache-Datei bleiben Negativkontrollen.
- `CubeVisibility`, `LightVisibility` und `AnimationPointerUVs` besitzen weiterhin
  bewusst keine Pins, bis ein GPU-Orakel sie korrekt importiert.

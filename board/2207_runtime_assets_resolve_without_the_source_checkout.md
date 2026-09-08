Type: defect
State: open
Parent: 2188
Area: client, render, assets, build
Tags: architecture, audit
Depends: 2152

# Runtime assets resolve without the source checkout

## Beleg und Auswirkung

Quellaudit 2026-09-08: SurfaceOutputs::Vertex/Fragment und zahlreiche Stage::Configure
bauen Pfade unter build/shaders. ShaderFile::ReadShaderFile öffnet sie mit fopen relativ
zum Prozess-CWD. Roots::Shipped wird dafür nicht verwendet. Main::Stands und OpenPlace
setzen außerdem src/assets/drive, src/assets und /tmp/outshine-drive-cache fest.
Ein installierter Client oder Host aus anderem Arbeitsverzeichnis benötigt damit implizit
den Quellbaum. Die Existenz der Roots-API allein löst das nicht.

## Entscheidung

Benchmark: installierbare Engine-/Assetpakete mit explizitem Resource-Resolver.
Shader-IDs und generierte Varianten bleiben Engine-Wissen; Speicherort, Asset-/Cache-/
Ausgabewurzeln sind Hostkonfiguration. Kein Prozess-chdir und keine absolute Entwicklerpfade.
Build erzeugt vollständiges Ressourcenmanifest samt Version/Hashes; Resolver liefert Bytes
und eindeutige Fehler. Public Engine und Client verwenden denselben Resolver. Client erhält
übersteuerbare Wurzeln, plattformgerechte Defaults und dokumentierte Szenario-relative URIs.

## Abnahme

- [ ] Paket außerhalb Checkout aus fremdem CWD starten; glTF und Place-Szenario rendern.
- [ ] Alle Shader-Varianten aus Paket nachweisbar; fehlendes Artefakt liefert Fehler vor Draw.
- [ ] Zwei Instanzen mit verschiedenen Assets/Caches beeinflussen einander nicht.
- [ ] Negativkontrolle: Quellbaum nicht erreichbar; kein stiller Fallback auf build/shaders.
- [ ] PNGs vor/nach Resolverwechsel identisch; Make-Lint und Paket-Smoke grün.

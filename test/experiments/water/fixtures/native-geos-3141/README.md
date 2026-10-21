# Nativer Integrationsgegenfall: neue physische Ill-Triangulierung (#760)

Die unveränderte `compare_physical_river.inputs`-Aufbereitung liefert mit der in
`case.json` angegebenen Umgebung 1494 Vertices, 1495 Faces und 1619 positive
Quellmitgliedschaften. Der Original-WATER2-Hash und die geografische Auswahl bleiben
unverändert; die vorherige Referenz hatte 1492/1492/1607. Die Differenzursache ist
noch offen. Diese Dateien sind eine feste numerische Gegeneingabe, kein neuer
Produktcache, kein zulässiger Fit und keine Genauigkeitsvorgabe.

Der ursprüngliche physische lineare QP-Aufruf scheitert mit `InsufficientProgress`
nach einer Iteration. Höhen-/Neigungs-/Kontaktbedingungen wurden nicht gelockert.
Die Solvermatrizen wurden anschließend bei identischer Aufbereitung direkt vor
`clarabel.DefaultSolver` abgegriffen; beim Abgriff lief kein weiterer Solver.
Originalquellen, Versionen, Zahlen und Dateihashes stehen einmal in `case.json`.

| Datei | Inhalt |
|---|---|
| `physical-inputs.npz` | ENU-Ziele/XY, Dreiecke, Richtungen, Quell-CSR, Kurse, Kappen, Rahmen/Prisma und primäre Zeilen |
| `physical-hessian.npz` | Tatsächlich übergebene obere CSC-Dreiecksmatrix P |
| `physical-constraints.npz` | Tatsächlich übergebene CSC-Bedingungsmatrix A |
| `physical-vectors.npz` | Tatsächlich übergebene q-/b-Vektoren als linear/bound |

Diese Instanz besitzt 1494 Unbekannte und 13433 nichtnegative Kegelzeilen, keine
harten Punktkontakte und keine Hilfs-/SOCP-Variablen. Es gilt `A*x+s=b`, `s>=0`.
P/q beziehen sich auf den im bestehenden Referenzkern gewählten ENU-Höhendatumoffset;
die Geometrie-ENU-Ziele selbst bleiben unverändert. NPZ-Dateien enthalten keine
Pickle-Objekte. Der ursprüngliche vollständige Solverstatus steht in `python_fit_result`.

Direkter Replay der exakten endlichen QP-Daten, ohne neue Triangulierung:

```sh
python - <<'PYTHON'
from pathlib import Path
import json
import clarabel
import numpy as np
from scipy import sparse
p = Path('test/experiments/water/fixtures/native-geos-3141')
v = np.load(p / 'physical-vectors.npz', allow_pickle=False)
settings = clarabel.DefaultSettings()
settings.verbose = False
settings.max_threads = 1
settings.max_iter = 200
settings.time_limit = 10
settings.tol_feas = settings.tol_gap_abs = settings.tol_gap_rel = 1e-11
result = clarabel.DefaultSolver(
    sparse.load_npz(p / 'physical-hessian.npz'), v['linear'],
    sparse.load_npz(p / 'physical-constraints.npz'), v['bound'],
    [clarabel.NonnegativeConeT(len(v['bound']))], settings,
).solve()
print(json.dumps({'status': str(result.status), 'iterations': result.iterations}))
PYTHON
```

Ein erfolgreicher Solverstatus wäre allein noch keine Abnahme: physische
Whole-face-Grenzen, Quellabdeckung, Kontakte und endliche Primal-/Dualresiduen
müssen anschließend unabhängig geprüft werden. Der Gegeneingabesatz erzwingt
keinen bestimmten Fehlerstatus auf anderer Software/Hardware. Er ermöglicht
Research die Ursachenprüfung und denselben nativen Portvergleich ohne erneute
plattformabhängige Triangulierung. Gemeinsamer Vertrag: [Wasserprofile](../../../../../lib/water_profiles.md).

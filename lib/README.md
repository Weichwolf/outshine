# Outshine Python-Referenzen

Python-Referenzverfahren des Researchers (`outshine_ref`): Transport, Gebäude, Vegetation,
Geometrie, Wasser, Materialien. Sie entscheiden Verfahren an echten Daten und liefern
Rezepte; die native Umsetzung liegt unter `src/`.

```sh
.venv/bin/python -m pip install -e ./lib
.venv/bin/python -m unittest discover -s lib/tests -v
```

Regeln:
- Ein Modul bleibt nur, solange ein offenes Issue es braucht oder der native Code noch
  dagegen geprüft wird. Nach der nativen Übernahme wird es gelöscht.
- Ergebnisse stehen als Kommentar im Issue, nicht als Markdown-Protokoll hier.
- Der Produktbuild liest genau eine Datei aus diesem Baum:
  `src/outshine_ref/vegetation/data/leaf-optics-table-1.json`.

Type: bug
State: active
Area: scenario, format
Parent: 2185
Depends:

# XML flags will read the scenario writer's boolean values

ScenarioWrite::Yes schreibt `yes/no`; Xml::Ref::Flag erkennt nur `true/false/1/0`.
Dadurch wird explizites `vegetation="no"` als Default true eingelesen.
ForestInstancesKeepTheirSpecies: 252 Checks, zwei XML-Fehler im Lauf
`build/scenario-vegetation-isolation.log`; Katalog-Isolationsprüfungen bestanden.

**Benchmark**: Neither Unreal nor RAGE defines this local XML dialect. The choice
is mine: gemeinsame Flag-Konvertierung um die vorhandenen Writer-Werte erweitern,
bestehende Schreibweisen erhalten. Kein Sonderparser für Vegetation.

- [ ] yes/true/1 und no/false/0 ergeben jeweils denselben Booleschen Wert.
- [ ] Writer → Reader erhält aktivierte und deaktivierte Features.
- [ ] Fehlendes Attribut erhält den deklarierten Default.
- [ ] Unveränderter alter Parser verletzt die Roundtrip-Prüfung.

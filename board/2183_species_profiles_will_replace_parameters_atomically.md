Type: defect
State: active
Area: generators
Depends:

# Species profiles will replace parameters and their source atomically

TreeSpecies::Parse verwendet bestehende Felder als Defaults und schreibt vor der
Validierung in das lebende Objekt. Ein erneut gelesenes vollständiges Profil kann
somit Parameter des vorherigen behalten; ein verworfenes Profil kann das Objekt
teilweise ändern. Ein Kronencache-Schlüssel aus dem neuen Profiltext würde dann
nicht eindeutig den tatsächlich verwendeten Generatorzustand repräsentieren.

Entscheidung vor Implementierung: in ein frisch initialisiertes TreeSpecies lesen,
erst nach vollständiger Validierung austauschen. Bei Fehler bleiben Definition und
Parameter erhalten, Error nennt die Ablehnung. Erfolgreiche Parse übernimmt den
exakten Quelltext als eigene Definition; Shipping trägt ihn bereits zusammen mit
den unveränderlichen Spezies. Keine zweite Verzeichnisabfrage, keine neue Registry.
Unreal/RAGE-Benchmark: validiertes Asset als geschlossene Einheit veröffentlichen;
keine halb aktualisierten Generatorparameter unter einem neuen Artefaktschlüssel.
Vorhandene Parse-Logik als privaten Reader wiederverwenden.

Beweis: erfolgreicher Profilwechsel übernimmt Defaults statt vorheriger Werte;
Fehlschlag erhält Quelltext und Parameter; Quelltext bleibt nach Änderung des
Aufruferbuffers erhalten. Wald-Katalog- und Kronencache-Fixtures verwenden dieselbe
Definition wie ihr Parser. Negativkontrolle initialisiert den Parse-Kandidaten aus
dem bestehenden Objekt statt Defaults; der Profilwechsel muss rot werden.

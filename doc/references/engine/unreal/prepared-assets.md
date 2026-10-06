# Vorbereitete Assets, räumliche Pakete und Residency

Stand: 2026-10-06. Primärquellen unten sind Online-Dokumentation von Epic und ein
SIGGRAPH-Paper. Diese Notiz fasst Verfahren zusammen; sie ist keine Kopie der Dokumentation.
Den Outshine-Vertrag besitzt [WI 2280](../../../../board/2280_ready_assets_load_from_a_spatial_cache.md).

## Referenzen und Grenzen

| Quelle | Belegtes Verfahren | Grenze der Übertragung |
|---|---|---|
| [Unreal Derived Data Cache](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-derived-data-cache-in-unreal-engine) | Hierarchischer Lookup; Miss erzeugt abgeleitete Daten und speichert sie asynchron. | UE nutzt DDC beim Asset-Build. Cooked Games benötigen keinen DDC. Outshines Runtime-Misses sind unsere Übertragung. |
| [World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine) | Räumliche Zellen, Ladebedarf durch Streaming Sources und deren Reichweite. | Belegt keine konkrete Datenbank, Radiusabfrage oder Ladezeit für Outshine. |
| [World Partition HLOD](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine) | Vorbereitete vereinfachte Verbandsmeshes und Materialien stellen entfernte Zellen dar. | Proxies müssen unsere dynamische Beleuchtung und Bildstabilität erhalten; keine universelle Geometrieform. |
| [Texture Streaming Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/texture-streaming-overview-for-unreal-engine) | Sichtbedarf, Speicherbudget und Retention-Prioritäten einschließlich letzter Nutzung bestimmen residente Mips. | Texturverfahren; feste TTL-Sekunden je Geometrie-LOD sind damit nicht belegt. |
| [Geometry Clipmaps, Losasso/Hoppe, SIGGRAPH 2004](https://hhoppe.com/geomclipmap.pdf), [lokales PDF](../../terrain/siggraph/2004-geometry-clipmaps.pdf) | Komprimierte Terrainbasis, verschachtelte kamerazentrierte Gitter, inkrementelle Aktualisierung und synthetisierte feinere Details. | Heightfields; kein direkter Ersatz für Gebäude-, Vegetations- oder beliebige Objekt-LOD. |

## Entscheidung für Outshine

Assetbedarf → räumlicher Index → Hit: Rohling laden; Miss: asynchron erzeugen,
vollständig anreichern und atomar veröffentlichen. Gemeinsame Misses teilen einen Job.
Räumliche Pakete bündeln Produkte; keine Datei und kein Job je Haus oder Baum.

Rohlinge enthalten vollständige ergänzte Pläne, Seeds, Abhängigkeiten und native
Basis-/Fernprodukte. Nahgeometrie entsteht budgetiert daraus. Licht, Wetter, Wind und
dynamische Pose bleiben Laufzeitaufgaben. Treffer wiederholen keinen Quellen-Decode,
keine Anreicherung und keinen Rohling-Aufbau.

| Ebene | Gültigkeit und Freigabe |
|---|---|
| Netzwerk-Quellcache | Unveränderte Eingaben bis ausdrücklicher Leerung; keine automatische Aktualisierung. |
| SSD-Assetcache | Input-/Generator-/Formatversion bestimmt Gültigkeit. Speicherbudget darf ungenutzte, regenerierbare Produkte entfernen. Detailgrad allein macht sie nicht zeitlich ungültig. |
| RAM/GPU | Bedarf, Wiederbeschaffungskosten, letzte Nutzung und Budget bestimmen Residency. Benötigte Produkte bleiben resident; GPU-Freigabe erst nach letzter Nutzung. |
| Runtime-Nahdetails | Aus Rohlingen erzeugen, bei unverändertem Bedarf wiederverwenden; ungenutzte Details vor grober Rundumabdeckung freigeben. |

TTL ist höchstens eine gemessene Freigabeverzögerung gegen Thrashing, kein Ablaufdatum
gültiger Assets. Konkrete Zeiten bleiben offen. Fehlende Feinprodukte behalten gültige
grobe Eltern; Frustumwechsel löst keinen vollständigen Neuaufbau aus.

Die Quellen stützen diese Bausteine. Ladezeit, Speicherverbrauch und Bildqualität der
Kombination müssen Wien, Central Park und Tokyo in Outshine belegen.

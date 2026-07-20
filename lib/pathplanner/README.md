# Path Planner – Anforderungen an die Kartenberechnung

Dieses Dokument beschreibt die funktionalen Anforderungen an die Karten- und Wegpunktberechnung (`lib/pathplanner/`). Es dient als Referenz für Änderungen und als Grundlage für die Tests in `test_pathplanner/`.

## 1. Grundprinzip

Die Kartenberechnung füllt die mähbare Fläche (Perimeter minus Exclusions) effektiv mit Weglinien. Dabei gelten zwei harte Regeln:

- **Keine Exclusion wird überfahren.** Jeder Wegpunkt und jedes Verbindungssegment muss außerhalb aller Exclusions liegen.
- **Der Perimeter wird nicht verlassen.** Jeder Wegpunkt muss innerhalb des Perimeters liegen.

## 2. Eingabedaten

- `Map.perimeter` – äußere Begrenzung der Mähfläche (geschlossenes Polygon).
- `Map.exclusions` – Liste von auszusparenden Flächen (geschlossene Polygone).
- `Map.waypoints`, `Map.dockpoints` – optionale Referenzpunkte.
- `Settings.width` – Spurbreite in Metern.
- `Settings.distanceToBorder` – Sicherheitsabstand zum Perimeter (in Metern, wird mit `width` multipliziert).
- `Settings.borderLaps` – Anzahl der Randrunden (Border Laps).
- `Settings.mowArea` – ob die Innenfläche gemäht werden soll.
- `Settings.mowExclusionBorder` – ob um Exclusions herum Border Laps gefahren werden sollen.
- `Settings.mowBorderCcw` – Richtung der Border Laps (CCW vs. CW).
- `Settings.pattern` – Muster der Innenfläche: `0` = Linien, `1` = Kreuzhatch, `2` = Ringe.
- `Settings.angle` – Drehwinkel der Linien in Grad.

## 3. Funktionale Anforderungen

### 3.1 Geometrische Vorverarbeitung

- Exclusions, die den Perimeter berühren, werden leicht verkleinert (`shrinkPolygons`, 1 mm), damit sie als echte Löcher erhalten bleiben und nicht mit der Außenkontur verschmelzen.
- Für Border Laps werden Exclusions um einen kleinen Betrag (`width * 0.05`) aufgeblasen, damit die Randlinie sicher außerhalb der tatsächlichen Exclusion liegt.
- `distanceToBorder` wird auf das Perimeter und auf die Exclusions angewendet: die Mähfläche wird eingezogen, die Exclusions werden entsprechend vergrößert.

### 3.2 Musterberechnung

- **Linien (pattern 0/1):** Zickzack-Linien werden über die Mähfläche gelegt, mit `areaToMow` geschnitten (`clipIntersectOpen`) und anschließend mit den leicht aufgeblasenen Exclusions differenziert (`clipDifferenceOpen`).
- **Ringe (pattern 2):** Die Fläche wird in konzentrische Ringe zerlegt. Ringe, die eine Exclusion berühren, werden gegen die aufgeblasene Exclusion geschnitten (`width * 1.5` Puffer). Zusätzlich werden Ringe um die Exclusions selbst erzeugt, wenn `mowExclusionBorder` aktiv ist.

### 3.3 Verbindungen (Connectoren)

- Aufeinanderfolgende Segmente werden mit `connectPolysUsingPathFinding` verbunden.
- Ein direkter Connector wird nur verwendet, wenn er vollständig innerhalb der Mähfläche und außerhalb aller Exclusions liegt (`clipIntersectOpen` + `clipDifferenceOpen` gegen aufgeblasene Löcher).
- Wenn der direkte Weg blockiert ist, wird ein Umweg entlang der Boundary gesucht (`walkBoundaryWithHoles`). Der Umweg wird abschließend nochmals gegen die aufgeblasenen Exclusions geschnitten, damit kein Rest durch eine Exclusion führt.

### 3.4 Randrunden (Border Laps)

- `addBorderLaps` erzeugt die gewünschte Anzahl von Randrunden entlang des Perimeters.
- Wenn `mowExclusionBorder` aktiv ist, werden zusätzliche Runden um die Exclusions gelegt (`exclusionsBorderLaps`).
- Die Reihenfolge der Border Laps (vor oder nach der Innenfläche) wird durch `mowBorderCcw` gesteuert.

### 3.5 Startpunkt

- Wenn ein `State` mit Dock- oder GPS-Position übergeben wird, wird der Startpunkt (`startNear`) auf die nächstgelegene Kante der mähbaren Fläche projiziert, damit die Route nicht am ursprünglichen Perimeter beginnt.

## 4. Nicht-funktionale Anforderungen

- **Determinismus:** Gleiche Eingaben erzeugen gleiche Wegpunkte.
- **Robustheit:** Auch bei komplexen Perimetern (viele Punkte, Spitzen, enge Passagen) dürfen keine Exclusions überfahren werden.
- **Performance:** Die Berechnung läuft auf einem ESP32-S3; sie muss daher mit begrenztem Speicher und Rechenleistung auskommen.

## 5. Tests

Die Tests liegen in `test_pathplanner/` und werden mit dem dortigen `Makefile` gebaut:

```bash
cd test_pathplanner
make
./test_exclusion
```

### 5.1 Testziele

- **Exclusion-Überfahrungs-Test:** Für jede erzeugte Route wird geprüft, ob ein Segment eine Exclusion schneidet (`segmentInsideExclusion`). Das ist die wichtigste Bedingung.
- **Abstand-zum-Border-Test:** Für Szenarien mit `distanceToBorder` wird der minimale Abstand aller Wegpunkte zum Perimeter geprüft.
- **SVG-Visualisierung:** Jeder Test schreibt eine SVG-Datei nach `test_pathplanner/out/`, in der Perimeter (schwarz), Exclusions (rot) und Route (blau) dargestellt sind. Damit lassen sich Fehler visuell schnell finden.

### 5.2 Testabdeckung

Die Tests decken folgende Szenarien ab:

- Zentrale Exclusion (0° und 45° Winkel)
- Exclusion am Rand
- Mehrere Exclusions (zwei Löcher)
- Border Laps mit und ohne Exclusion-Border
- `distanceToBorder` (verschiedene Werte)
- Ringe (zentrale, randnahe und zwei Exclusions)
- Spur mit `distanceToBorder`
- Reale CassandRA-Karte (komplexes Perimeter mit dreieckiger Exclusion)

### 5.3 Auswertung

- **PASS:** Alle Segmente liegen außerhalb der Exclusions und der minimale Abstand zum Perimeter wird eingehalten.
- **FAIL:** Mindestens ein Segment schneidet eine Exclusion oder der Abstand ist zu klein. Die SVG-Dateien helfen, den Fehler zu lokalisieren.

## 6. Bekannte Einschränkungen / TODO

- `walkBoundaryWithHoles` kann in bestimmten Konstellationen (z. B. wenn Start und Ziel auf verschiedenen Boundaries projiziert werden) auf eine gerade Linie zurückfallen. Das wird in `connectPolysUsingPathFinding` abgefangen, indem das Ergebnis nochmals gegen die aufgeblasenen Exclusions geschnitten wird. Dadurch kann der Connector in mehrere Teile zerlegt werden, was zu Fahrwegen mit Lücken führt. Eine zukünftige Verbesserung wäre ein echter Pathfinding-Algorithmus, der die Exclusions als Hindernisse betrachtet.
- Der Parameter `holes` in `walkBoundaryWithHoles` wird aktuell nicht verwendet (Warnung beim Kompilieren). Er kann in Zukunft entfernt oder sinnvoll genutzt werden.

#include "map_manager.h"
#include "log.h"
#include <MD5Builder.h>

#define _LOG_ "MapManager::"

namespace ArduMower {
    namespace Modem {

        void MapMeta::marshal(JsonObject obj) const {
            obj["id"] = id;
            obj["name"] = name;
            obj["area"] = area;
            obj["hash"] = hash;
            obj["rotation"] = rotation;
            obj["timestamp"] = timestamp;
            obj["file"] = file;
        }

        void MapMeta::unmarshal(JsonObject obj) {
            id = obj["id"] | "";
            name = obj["name"] | "";
            area = obj["area"] | 0.0;
            hash = obj["hash"] | "";
            rotation = obj["rotation"] | 0.0;
            timestamp = obj["timestamp"] | 0u;
            file = obj["file"] | "";
        }

        bool MapManager::begin() {
            if (_initialized) return true;
            if (!SPIFFS.begin(true)) {
                Log(ERR, "%s begin: SPIFFS konnte nicht initialisiert werden", _LOG_);
                return false;
            }
            loadIndex();
            _initialized = true;
            return true;
        }

        bool MapManager::loadIndex() {
            _index.activeId = "";
            _index.nextFileId = 0;
            _index.maps.clear();

            File file = SPIFFS.open(INDEX_FILE);
            if (!file || file.isDirectory()) {
                Log(INFO, "%s loadIndex: kein Index vorhanden, starte leer", _LOG_);
                return true;
            }

            JsonDocument doc;
            auto err = deserializeJson(doc, file);
            file.close();
            if (err) {
                Log(ERR, "%s loadIndex: Deserialisierung fehlgeschlagen: %s", _LOG_, err.c_str());
                return false;
            }

            JsonObject root = doc.as<JsonObject>();
            _index.activeId = root["activeId"] | "";
            _index.nextFileId = root["nextFileId"] | 0u;
            JsonArray maps = root["maps"];
            if (maps) {
                for (JsonObject m : maps) {
                    MapMeta meta;
                    meta.unmarshal(m);
                    if (meta.id.length() > 0 && meta.file.length() > 0) {
                        _index.maps.push_back(meta);
                    }
                }
            }
            Log(INFO, "%s loadIndex: %d Karten geladen, aktiv=%s", _LOG_, _index.maps.size(), _index.activeId.c_str());
            return true;
        }

        bool MapManager::saveIndex() {
            File file = SPIFFS.open(INDEX_FILE, FILE_WRITE);
            if (!file) {
                Log(ERR, "%s saveIndex: Index-Datei konnte nicht geöffnet werden", _LOG_);
                return false;
            }

            JsonDocument doc;
            JsonObject root = doc.to<JsonObject>();
            root["activeId"] = _index.activeId;
            root["nextFileId"] = _index.nextFileId;
            JsonArray maps = root["maps"].to<JsonArray>();
            for (const auto &meta : _index.maps) {
                meta.marshal(maps.add<JsonObject>());
            }
            serializeJson(doc, file);
            file.close();
            return true;
        }

        String MapManager::allocateFileName() {
            String name = String(MAPS_DIR) + "/map_" + String(_index.nextFileId++) + ".json";
            return name;
        }

        MapMeta* MapManager::findMeta(const String &id) {
            for (auto &m : _index.maps) {
                if (m.id == id) return &m;
            }
            return nullptr;
        }

        const MapMeta* MapManager::findMeta(const String &id) const {
            for (const auto &m : _index.maps) {
                if (m.id == id) return &m;
            }
            return nullptr;
        }

        String MapManager::generateDefaultName() const {
            int n = 1;
            while (true) {
                String candidate = "Karte " + String(n);
                bool exists = false;
                for (const auto &m : _index.maps) {
                    if (m.name == candidate) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) return candidate;
                n++;
            }
        }

        double MapManager::polygonArea(const std::vector<ArduMower::Domain::Robot::MapPoint> &poly) {
            if (poly.size() < 3) return 0.0;
            double a = 0.0;
            for (size_t i = 0; i < poly.size(); i++) {
                size_t j = (i + 1) % poly.size();
                a += poly[i].X * poly[j].Y;
                a -= poly[j].X * poly[i].Y;
            }
            return a / 2.0;
        }

        double MapManager::computeArea(const ArduMower::Domain::Robot::MowerMap &map) {
            double area = std::abs(polygonArea(map.perimeter));
            for (const auto &ex : map.exclusions) {
                area -= std::abs(polygonArea(ex));
            }
            return std::max(0.0, area);
        }

        bool MapManager::isMapValid(const ArduMower::Domain::Robot::MowerMap &map) {
            // Dockpoints sind optional, falls kein Dock vorhanden ist.
            // Ein gültiger Perimeter mit mindestens 3 Punkten ist aber nötig.
            if (map.perimeter.size() < 3) {
                Log(WARN, "%s isMapValid: Perimeter hat weniger als 3 Punkte", _LOG_);
                return false;
            }
            return true;
        }

        String MapManager::computeHash(const ArduMower::Domain::Robot::MowerMap &map) {
            JsonDocument doc;
            JsonObject root = doc.to<JsonObject>();
            map.marshalGeometry(root);
            String json;
            serializeJson(doc, json);

            MD5Builder md5;
            md5.begin();
            md5.add(json);
            md5.calculate();
            return md5.toString();
        }

        String MapManager::save(const ArduMower::Domain::Robot::MowerMap &map, const String &name, double rotation) {
            if (!_initialized && !begin()) return "";

            if (!isMapValid(map)) {
                Log(WARN, "%s save: Karte ist ungültig, speichern abgelehnt", _LOG_);
                return "";
            }

            String hash = computeHash(map);
            if (hash.length() == 0) {
                Log(ERR, "%s save: Hash-Berechnung fehlgeschlagen", _LOG_);
                return "";
            }

            double area = computeArea(map);
            String displayName = name;
            if (displayName.length() == 0) displayName = generateDefaultName();

            MapMeta *meta = nullptr;
            String fileName;

            // Wenn eine Karte aktiv ist, wird diese überschrieben (stabiler Slot).
            if (_index.activeId.length() > 0) {
                meta = findMeta(_index.activeId);
            }

            if (meta) {
                fileName = meta->file;
                meta->hash = hash;
                meta->area = area;
                meta->rotation = rotation;
                meta->timestamp = millis();
                if (name.length() > 0) meta->name = displayName;
                Log(INFO, "%s save: Karte '%s' (%s) aktualisiert", _LOG_, meta->name.c_str(), meta->id.c_str());
            } else {
                // Keine aktive Karte: prüfen, ob gleiche Geometrie bereits existiert
                for (auto &m : _index.maps) {
                    if (m.hash == hash) {
                        meta = &m;
                        break;
                    }
                }
                if (meta) {
                    fileName = meta->file;
                    meta->name = displayName;
                    meta->area = area;
                    meta->rotation = rotation;
                    meta->timestamp = millis();
                    Log(INFO, "%s save: existierende Karte '%s' aktualisiert", _LOG_, displayName.c_str());
                } else {
                    fileName = allocateFileName();
                    MapMeta newMeta;
                    newMeta.id = hash;
                    newMeta.name = displayName;
                    newMeta.area = area;
                    newMeta.hash = hash;
                    newMeta.rotation = rotation;
                    newMeta.timestamp = millis();
                    newMeta.file = fileName;
                    _index.maps.push_back(newMeta);
                    meta = &_index.maps.back();
                    Log(INFO, "%s save: neue Karte '%s' (%s, %.1f m²)", _LOG_, displayName.c_str(), hash.c_str(), area);
                }
            }

            // JSON-Dokumente vorab bauen, um Speicherbedarf prüfen zu können
            JsonDocument mapDoc;
            JsonObject mapRoot = mapDoc.to<JsonObject>();
            JsonObject metaObj = mapRoot["meta"].to<JsonObject>();
            if (meta) meta->marshal(metaObj);
            JsonObject mapObj = mapRoot["map"].to<JsonObject>();
            map.marshalGeometry(mapObj);

            JsonDocument indexDoc;
            JsonObject indexRoot = indexDoc.to<JsonObject>();
            indexRoot["activeId"] = hash;
            indexRoot["nextFileId"] = _index.nextFileId;
            JsonArray mapsArr = indexRoot["maps"].to<JsonArray>();
            for (const auto &m : _index.maps) {
                m.marshal(mapsArr.add<JsonObject>());
            }

            size_t required = measureJson(mapDoc) + measureJson(indexDoc) + 4096;
            size_t freeBytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
            if (required > freeBytes) {
                Log(ERR, "%s save: Nicht genug SPIFFS-Speicher (benötigt ~%u, frei %u)",
                    _LOG_, (unsigned)required, (unsigned)freeBytes);
                return "";
            }

            // Map-Datei schreiben
            File file = SPIFFS.open(fileName, FILE_WRITE);
            if (!file) {
                Log(ERR, "%s save: Datei %s konnte nicht geöffnet werden", _LOG_, fileName.c_str());
                return "";
            }
            serializeJson(mapDoc, file);
            file.close();

            // Aktiv setzen und Index speichern
            if (meta) _index.activeId = meta->id;
            saveIndex();

            return meta ? meta->id : "";
        }

        bool MapManager::load(const String &id, ArduMower::Domain::Robot::MowerMap &out) {
            if (!_initialized && !begin()) return false;
            const MapMeta *meta = findMeta(id);
            if (!meta) {
                Log(WARN, "%s load: Karte %s nicht gefunden", _LOG_, id.c_str());
                return false;
            }
            File file = SPIFFS.open(meta->file);
            if (!file || file.isDirectory()) {
                Log(ERR, "%s load: Datei %s nicht lesbar", _LOG_, meta->file.c_str());
                return false;
            }
            JsonDocument doc;
            auto err = deserializeJson(doc, file);
            file.close();
            if (err) {
                Log(ERR, "%s load: Deserialisierung fehlgeschlagen: %s", _LOG_, err.c_str());
                return false;
            }
            out.unmarshal(doc.as<JsonObject>());
            out.rotation = meta->rotation;
            out.timestamp = millis();
            return true;
        }

        bool MapManager::loadActive(ArduMower::Domain::Robot::MowerMap &out) {
            if (_index.activeId.length() == 0) return false;
            return load(_index.activeId, out);
        }

        bool MapManager::rename(const String &id, const String &name) {
            if (!_initialized && !begin()) return false;
            MapMeta *meta = findMeta(id);
            if (!meta) return false;
            String newName = name;
            if (newName.length() == 0) newName = generateDefaultName();
            meta->name = newName;
            meta->timestamp = millis();

            // Meta-Bereich in der Map-Datei ebenfalls aktualisieren
            File file = SPIFFS.open(meta->file);
            if (!file || file.isDirectory()) {
                saveIndex();
                return true;
            }
            JsonDocument doc;
            deserializeJson(doc, file);
            file.close();

            JsonObject root = doc.as<JsonObject>();
            JsonObject metaObj = root["meta"].to<JsonObject>();
            meta->marshal(metaObj);

            file = SPIFFS.open(meta->file, FILE_WRITE);
            if (file) {
                serializeJson(doc, file);
                file.close();
            }
            saveIndex();
            return true;
        }

        bool MapManager::remove(const String &id) {
            if (!_initialized && !begin()) return false;
            for (auto it = _index.maps.begin(); it != _index.maps.end(); ++it) {
                if (it->id == id) {
                    if (SPIFFS.exists(it->file)) SPIFFS.remove(it->file);
                    if (_index.activeId == id) _index.activeId = "";
                    _index.maps.erase(it);
                    saveIndex();
                    return true;
                }
            }
            return false;
        }

        String MapManager::findByHash(const String &hash) const {
            for (const auto &m : _index.maps) {
                if (m.hash == hash) return m.id;
            }
            return "";
        }

        bool MapManager::setActive(const String &id) {
            if (!_initialized && !begin()) return false;
            if (id.length() > 0 && !findMeta(id)) return false;
            _index.activeId = id;
            return saveIndex();
        }

    } // namespace Modem
} // namespace ArduMower

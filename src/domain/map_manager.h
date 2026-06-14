#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <vector>
#include "mower_map.h"

namespace ArduMower {
    namespace Modem {

        struct MapMeta {
            String id;            // Stabiler Slot-Identifier (bei alten Karten = Hash, bei neuen eindeutig)
            String name;          // Anzeigename
            double area = 0.0;    // Fläche in m²
            String hash;          // Hash der aktuellen Kartengeometrie
            double rotation = 0.0; // Kartenausrichtung in Grad
            uint32_t timestamp = 0;
            String file;          // SPIFFS-Dateiname, z.B. /maps/map_0.json

            void marshal(JsonObject obj) const;
            void unmarshal(JsonObject obj);
        };

        class MapManager {
        public:
            bool begin();

            // Speichert die übergebene Karte unter dem angegebenen Namen.
            // Ist bereits eine Karte aktiv, wird diese überschrieben (stabiler Slot).
            // Existiert keine aktive Karte, aber eine Karte mit gleichem Hash, wird diese aktualisiert.
            // Rückgabe: ID oder leerer String bei Fehler.
            String save(const ArduMower::Domain::Robot::MowerMap &map, const String &name, double rotation = 0.0);

            bool load(const String &id, ArduMower::Domain::Robot::MowerMap &out);
            bool loadActive(ArduMower::Domain::Robot::MowerMap &out);

            bool rename(const String &id, const String &name);
            bool remove(const String &id);

            const std::vector<MapMeta>& list() const { return _index.maps; }

            String findByHash(const String &hash) const;
            bool setActive(const String &id);
            String activeId() const { return _index.activeId; }

            static String computeHash(const ArduMower::Domain::Robot::MowerMap &map);
            static double computeArea(const ArduMower::Domain::Robot::MowerMap &map);
            static bool isMapValid(const ArduMower::Domain::Robot::MowerMap &map);

        private:
            struct Index {
                String activeId;
                uint32_t nextFileId = 0;
                std::vector<MapMeta> maps;
            };

            Index _index;
            bool _initialized = false;

            static constexpr const char *MAPS_DIR = "/maps";
            static constexpr const char *INDEX_FILE = "/maps/index.json";

            bool loadIndex();
            bool saveIndex();
            String allocateFileName();
            MapMeta* findMeta(const String &id);
            const MapMeta* findMeta(const String &id) const;
            String generateDefaultName() const;
            static double polygonArea(const std::vector<ArduMower::Domain::Robot::MapPoint> &poly);
        };

    } // namespace Modem
} // namespace ArduMower

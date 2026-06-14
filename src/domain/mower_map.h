#pragma once
#include <vector>
#include <string>
#include <map>


#include <ArduinoJson.h>
#include <string>
#include <vector>
#include "log.h"



namespace ArduMower {
    namespace Domain {
        namespace Robot {
            struct MapPoint {
                double X;
                double Y;
                
                void marshal(JsonObject obj) const {
                    obj["X"] = X;
                    obj["Y"] = Y;                }
            };


            struct MowerMap {
                uint32_t timestamp;

                std::vector<MapPoint> perimeter;
                std::vector<std::vector<MapPoint>> exclusions;
                std::vector<MapPoint> waypoints;
                std::vector<MapPoint> dockpoints;

                // Flag, ob aktuell ein Lesevorgang läuft (z.B. für Map-Transfer)
                bool reading = false;

                // Setzt das reading-Flag (z.B. vor Map-Transfer)
                void beginRead() { reading = true; }
                // Hebt das reading-Flag wieder auf
                void endRead() { reading = false; }
                // Prüft, ob aktuell gelesen wird
                bool isReading() const { return reading; }

                void marshal(JsonObject obj) const {
                    JsonArray perim = obj["perimeter"].to<JsonArray>();
                    for (const auto& p : perimeter) p.marshal(perim.add<JsonObject>());
                    JsonArray excls = obj["exclusions"].to<JsonArray>();
                    for (const auto& ex : exclusions) {
                        JsonArray exArr = excls.add<JsonArray>();
                        for (const auto& p : ex) p.marshal(exArr.add<JsonObject>());
                    }
                    JsonArray wps = obj["waypoints"].to<JsonArray>();
                    for (const auto& p : waypoints) p.marshal(wps.add<JsonObject>());
                    JsonArray docks = obj["dockpoints"].to<JsonArray>();
                    for (const auto& p : dockpoints) p.marshal(docks.add<JsonObject>());
                    Log(DBG, "marshal map perimeter %d, exclusions %d, waypoints %d, dockpoints %d", perimeter.size(), exclusions.size(), waypoints.size(), dockpoints.size());
                }
            };

        } // namespace Robot
    } // namespace Domain
} // namespace ArduMower

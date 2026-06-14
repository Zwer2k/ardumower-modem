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
                    obj["Y"] = Y;
                }
            };

            inline MapPoint unmarshalMapPoint(JsonObject obj) {
                MapPoint p{0, 0};
                if (obj["X"].is<JsonVariant>()) p.X = obj["X"];
                else if (obj["x"].is<JsonVariant>()) p.X = obj["x"];
                if (obj["Y"].is<JsonVariant>()) p.Y = obj["Y"];
                else if (obj["y"].is<JsonVariant>()) p.Y = obj["y"];
                return p;
            }


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

                // Kanonische Serialisierung nur der Geometrie (für Hash/Vergleich)
                void marshalGeometry(JsonObject obj) const {
                    JsonArray perim = obj["perimeter"].to<JsonArray>();
                    for (const auto& p : perimeter) p.marshal(perim.add<JsonObject>());
                    JsonArray excls = obj["exclusions"].to<JsonArray>();
                    for (const auto& ex : exclusions) {
                        JsonArray exArr = excls.add<JsonArray>();
                        for (const auto& p : ex) p.marshal(exArr.add<JsonObject>());
                    }
                    JsonArray docks = obj["dockpoints"].to<JsonArray>();
                    for (const auto& p : dockpoints) p.marshal(docks.add<JsonObject>());
                    JsonArray wps = obj["waypoints"].to<JsonArray>();
                    for (const auto& p : waypoints) p.marshal(wps.add<JsonObject>());
                }

                void marshal(JsonObject obj) const {
                    marshalGeometry(obj);
                    Log(DBG, "marshal map perimeter %d, exclusions %d, waypoints %d, dockpoints %d", perimeter.size(), exclusions.size(), waypoints.size(), dockpoints.size());
                }

                void unmarshalGeometry(JsonObject obj) {
                    perimeter.clear();
                    JsonArray perim = obj["perimeter"];
                    if (perim) {
                        for (JsonObject p : perim) {
                            perimeter.push_back(unmarshalMapPoint(p));
                        }
                    }
                    exclusions.clear();
                    JsonArray excls = obj["exclusions"];
                    if (excls) {
                        for (JsonArray ex : excls) {
                            std::vector<MapPoint> e;
                            for (JsonObject p : ex) {
                                e.push_back(unmarshalMapPoint(p));
                            }
                            exclusions.push_back(e);
                        }
                    }
                    dockpoints.clear();
                    JsonArray docks = obj["dockpoints"];
                    if (docks) {
                        for (JsonObject p : docks) {
                            dockpoints.push_back(unmarshalMapPoint(p));
                        }
                    }
                    waypoints.clear();
                    JsonArray wps = obj["waypoints"];
                    if (wps) {
                        for (JsonObject p : wps) {
                            waypoints.push_back(unmarshalMapPoint(p));
                        }
                    }
                }

                void unmarshal(JsonObject obj) {
                    unmarshalGeometry(obj["map"]);
                }
            };

        } // namespace Robot
    } // namespace Domain
} // namespace ArduMower

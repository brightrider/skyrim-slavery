#pragma once

#include <string_view>

struct ActorTableRow {
    char idHexBuf[16] = {};
    std::string_view name = {};
    std::string_view type = {};
    std::string_view weapon = {};
    std::string_view status = {};
    std::string_view location = {};
    std::string_view task = {};
    std::string_view age = {};
    float distance = 0.0f;
};

inline std::string_view ActorTableRowIdHex(const ActorTableRow& row) {
    return row.idHexBuf;
}

void PopulateActorTableRow(RE::Actor* actor, std::string* taskBuf, ActorTableRow& out);

struct MarkerTableRow {
    char idHexBuf[16] = {};
    RE::BSFixedString jcNameStorage;
    std::string_view type = {};
    std::string_view description = {};
    std::string_view location = {};
    float distance = 0.0f;
};

inline std::string_view MarkerTableRowName(const MarkerTableRow& row) {
    return row.jcNameStorage.empty() ? std::string_view{"unknown"} : std::string_view{row.jcNameStorage.c_str()};
}

inline std::string_view MarkerTableRowIdHex(const MarkerTableRow& row) {
    return row.idHexBuf;
}

void PopulateMarkerTableRow(RE::TESObjectREFR* marker, float distance, MarkerTableRow& out);
void FormatMarkerDescription(const MarkerTableRow& row, std::string& buf);

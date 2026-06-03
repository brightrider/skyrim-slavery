#pragma once

struct ActorTableRow {
    char idHexBuf[16] = {};
    std::string_view name = {};
    std::string_view idHex = {};
    std::string_view type = {};
    std::string_view status = {};
    std::string_view location = {};
    std::string_view task = {};
    bool isChild = false;
    float distance = 0.0f;
};

void PopulateActorTableRow(RE::Actor* actor, std::string* taskBuf, ActorTableRow& out);

struct MarkerTableRow {
    char idHexBuf[16] = {};
    RE::BSFixedString jcNameStorage;
    std::string_view jcName = {};
    std::string_view idHex = {};
    std::string_view description = {};
    std::string_view location = {};
    float distance = 0.0f;
};

void PopulateMarkerTableRow(RE::TESObjectREFR* marker, float distance, MarkerTableRow& out);
void FormatMarkerDescription(const MarkerTableRow& row, std::string& buf);

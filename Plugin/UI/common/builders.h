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

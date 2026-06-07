#include "builders.h"

#include <charconv>
#include <cstdio>
#include <cstring>

#include "../../JCAPI.h"
#include "../../utility.h"

void PopulateActorTableRow(RE::Actor* actor, std::string* taskBuf, ActorTableRow& out) {
    out = {};

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    if (const char* displayName = actor->GetDisplayFullName()) {
        out.name = displayName;
    }

    std::snprintf(out.idHexBuf, sizeof(out.idHexBuf), "%08X", actor->GetFormID());

    if (const RE::TESBoundObject* base = actor->GetBaseObject()) {
        if (const char* typeName = base->GetName()) {
            out.type = typeName;
        }
    }

    out.weapon = Utility::GetWeaponName(actor);

    static constexpr char kActorAgeAdult[] = "Adult";
    static constexpr char kActorAgeChild[] = "Child";
    out.age = actor->IsChild() ? std::string_view(kActorAgeChild) : std::string_view(kActorAgeAdult);
    out.status = actor->IsDead() ? "Dead" : "Alive";

    if (const RE::BGSLocation* location = actor->GetCurrentLocation()) {
        if (const char* locName = location->GetName()) {
            out.location = locName;
        }
    } else {
        out.location = "Unknown";
    }

    out.distance = actor->GetPosition().GetDistance(player->GetPosition());

    if (taskBuf != nullptr) {
        taskBuf->clear();
        Utility::CreateTaskDescription(actor, *taskBuf);
        out.task = *taskBuf;
    }
}

void PopulateMarkerTableRow(RE::TESObjectREFR* marker, float distance, MarkerTableRow& out) {
    out = {};

    if (!marker) {
        return;
    }

    out.distance = distance;

    JC::ObjectId markerDb = JC::GetMarkerDb();
    if (markerDb == 0) {
        out.jcNameStorage = "unknown";
        std::snprintf(out.idHexBuf, sizeof(out.idHexBuf), "unknown");
        out.description = "unknown";
        out.location = "unknown";
        return;
    }

    out.jcNameStorage = JC::jFormMapGetStr(JC::Domain, markerDb, marker, "unknown");
    if (out.jcNameStorage.empty()) {
        out.jcNameStorage = "unknown";
    }

    std::snprintf(out.idHexBuf, sizeof(out.idHexBuf), "%08X", marker->GetFormID());

    if (const RE::TESBoundObject* base = marker->GetBaseObject()) {
        out.type = RE::FormTypeToString(base->GetFormType());
    }

    const char* desc = marker->GetDisplayFullName();
    if (!desc || desc[0] == '\0' || std::strstr(desc, "not be visible")) {
        if (const RE::TESBoundObject* base = marker->GetBaseObject()) {
            desc = base->GetName();
            if (!desc || desc[0] == '\0' || std::strstr(desc, "not be visible")) {
                desc = marker->GetFormEditorID();
                if (!desc || desc[0] == '\0') {
                    if (base) {
                        desc = Utility::GetFormEditorID(base->GetFormID());
                    }
                }
            }
        }
    }
    out.description = desc ? desc : "unknown";

    out.location = "unknown";
    if (const RE::BGSLocation* loc = marker->GetCurrentLocation()) {
        if (const char* locName = loc->GetName()) {
            if (locName[0] != '\0') {
                out.location = locName;
            }
        }
    }
}

void FormatMarkerDescription(const MarkerTableRow& row, std::string& buf) {
    char tmp[32];
    buf.clear();

    const std::string_view name = MarkerTableRowName(row);
    const std::string_view idHex = MarkerTableRowIdHex(row);

    buf.append(name.empty() ? "unknown" : name.data());
    buf.append("[");
    buf.append(idHex.empty() ? "unknown" : idHex.data());
    buf.append(", ");
    buf.append(row.type.empty() ? "unknown" : row.type.data());
    buf.append(", ");
    buf.append(row.description.empty() ? "unknown" : row.description.data());
    buf.append(", ");
    buf.append(row.location.empty() ? "unknown" : row.location.data());
    buf.append("(");
    auto [ptr, ec] = std::to_chars(tmp, tmp + sizeof(tmp), row.distance, std::chars_format::fixed, 2);
    if (ec == std::errc{}) {
        buf.append(tmp, ptr);
    } else {
        buf.append("unknown");
    }
    buf.append(")]");
}

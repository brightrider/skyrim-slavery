#include "builders.h"

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
    out.idHex = out.idHexBuf;

    if (const RE::TESBoundObject* base = actor->GetBaseObject()) {
        if (const char* typeName = base->GetName()) {
            out.type = typeName;
        }
    }

    out.isChild = actor->IsChild();
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

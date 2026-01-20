#include <Shlwapi.h>
#include <Windows.h>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/msvc_sink.h"

#pragma comment(lib, "Shlwapi.lib")

// Copyright (c) 2021-2025 powerofthree
// See LICENSES/MIT-powerofthree.txt
std::vector<RE::Actor*> GetActors(RE::StaticFunctionTag*, const RE::TESFaction* a_faction,
                                  std::vector<std::string> a_displayNames = {}, float a_radius = 0.0f,
                                  std::uint32_t a_numResults = 0) {
    std::vector<RE::Actor*> result;

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return result;
    }

    bool isPlayer =
        std::ranges::any_of(a_displayNames, [](const std::string& e) { return _stricmp(e.c_str(), "player") == 0; });
    if (isPlayer) {
        result.push_back(player);
    }

    if (!a_faction) {
        return result;
    }

    if (const auto processLists = RE::ProcessLists::GetSingleton(); processLists) {
        RE::BSTArray<RE::ActorHandle>* highActorHandles = &processLists->highActorHandles;
        RE::BSTArray<RE::ActorHandle>* middleHighActorHandles = &processLists->middleHighActorHandles;
        RE::BSTArray<RE::ActorHandle>* middleLowActorHandles = &processLists->middleLowActorHandles;
        RE::BSTArray<RE::ActorHandle>* lowActorHandles = &processLists->lowActorHandles;

        std::vector<RE::BSTArray<RE::ActorHandle>*> allActors;
        allActors.push_back(highActorHandles);
        allActors.push_back(middleHighActorHandles);
        allActors.push_back(middleLowActorHandles);
        allActors.push_back(lowActorHandles);

        for (auto array : allActors) {
            for (auto& actorHandle : *array) {
                auto actorPtr = actorHandle.get();
                if (actorPtr) {
                    if (!actorPtr->IsInFaction(a_faction)) {
                        continue;
                    }

                    if (a_radius > 0.0f &&
                        actorPtr->GetPosition().GetSquaredDistance(player->GetPosition()) > a_radius * a_radius) {
                        continue;
                    }

                    if (!a_displayNames.empty()) {
                        const char* displayName = actorPtr->GetDisplayFullName();
                        if (!displayName) {
                            continue;
                        }

                        bool result = std::ranges::any_of(a_displayNames, [displayName](const std::string& e) {
                            return _stricmp(e.c_str(), displayName) == 0;
                        });
                        if (!result) {
                            continue;
                        }
                    }

                    result.push_back(actorPtr.get());
                    if (a_numResults > 0 && result.size() >= a_numResults) {
                        return result;
                    }
                }
            }
        }
    }

    return result;
}

std::vector<RE::Actor*> OrderActorsByDisplayNames(RE::StaticFunctionTag*, std::vector<RE::Actor*> a_actors,
                                                  std::vector<std::string> a_displayNames) {
    std::vector<RE::Actor*> result;
    result.reserve(a_displayNames.size());

    const auto player = RE::PlayerCharacter::GetSingleton();

    for (const auto& wantedName : a_displayNames) {
        for (auto* actor : a_actors) {
            if (!actor) {
                continue;
            }

            const char* name = actor->GetDisplayFullName();
            if (actor == player) {
                name = "player";
            }
            if (name && _stricmp(wantedName.c_str(), name) == 0) {
                result.push_back(actor);
                break;
            }
        }
    }

    return result;
}

void LogActors(RE::StaticFunctionTag*, std::vector<RE::TESForm*> a_markers,
               std::vector<RE::BSFixedString> a_markerNames, std::string a_filter = "", float a_radius = 0.0f) {
    if (a_markers.size() != a_markerNames.size()) {
        return;
    }

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) {
        return;
    }

    auto* BRSS_Actors = dh->LookupForm<RE::TESFaction>(0x5900, "SkyrimSlavery.esp");
    auto* BRSS_PackageKeyword1 = dh->LookupForm<RE::BGSKeyword>(0xAA0F, "SkyrimSlavery.esp");
    auto* BRSS_PackageKeyword2 = dh->LookupForm<RE::BGSKeyword>(0xAA10, "SkyrimSlavery.esp");
    if (!BRSS_Actors || !BRSS_PackageKeyword1 || !BRSS_PackageKeyword2) {
        return;
    }

    std::unordered_map<RE::TESForm*, const char*> markerNameByForm;
    markerNameByForm.reserve(a_markers.size());
    for (std::size_t i = 0; i < a_markers.size(); i++) {
        RE::TESForm* marker = a_markers[i];
        if (!marker) {
            continue;
        }

        const char* name = a_markerNames[i].c_str();
        markerNameByForm.emplace(marker, name);
    }

    auto _GetFormEditorID = reinterpret_cast<const char* (*)(std::uint32_t)>(
        GetProcAddress(GetModuleHandleA("po3_Tweaks.dll"), "GetFormEditorID"));

    auto GetName = [_GetFormEditorID, &markerNameByForm](RE::TESObjectREFR* ref) {
        if (!ref || !ref->GetBaseObject()) {
            return "unknown";
        }

        const char* desc = ref->GetDisplayFullName();
        if (!desc || desc[0] == '\0' || strstr(desc, "not be visible")) {
            desc = ref->GetBaseObject()->GetName();
            if (!desc || desc[0] == '\0' || strstr(desc, "not be visible")) {
                if (auto it = markerNameByForm.find(ref); it != markerNameByForm.end()) {
                    desc = it->second;
                } else {
                    desc = nullptr;
                }
                if (!desc || desc[0] == '\0') {
                    desc = ref->GetFormEditorID();
                    if (!desc || desc[0] == '\0') {
                        if (_GetFormEditorID) {
                            desc = _GetFormEditorID(ref->GetBaseObject()->GetFormID());
                        }
                    }
                }
            }
        }
        if (!desc || desc[0] == '\0') {
            desc = "unknown";
        }
        return desc;
    };

    std::string line;
    line.reserve(512);

    char buf[32];
    for (RE::Actor* actor : GetActors(nullptr, BRSS_Actors, {}, a_radius)) {
        if (!actor || !actor->GetBaseObject()) {
            continue;
        }

        line.clear();

        const char* name = actor->GetDisplayFullName();
        if (!name || name[0] == '\0') {
            name = "unknown";
        }
        line.append(name);
        line.append("[");

        auto [ptr1, ec1] = std::to_chars(buf, buf + sizeof(buf), actor->GetFormID(), 16);
        if (ec1 == std::errc{}) {
            line.append(buf, ptr1);
        } else {
            line.append("unknown");
        }
        line.append(", ");

        const char* type = actor->GetBaseObject()->GetName();
        if (!type || type[0] == '\0') {
            type = "unknown";
        }
        line.append(type);
        line.append(", ");

        line.append(actor->IsDead() ? "dead" : "alive");
        line.append(", ");

        const char* locStr = "unknown";
        RE::BGSLocation* loc = actor->GetCurrentLocation();
        if (loc) {
            const char* locName = loc->GetName();
            if (locName && locName[0] != '\0') {
                locStr = locName;
            }
        }
        line.append(locStr);

        line.append("(");
        float distance = actor->GetPosition().GetDistance(player->GetPosition());
        if (distance > 1000000.0f) {
            distance = -1;
        }
        auto [ptr2, ec2] = std::to_chars(buf, buf + sizeof(buf), distance, std::chars_format::fixed, 2);
        if (ec2 == std::errc{}) {
            line.append(buf, ptr2);
        } else {
            line.append("unknown");
        }
        line.append(")");

        line.append("] ");

        int procedureCode = -1;
        RE::ActorValueOwner* valueOwner = actor->AsActorValueOwner();
        if (valueOwner) {
            procedureCode = (int)valueOwner->GetBaseActorValue(RE::ActorValue::kVariable08);
        }
        const char* nameP1 = GetName(actor->GetLinkedRef(BRSS_PackageKeyword1));
        const char* nameP2 = GetName(actor->GetLinkedRef(BRSS_PackageKeyword2));
        switch (procedureCode) {
            case 0:
                line.append("Idle");
                break;

            case 1:
                line.append("Traveling to ");
                line.append(nameP1);
                break;

            case 2:
                line.append("Following ");
                line.append(nameP1);
                break;

            case 3:
                line.append("Using Idle Marker ");
                line.append(nameP1);
                break;

            case 4:
                line.append("Using weapon on ");
                line.append(nameP2);
                break;

            case 6:
                line.append("Patrolling between ");
                line.append(nameP1);
                line.append(" and ");
                line.append(nameP2);
                break;

            case 7:
                line.append("Aiming at ");
                line.append(nameP2);
                break;

            case 8:
                line.append("Sitting at ");
                line.append(nameP1);
                break;

            case 9:
                line.append("Using weapon on ");
                line.append(nameP2);
                break;

            case 10:
                line.append("Using Idle Marker ");
                line.append(nameP1);
                break;

            default:
                line.append("No action assigned");
                break;
        }

        if (!a_filter.empty() && !StrStrIA(line.c_str(), a_filter.c_str())) {
            continue;
        }

        RE::ConsoleLog::GetSingleton()->Print(line.c_str());
    }
}

void LogMarkers(RE::StaticFunctionTag*, std::vector<RE::TESForm*> a_markers,
                std::vector<RE::BSFixedString> a_markerNames, std::string a_filter = "", float a_radius = 0.0f) {
    if (a_markers.size() != a_markerNames.size()) {
        return;
    }

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    auto _GetFormEditorID = reinterpret_cast<const char* (*)(std::uint32_t)>(
        GetProcAddress(GetModuleHandleA("po3_Tweaks.dll"), "GetFormEditorID"));

    std::string line;
    line.reserve(120);

    char buf[32];
    for (std::size_t i = 0; i < a_markers.size(); i++) {
        if (!a_markers[i]) {
            continue;
        }
        RE::TESObjectREFR* marker = a_markers[i]->As<RE::TESObjectREFR>();
        if (!marker || !marker->GetBaseObject()) {
            continue;
        }

        float distanceSquared = marker->GetPosition().GetSquaredDistance(player->GetPosition());
        if (a_radius > 0.0f && distanceSquared > a_radius * a_radius) {
            continue;
        }
        float distance;
        if (distanceSquared > 1000000.0f * 1000000.0f) {
            distance = -1;
        } else {
            distance = sqrt(distanceSquared);
        }

        line.clear();

        line.append(a_markerNames[i].c_str() ? a_markerNames[i].c_str() : "unknown");
        line.append("[");

        auto [ptr1, ec1] = std::to_chars(buf, buf + sizeof(buf), marker->GetFormID(), 16);
        if (ec1 == std::errc{}) {
            line.append(buf, ptr1);
        } else {
            line.append("unknown");
        }
        line.append(", ");

        const char* desc = marker->GetDisplayFullName();
        if (!desc || desc[0] == '\0' || strstr(desc, "not be visible")) {
            desc = marker->GetBaseObject()->GetName();
            if (!desc || desc[0] == '\0' || strstr(desc, "not be visible")) {
                desc = marker->GetFormEditorID();
                if (!desc || desc[0] == '\0') {
                    if (_GetFormEditorID) {
                        desc = _GetFormEditorID(marker->GetBaseObject()->GetFormID());
                    }
                }
            }
        }
        line.append(desc ? desc : "unknown");
        line.append(", ");

        const char* locStr = "unknown";
        RE::BGSLocation* loc = marker->GetCurrentLocation();
        if (loc) {
            const char* locName = loc->GetName();
            if (locName && locName[0] != '\0') {
                locStr = locName;
            }
        }
        line.append(locStr);

        line.append("(");
        auto [ptr2, ec2] = std::to_chars(buf, buf + sizeof(buf), distance, std::chars_format::fixed, 2);
        if (ec2 == std::errc{}) {
            line.append(buf, ptr2);
        } else {
            line.append("unknown");
        }
        line.append(")");

        line.append("]");

        if (!a_filter.empty() && !StrStrIA(line.c_str(), a_filter.c_str())) {
            continue;
        }

        RE::ConsoleLog::GetSingleton()->Print(line.c_str());
    }
}

static bool InitializeLogger() {
    auto logdir = SKSE::log::log_directory();
    if (!logdir) {
        return false;
    }
    auto pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    auto logpath = *logdir / std::format("{}.log", pluginName);
    if (IsDebuggerPresent()) {
        auto logger1 = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logpath.string(), false);
        auto logger2 = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        spdlog::sinks_init_list loggers = {logger1, logger2};
        auto logger = std::make_shared<spdlog::logger>("logger-main", loggers);
        spdlog::set_default_logger(logger);
    } else {
        auto logger = spdlog::basic_logger_mt("logger-main", logpath.string(), false);
        spdlog::set_default_logger(logger);
    }
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
    return true;
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    InitializeLogger();

    SKSE::GetPapyrusInterface()->Register([](RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("GetActors", "BRSSUtil", GetActors);
        vm->RegisterFunction("OrderActorsByDisplayNames", "BRSSUtil", OrderActorsByDisplayNames);
        vm->RegisterFunction("LogActors", "BRSSUtil", LogActors);
        vm->RegisterFunction("LogMarkers", "BRSSUtil", LogMarkers);

        return true;
    });

    return true;
}

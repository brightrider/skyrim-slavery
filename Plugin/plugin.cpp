#include <Shlwapi.h>
#include <Windows.h>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/msvc_sink.h"

#include "JCAPI.h"
#include "UI.h"
#include "utility.h"

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

void LogActors(RE::StaticFunctionTag*, std::string a_filter = "", float a_radius = 0.0f) {
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) {
        return;
    }

    auto* BRSS_Actors = dh->LookupForm<RE::TESFaction>(0x5900, "SkyrimSlavery.esp");
    if (!BRSS_Actors) {
        return;
    }

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

        Utility::CreateTaskDescription(actor, line);

        if (!a_filter.empty() && !StrStrIA(line.c_str(), a_filter.c_str())) {
            continue;
        }

        RE::ConsoleLog::GetSingleton()->Print(line.c_str());
    }
}

void LogMarkers(RE::StaticFunctionTag*, std::string a_filter = "", float a_radius = 0.0f) {
    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    JC::ObjectId markerDb = JC::GetMarkerDb();
    if (markerDb == 0) {
        return;
    }

    std::string line;
    line.reserve(120);

    RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, nullptr, nullptr);
    while (currentForm) {
        RE::TESObjectREFR* marker = currentForm->As<RE::TESObjectREFR>();
        if (!marker || !marker->GetBaseObject()) {
            currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
            continue;
        }

        float distanceSquared = marker->GetPosition().GetSquaredDistance(player->GetPosition());
        if (a_radius > 0.0f && distanceSquared > a_radius * a_radius) {
            currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
            continue;
        }
        float distance;
        if (distanceSquared > 1000000.0f * 1000000.0f) {
            distance = -1;
        } else {
            distance = sqrt(distanceSquared);
        }

        Utility::CreateMarkerDescription(marker, line, distance);

        if (!a_filter.empty() && !StrStrIA(line.c_str(), a_filter.c_str())) {
            currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
            continue;
        }

        RE::ConsoleLog::GetSingleton()->Print(line.c_str());

        currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
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

static void OnSKSEMessage(SKSE::MessagingInterface::Message* msg) {
    if (!msg) {
        return;
    }

    switch (msg->type) {
        case SKSE::MessagingInterface::kPostLoad:
            SKSE::GetMessagingInterface()->RegisterListener(JC_PLUGIN_NAME, [](SKSE::MessagingInterface::Message* msg) {
                if (!msg || msg->type != jc::message_root_interface) {
                    return;
                }

                JC::Load(jc::root_interface::from_void(msg->data));
            });
            break;

        case SKSE::MessagingInterface::kDataLoaded:
            UI::Register();
            break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    InitializeLogger();

    SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);

    SKSE::GetPapyrusInterface()->Register([](RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("GetActors", "BRSSUtil", GetActors);
        vm->RegisterFunction("OrderActorsByDisplayNames", "BRSSUtil", OrderActorsByDisplayNames);
        vm->RegisterFunction("LogActors", "BRSSUtil", LogActors);
        vm->RegisterFunction("LogMarkers", "BRSSUtil", LogMarkers);

        return true;
    });

    return true;
}

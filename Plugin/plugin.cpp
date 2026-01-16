#include <Windows.h>

#include <ranges>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/msvc_sink.h"

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

        return true;
    });

    return true;
}

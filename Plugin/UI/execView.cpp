#include "execView.h"

#include <array>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory_resource>
#include <string>
#include <vector>

#include "../JCAPI.h"
#include "../PAPI.h"
#include "../SKSEMenuFramework.h"
#include "../utility.h"

#include "actorSelector.h"
#include "markerSelector.h"

static constexpr float kGuardGroupDistanceThresholdSq = 1024.0f * 1024.0f;
static constexpr const char* kMarkerGridNamePrefix = "grid_";

enum class AddEventStep : std::uint8_t {
    None,
    Marker,
    Guards,
    Slaves,
};

enum class AddImmediateEventStep : std::uint8_t {
    None,
    Attackers,
    Targets,
};

static AddEventStep g_addEventStep = AddEventStep::None;
static std::string g_addEventMarkerGridName;
static std::vector<RE::Actor*> g_addEventGuards;

static AddImmediateEventStep g_addImmediateEventStep = AddImmediateEventStep::None;
static std::vector<RE::Actor*> g_addImmediateEventAttackers;

using GuardGroupList = std::pmr::vector<std::pmr::vector<RE::Actor*>>;

static void ResetAddEventFlow() {
    g_addEventStep = AddEventStep::None;
    g_addEventMarkerGridName.clear();
    g_addEventGuards.clear();
}

static void ResetAddImmediateEventFlow() {
    g_addImmediateEventStep = AddImmediateEventStep::None;
    g_addImmediateEventAttackers.clear();
}

static bool TryOpenActorSelector(const char* initialFilter = nullptr, bool force = false) {
    if (UI::ActorSelector::Window && UI::ActorSelector::Window->IsOpen) {
        return false;
    }

    UI::ActorSelector::Open(initialFilter, force);
    return true;
}

static bool ExtractMarkerGridName(RE::TESObjectREFR* marker, std::string& outGridName) {
    if (!marker) {
        return false;
    }

    char nameBuffer[256];
    Utility::GetName(marker, nameBuffer, sizeof(nameBuffer));

    std::size_t length = std::strlen(nameBuffer);
    if (length == 0) {
        return false;
    }

    std::size_t digitEnd = length;
    while (digitEnd > 0 && std::isdigit(static_cast<unsigned char>(nameBuffer[digitEnd - 1])) != 0) {
        --digitEnd;
    }

    if (digitEnd == length || digitEnd == 0 || nameBuffer[digitEnd - 1] != '_') {
        return false;
    }

    length = digitEnd - 1;
    nameBuffer[length] = '\0';

    const std::size_t prefixLength = std::strlen(kMarkerGridNamePrefix);
    if (length <= prefixLength || std::strncmp(nameBuffer, kMarkerGridNamePrefix, prefixLength) != 0) {
        return false;
    }

    outGridName.assign(nameBuffer + prefixLength);
    return !outGridName.empty();
}

static void ProcessAddEventFlow(RE::TESDataHandler* dh) {
    if (g_addEventStep == AddEventStep::Marker && UI::MarkerSelector::Window && !UI::MarkerSelector::Window->IsOpen) {
        std::vector<RE::TESObjectREFR*> selectedMarkers;
        if (!UI::MarkerSelector::ConsumeSelectedMarkers(selectedMarkers) || selectedMarkers.empty() ||
            !ExtractMarkerGridName(selectedMarkers[0], g_addEventMarkerGridName)) {
            ResetAddEventFlow();
            return;
        }

        g_addEventStep = AddEventStep::Guards;
        UI::ActorSelector::Open("guard 1024", true);
        return;
    }

    if (g_addEventStep == AddEventStep::Guards && UI::ActorSelector::Window && !UI::ActorSelector::Window->IsOpen) {
        std::vector<RE::Actor*> selectedGuards;
        if (!UI::ActorSelector::ConsumeSelectedActors(selectedGuards) || selectedGuards.empty()) {
            ResetAddEventFlow();
            return;
        }

        g_addEventGuards = std::move(selectedGuards);
        g_addEventStep = AddEventStep::Slaves;
        UI::ActorSelector::Open("slave 1024", true);
        return;
    }

    if (g_addEventStep == AddEventStep::Slaves && UI::ActorSelector::Window && !UI::ActorSelector::Window->IsOpen) {
        std::vector<RE::Actor*> selectedSlaves;
        if (!UI::ActorSelector::ConsumeSelectedActors(selectedSlaves) || selectedSlaves.empty() ||
            selectedSlaves.size() != g_addEventGuards.size()) {
            ResetAddEventFlow();
            return;
        }

        static auto* BRSS_ControllerQuest = dh->LookupForm<RE::TESQuest>(0x0002E123, "SkyrimSlavery.esp");
        if (BRSS_ControllerQuest) {
            Papyrus::Call(
                BRSS_ControllerQuest,
                "BRSSControllerScript",
                "ExecutionSetup",
                [](RE::BSScript::Variable) {},
                std::move(g_addEventGuards),
                std::move(selectedSlaves),
                g_addEventMarkerGridName
            );
        }

        ResetAddEventFlow();
    }
}

static void ProcessAddImmediateEventFlow() {
    if (g_addImmediateEventStep == AddImmediateEventStep::Attackers && UI::ActorSelector::Window &&
        !UI::ActorSelector::Window->IsOpen) {
        std::vector<RE::Actor*> selectedAttackers;
        if (!UI::ActorSelector::ConsumeSelectedActors(selectedAttackers) || selectedAttackers.empty()) {
            ResetAddImmediateEventFlow();
            return;
        }

        g_addImmediateEventAttackers = std::move(selectedAttackers);
        g_addImmediateEventStep = AddImmediateEventStep::Targets;
        TryOpenActorSelector("slave 1024", true);
        return;
    }

    if (g_addImmediateEventStep == AddImmediateEventStep::Targets && UI::ActorSelector::Window &&
        !UI::ActorSelector::Window->IsOpen) {
        std::vector<RE::Actor*> selectedTargets;
        if (!UI::ActorSelector::ConsumeSelectedActors(selectedTargets) || selectedTargets.empty() ||
            g_addImmediateEventAttackers.empty()) {
            ResetAddImmediateEventFlow();
            return;
        }

        const std::vector<RE::Actor*>& attackers = g_addImmediateEventAttackers;
        const std::size_t attackerCount = attackers.size();
        for (std::size_t targetIndex = 0; targetIndex < selectedTargets.size(); ++targetIndex) {
            RE::Actor* attacker = attackers[targetIndex % attackerCount];
            RE::Actor* target = selectedTargets[targetIndex];
            if (!attacker || !target) {
                continue;
            }

            Utility::SetHealth(target, 1.0f);

            Papyrus::Call(
                attacker,
                "Actor",
                "StartCombat",
                [](RE::BSScript::Variable) {},
                target
            );

            Papyrus::Call(
                target,
                "BRSSActorScript",
                "RemoveFromBRSS",
                [](RE::BSScript::Variable) {}
            );
        }

        ResetAddImmediateEventFlow();
    }
}

static void RenderAddEventButton() {
    if (g_addEventStep != AddEventStep::None) {
        ImGuiMCP::BeginDisabled();
    }

    if (ImGuiMCP::Button("Add event")) {
        g_addEventStep = AddEventStep::Marker;
        UI::MarkerSelector::Open();
    }

    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Add immediate event")) {
        if (TryOpenActorSelector("guard 1024", true)) {
            g_addImmediateEventStep = AddImmediateEventStep::Attackers;
        }
    }

    if (g_addEventStep != AddEventStep::None) {
        ImGuiMCP::EndDisabled();
    }

    ImGuiMCP::Spacing();
}

static bool IsExecutionGuardCandidate(RE::Actor* actor, RE::BGSKeyword* packageKeyword2) {
    if (!actor || !packageKeyword2) {
        return false;
    }

    RE::TESObjectREFR* link2 = actor->GetLinkedRef(packageKeyword2);
    return link2 && link2->As<RE::Actor>();
}

static void CollectExecutionGuardCandidates(
    JC::ObjectId actorDb,
    RE::BGSKeyword* packageKeyword2,
    std::pmr::vector<RE::Actor*>& outCandidates) {
    outCandidates.clear();

    RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
    while (currentForm) {
        RE::Actor* currentActor = currentForm->As<RE::Actor>();
        if (currentActor && IsExecutionGuardCandidate(currentActor, packageKeyword2)) {
            outCandidates.push_back(currentActor);
        }
        currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
    }
}

static bool GuardsWithinGroupDistance(RE::Actor* a, RE::Actor* b) {
    return a->GetPosition().GetSquaredDistance(b->GetPosition()) <= kGuardGroupDistanceThresholdSq;
}

static void BuildGuardGroups(
    const std::pmr::vector<RE::Actor*>& candidates,
    std::pmr::memory_resource* pool,
    GuardGroupList& outGroups) {
    outGroups.clear();

    const std::size_t candidateCount = candidates.size();
    if (candidateCount == 0) {
        return;
    }

    // worst case num of elements = candidates.size()
    std::pmr::vector<bool> visited(pool);
    visited.assign(candidateCount, false);

    // worst case num of elements = candidates.size()
    std::pmr::vector<std::size_t> queue(pool);
    queue.reserve(candidateCount);

    // worst case num of elements = candidates.size()
    std::pmr::vector<RE::Actor*> group(pool);
    group.reserve(candidateCount);

    for (std::size_t seedIndex = 0; seedIndex < candidateCount; ++seedIndex) {
        if (visited[seedIndex]) {
            continue;
        }

        group.clear();

        queue.clear();
        queue.push_back(seedIndex);
        visited[seedIndex] = true;

        while (!queue.empty()) {
            const std::size_t currentIndex = queue.back();
            queue.pop_back();

            RE::Actor* currentGuard = candidates[currentIndex];
            group.push_back(currentGuard);

            for (std::size_t otherIndex = 0; otherIndex < candidateCount; ++otherIndex) {
                if (visited[otherIndex]) {
                    continue;
                }

                if (GuardsWithinGroupDistance(currentGuard, candidates[otherIndex])) {
                    visited[otherIndex] = true;
                    queue.push_back(otherIndex);
                }
            }
        }

        outGroups.push_back(std::pmr::vector<RE::Actor*>(pool));
        outGroups.back().assign(group.begin(), group.end());
    }
}

static void FormatGroupHeader(RE::Actor* representative, std::string& buf) {
    char tmp[32];
    buf.clear();

    const char* locationName = "unknown";
    if (representative) {
        if (const RE::BGSLocation* location = representative->GetCurrentLocation()) {
            if (const char* locName = location->GetName()) {
                if (locName[0] != '\0') {
                    locationName = locName;
                }
            }
        }
    }

    float distance = 0.0f;
    if (representative) {
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            distance = representative->GetPosition().GetDistance(player->GetPosition());
        }
    }

    buf.append(locationName);
    buf.append(" (");
    auto [ptr, ec] = std::to_chars(tmp, tmp + sizeof(tmp), distance, std::chars_format::fixed, 2);
    if (ec == std::errc{}) {
        buf.append(tmp, ptr);
    } else {
        buf.append("unknown");
    }
    buf.append(")");
}

static void RenderExecutionGroupTable(const std::pmr::vector<RE::Actor*>& group, RE::BGSKeyword* packageKeyword2) {
    constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                     ImGuiMCP::ImGuiTableFlags_RowBg |
                                                     ImGuiMCP::ImGuiTableFlags_SizingFixedFit;
    if (!ImGuiMCP::BeginTable("ExecutionGroup", 2, tableFlags)) {
        return;
    }

    ImGuiMCP::TableSetupColumn("Guard");
    ImGuiMCP::TableSetupColumn("Slave");
    ImGuiMCP::TableHeadersRow();

    char guardNameBuffer[256];
    char slaveNameBuffer[256];

    for (RE::Actor* guard : group) {
        if (!guard) {
            continue;
        }

        RE::TESObjectREFR* link2 = guard->GetLinkedRef(packageKeyword2);
        RE::Actor* linkedActor = link2 ? link2->As<RE::Actor>() : nullptr;
        if (!linkedActor) {
            continue;
        }

        Utility::GetName(guard, guardNameBuffer, sizeof(guardNameBuffer));
        Utility::GetName(linkedActor, slaveNameBuffer, sizeof(slaveNameBuffer));

        ImGuiMCP::TableNextRow();
        ImGuiMCP::TableSetColumnIndex(0);
        ImGuiMCP::TextUnformatted(guardNameBuffer);
        ImGuiMCP::TableSetColumnIndex(1);
        ImGuiMCP::TextUnformatted(slaveNameBuffer);
    }

    ImGuiMCP::EndTable();
}

void __stdcall UI::ExecView::Render() {
    RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) {
        return;
    }

    RenderAddEventButton();
    ProcessAddEventFlow(dh);
    ProcessAddImmediateEventFlow();

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    JC::ObjectId actorDb = JC::GetActorDb();
    if (actorDb == 0) {
        return;
    }

    static auto* BRSS_PackageKeyword2 = dh->LookupForm<RE::BGSKeyword>(0xAA10, "SkyrimSlavery.esp");
    if (!BRSS_PackageKeyword2) {
        return;
    }

    alignas(std::max_align_t) static std::array<std::byte, 256 * 1024> execViewBuffer;
    std::pmr::monotonic_buffer_resource execViewPool{
        execViewBuffer.data(),
        execViewBuffer.size(),
        std::pmr::null_memory_resource()
    };

    std::pmr::vector<RE::Actor*> candidates(&execViewPool);
    candidates.reserve(512);
    CollectExecutionGuardCandidates(actorDb, BRSS_PackageKeyword2, candidates);

    GuardGroupList groups(&execViewPool);
    groups.reserve(128);
    BuildGuardGroups(candidates, &execViewPool, groups);

    static std::string groupHeader = [] {
        std::string s;
        s.reserve(128);
        return s;
    }();

    for (std::size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
        const auto& group = groups[groupIndex];
        if (group.empty()) {
            continue;
        }

        ImGuiMCP::PushID(static_cast<int>(groupIndex));

        FormatGroupHeader(group[0], groupHeader);
        if (ImGuiMCP::CollapsingHeader(groupHeader.c_str())) {
            ImGuiMCP::Indent();
            RenderExecutionGroupTable(group, BRSS_PackageKeyword2);
            ImGuiMCP::Unindent();
        }

        ImGuiMCP::PopID();
    }
}

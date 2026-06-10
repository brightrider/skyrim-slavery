#include "trackingView.h"

#include <optional>
#include <vector>

#include "../SKSEMenuFramework.h"

#include "actorSelector.h"
#include "markerSelector.h"

#include "../PAPI.h"
#include "../utility.h"

static bool g_pendingTrackActor = false;
static bool g_pendingTrackObject = false;

static std::optional<std::int32_t> FindFirstEmptyTrackSlot(RE::TESQuest* quest, RE::PlayerCharacter* player) {
    for (auto* baseAlias : quest->aliases) {
        if (!baseAlias) {
            continue;
        }

        auto* ref = static_cast<RE::BGSRefAlias*>(baseAlias)->GetReference();
        if (ref == player) {
            continue;
        }

        if (!ref) {
            return static_cast<std::int32_t>(baseAlias->aliasID - 2);
        }
    }

    return std::nullopt;
}

static void TryTrackOnMap(RE::TESQuest* quest, RE::TESObjectREFR* target, RE::PlayerCharacter* player) {
    const auto slot = FindFirstEmptyTrackSlot(quest, player);
    if (!slot || !target) {
        return;
    }

    Papyrus::Call(
        quest,
        "BRSSControllerScript",
        "TrackOnMap",
        [](RE::BSScript::Variable) {},
        *slot,
        static_cast<RE::TESObjectREFR*>(target)
    );
}

void __stdcall UI::TrackingView::Render() {
    RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) {
        return;
    }

    static auto* BRSS_ControllerQuest = dh->LookupForm<RE::TESQuest>(0x0002E123, "SkyrimSlavery.esp");
    if (!BRSS_ControllerQuest) {
        ImGuiMCP::Text("Quest not found");
        return;
    }

    const auto player = RE::PlayerCharacter::GetSingleton();

    const float actionButtonSize = ImGuiMCP::GetFrameHeight();
    const float actionsColumnWidth = actionButtonSize + ImGuiMCP::GetStyle()->CellPadding.x * 2.0f;
    const ImGuiMCP::ImVec2 actionButtonSizeVec{ actionButtonSize, actionButtonSize };

    constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                     ImGuiMCP::ImGuiTableFlags_RowBg |
                                                     ImGuiMCP::ImGuiTableFlags_SizingFixedFit;
    if (ImGuiMCP::BeginTable("TrackingTable", 3, tableFlags)) {
        ImGuiMCP::TableSetupColumn("Slot", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGuiMCP::TableSetupColumn("Reference", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
        ImGuiMCP::TableSetupColumn("", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, actionsColumnWidth);
        ImGuiMCP::TableHeadersRow();

        for (auto* baseAlias : BRSS_ControllerQuest->aliases) {
            if (!baseAlias) {
                continue;
            }

            auto* ref = static_cast<RE::BGSRefAlias*>(baseAlias)->GetReference();

            if (ref == player) {
                continue;
            }

            const auto slot = static_cast<std::int32_t>(baseAlias->aliasID - 2);

            ImGuiMCP::TableNextRow();
            ImGuiMCP::TableSetColumnIndex(0);
            ImGuiMCP::Text("%d", slot);

            ImGuiMCP::TableSetColumnIndex(1);
            if (!ref) {
                ImGuiMCP::TextUnformatted("Empty");
            } else {
                char nameBuffer[256];
                Utility::GetName(ref, nameBuffer, sizeof(nameBuffer));
                ImGuiMCP::TextUnformatted(nameBuffer);
            }

            ImGuiMCP::TableSetColumnIndex(2);
            ImGuiMCP::PushID(slot);
            if (ImGuiMCP::Button("X", actionButtonSizeVec)) {
                Papyrus::Call(
                    BRSS_ControllerQuest,
                    "BRSSControllerScript",
                    "UntrackOnMap",
                    [](RE::BSScript::Variable) {},
                    slot
                );
            }
            ImGuiMCP::PopID();
        }

        ImGuiMCP::EndTable();
    }

    ImGuiMCP::Spacing();
    if (ImGuiMCP::Button("Track actor")) {
        g_pendingTrackActor = true;
        g_pendingTrackObject = false;
        UI::ActorSelector::Open();
    }
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Track object")) {
        g_pendingTrackObject = true;
        g_pendingTrackActor = false;
        UI::MarkerSelector::Open();
    }

    if (g_pendingTrackActor && ActorSelector::Window && !ActorSelector::Window->IsOpen) {
        g_pendingTrackActor = false;

        std::vector<RE::Actor*> selectedActors;
        if (ActorSelector::ConsumeSelectedActors(selectedActors) && !selectedActors.empty()) {
            TryTrackOnMap(BRSS_ControllerQuest, static_cast<RE::TESObjectREFR*>(selectedActors[0]), player);
        }
    }

    if (g_pendingTrackObject && MarkerSelector::Window && !MarkerSelector::Window->IsOpen) {
        g_pendingTrackObject = false;

        std::vector<RE::TESObjectREFR*> selectedMarkers;
        if (MarkerSelector::ConsumeSelectedMarkers(selectedMarkers) && !selectedMarkers.empty()) {
            TryTrackOnMap(BRSS_ControllerQuest, selectedMarkers[0], player);
        }
    }
}

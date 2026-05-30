#include "markerListView.h"

#include <memory_resource>

#include "../SKSEMenuFramework.h"

#include "common/builders.h"

#include "../JCAPI.h"
#include "../utility.h"

struct RevLinks {
    std::array<RE::TESObjectREFR*, 128> refs;
    std::uint16_t count = 0;
};

static const char* ActorTableRowTextOrEmpty(std::string_view text) {
    return text.empty() ? "" : text.data();
}

static void RenderActorTableRow(const ActorTableRow& row) {
    ImGuiMCP::TableNextRow();
    ImGuiMCP::TableSetColumnIndex(0);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.name));
    ImGuiMCP::TableSetColumnIndex(1);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.idHex));
    ImGuiMCP::TableSetColumnIndex(2);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.type));
    ImGuiMCP::TableSetColumnIndex(3);
    ImGuiMCP::TextUnformatted(row.isChild ? "Yes" : "No");
    ImGuiMCP::TableSetColumnIndex(4);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.status));
    ImGuiMCP::TableSetColumnIndex(5);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.location));
    ImGuiMCP::TableSetColumnIndex(6);
    ImGuiMCP::Text("%.2f", row.distance);
    ImGuiMCP::TableSetColumnIndex(7);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.task));
}

static void RenderLinkedActors(const RevLinks* links) {
    if (!links || links->count == 0) {
        ImGuiMCP::TextDisabled("No linked actors");
        return;
    }

    static std::string taskBuffer = [] {
        std::string s;
        s.reserve(128);
        return s;
    }();

    constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                     ImGuiMCP::ImGuiTableFlags_RowBg |
                                                     ImGuiMCP::ImGuiTableFlags_SizingFixedFit;
    if (ImGuiMCP::BeginTable("LinkedActors", 8, tableFlags)) {
        ImGuiMCP::TableSetupColumn("Name");
        ImGuiMCP::TableSetupColumn("ID");
        ImGuiMCP::TableSetupColumn("Type");
        ImGuiMCP::TableSetupColumn("Child");
        ImGuiMCP::TableSetupColumn("Status");
        ImGuiMCP::TableSetupColumn("Location");
        ImGuiMCP::TableSetupColumn("Distance");
        ImGuiMCP::TableSetupColumn("Task", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
        ImGuiMCP::TableHeadersRow();

        for (std::uint16_t i = 0; i < links->count; ++i) {
            RE::TESObjectREFR* actorRef = links->refs[i];
            RE::Actor* actor = actorRef ? actorRef->As<RE::Actor>() : nullptr;
            if (!actor) {
                continue;
            }

            ActorTableRow row = {};
            PopulateActorTableRow(actor, &taskBuffer, row);
            RenderActorTableRow(row);
        }

        ImGuiMCP::EndTable();
    }
}

static void CollapsedHeader(const char* label, const void* uniqueId, const RevLinks* links) {
    ImGuiMCP::PushID(uniqueId);
    if (ImGuiMCP::CollapsingHeader(label)) {
        ImGuiMCP::Indent();
        RenderLinkedActors(links);
        ImGuiMCP::Unindent();
    }
    ImGuiMCP::PopID();
}

void __stdcall UI::RenderMarkerListView() {
    static std::string header = [] {
        std::string s;
        s.reserve(128);
        return s;
    }();

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        return;
    }

    JC::ObjectId actorDb = JC::GetActorDb();
    if (actorDb == 0) {
        ImGuiMCP::Text("No actor database found");
        return;
    }

    JC::ObjectId markerDb = JC::GetMarkerDb();
    if (markerDb == 0) {
        ImGuiMCP::Text("No marker database found");
        return;
    }

    RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) {
        return;
    }

    static auto* BRSS_PackageKeyword1 = dh->LookupForm<RE::BGSKeyword>(0xAA0F, "SkyrimSlavery.esp");
    static auto* BRSS_PackageKeyword2 = dh->LookupForm<RE::BGSKeyword>(0xAA10, "SkyrimSlavery.esp");
    if (!BRSS_PackageKeyword1 || !BRSS_PackageKeyword2) {
        return;
    }

    alignas(std::max_align_t) static std::array<std::byte, 8 * 1024 * 1024> revLinkBuffer;
    std::pmr::monotonic_buffer_resource revLinkPool{
        revLinkBuffer.data(),
        revLinkBuffer.size(),
        std::pmr::null_memory_resource()
    };
    std::pmr::unordered_map<RE::TESObjectREFR*, RevLinks> revLink{ &revLinkPool };
    revLink.reserve(4096);

    auto addRevLink = [&](RE::TESObjectREFR* marker, RE::TESObjectREFR* actor) {
        auto& links = revLink[marker];
        if (links.count < links.refs.size()) {
            links.refs[links.count++] = actor;
        }
    };

    RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
    while (currentForm) {
        RE::Actor* currentActor = currentForm->As<RE::Actor>();
        if (currentActor) {
            RE::TESObjectREFR* link1 = currentActor->GetLinkedRef(BRSS_PackageKeyword1);
            RE::TESObjectREFR* link2 = currentActor->GetLinkedRef(BRSS_PackageKeyword2);

            if (link1) {
                addRevLink(link1, currentActor);
            }

            if (link2 && link2 != link1) {
                addRevLink(link2, currentActor);
            }
        }

        currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
    }

    currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, nullptr, nullptr);
    while (currentForm) {
        RE::TESObjectREFR* currentMarker = currentForm->As<RE::TESObjectREFR>();
        if (!currentMarker) {
            currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
            continue;
        }

        Utility::CreateMarkerDescription(currentMarker, header, currentMarker->GetPosition().GetDistance(player->GetPosition()));

        const RevLinks* links = nullptr;
        if (const auto it = revLink.find(currentMarker); it != revLink.end()) {
            links = &it->second;
        }
        CollapsedHeader(header.c_str(), currentMarker, links);

        currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
    }
}

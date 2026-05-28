#include "actorListView.h"

#include "../SKSEMenuFramework.h"

#include "../JCAPI.h"
#include "../utility.h"

void __stdcall UI::RenderActorListView() {
    static char filterBuffer[256] = {};
    static std::string taskBuffer = [] {
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

    ImGuiMCP::SetNextItemWidth(-1.0f);
    ImGuiMCP::InputTextWithHint("##ActorListFilter", "Filter...", filterBuffer, sizeof(filterBuffer));
    ImGuiMCP::Spacing();

    constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                     ImGuiMCP::ImGuiTableFlags_RowBg |
                                                     ImGuiMCP::ImGuiTableFlags_SizingFixedFit;
    if (ImGuiMCP::BeginTable("ActorListTable", 8, tableFlags)) {
        ImGuiMCP::TableSetupColumn("Name");
        ImGuiMCP::TableSetupColumn("ID");
        ImGuiMCP::TableSetupColumn("Type");
        ImGuiMCP::TableSetupColumn("Child");
        ImGuiMCP::TableSetupColumn("Status");
        ImGuiMCP::TableSetupColumn("Location");
        ImGuiMCP::TableSetupColumn("Distance");
        ImGuiMCP::TableSetupColumn("Task", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
        ImGuiMCP::TableHeadersRow();

        RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
        while (currentForm) {
            RE::Actor* currentActor = currentForm->As<RE::Actor>();
            if (!currentActor) {
                currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
                continue;
            }

            float distance = currentActor->GetPosition().GetDistance(player->GetPosition());

            ImGuiMCP::TableNextRow();
            ImGuiMCP::TableSetColumnIndex(0);
            ImGuiMCP::TextUnformatted(currentActor->GetDisplayFullName());
            ImGuiMCP::TableSetColumnIndex(1);
            ImGuiMCP::Text("%08X", currentActor->GetFormID());
            ImGuiMCP::TableSetColumnIndex(2);
            ImGuiMCP::TextUnformatted(currentActor->GetBaseObject()->GetName());
            ImGuiMCP::TableSetColumnIndex(3);
            ImGuiMCP::TextUnformatted(currentActor->IsChild() ? "Yes" : "No");
            ImGuiMCP::TableSetColumnIndex(4);
            ImGuiMCP::TextUnformatted(currentActor->IsDead() ? "Dead" : "Alive");
            ImGuiMCP::TableSetColumnIndex(5);
            ImGuiMCP::TextUnformatted(currentActor->GetCurrentLocation() ? currentActor->GetCurrentLocation()->GetName() : "Unknown");
            ImGuiMCP::TableSetColumnIndex(6);
            ImGuiMCP::Text("%.2f", distance);
            ImGuiMCP::TableSetColumnIndex(7);
            Utility::CreateTaskDescription(currentActor, taskBuffer);
            ImGuiMCP::TextUnformatted(taskBuffer.c_str());
            taskBuffer.clear();

            currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
        }
        ImGuiMCP::EndTable();
    }
}

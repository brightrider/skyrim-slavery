#include "formSelector.h"

static RE::TESForm* g_selectedForm = nullptr;
static bool g_hasSelectedForm = false;

void UI::FormSelector::Open() {
    g_hasSelectedForm = false;
    g_selectedForm = nullptr;
    if (Window) {
        Window->IsOpen = true;
    }
}

bool UI::FormSelector::ConsumeSelectedForm(RE::TESForm*& outForm) {
    if (!g_hasSelectedForm) {
        outForm = nullptr;
        return false;
    }

    outForm = g_selectedForm;
    g_selectedForm = nullptr;
    g_hasSelectedForm = false;
    return true;
}

void __stdcall UI::FormSelector::Render() {
    auto viewport = ImGuiMCP::GetMainViewport();
    const ImGuiMCP::ImVec2 center{
        viewport->Pos.x + viewport->Size.x * 0.5f,
        viewport->Pos.y + viewport->Size.y * 0.5f
    };
    ImGuiMCP::SetNextWindowPos(center, ImGuiMCP::ImGuiCond_Appearing, ImGuiMCP::ImVec2{0.5f, 0.5f});
    ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{viewport->Size.x * 0.4f, viewport->Size.y * 0.4f}, ImGuiMCP::ImGuiCond_Appearing);
    ImGuiMCP::Begin("Form selector##SkyrimSlavery", nullptr, 0);

    bool closeWindow = false;
    if (ImGuiMCP::Button("OK")) {
        RE::TESForm* selectedForm = nullptr;
        if (RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton()) {
            selectedForm = dh->LookupForm(0x2EC1C, "Skyrim.esm");
        }
        g_selectedForm = selectedForm;
        g_hasSelectedForm = true;
        closeWindow = true;
    }
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Cancel")) {
        g_selectedForm = nullptr;
        g_hasSelectedForm = false;
        closeWindow = true;
    }

    if (closeWindow) {
        Window->IsOpen = false;
    }
    ImGuiMCP::End();
}

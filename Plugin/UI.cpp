#include "UI.h"

#include "SKSEMenuFramework.h"

static void __stdcall RenderWindow() {
    ImGuiMCP::Text("Hello from ImGui");
}

void UI::Register() {
    if (!SKSEMenuFramework::IsInstalled()) {
        return;
    }

    SKSEMenuFramework::SetSection("Skyrim Slavery");
    SKSEMenuFramework::AddSectionItem("Main", RenderWindow);
}

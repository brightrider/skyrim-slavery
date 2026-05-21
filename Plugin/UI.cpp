#include "UI.h"

#include "SKSEMenuFramework.h"

#include "UI/actorListView.h"

void UI::Register() {
    if (!SKSEMenuFramework::IsInstalled()) {
        return;
    }

    SKSEMenuFramework::SetSection("Skyrim Slavery");
    SKSEMenuFramework::AddSectionItem("Actor list", RenderActorListView);
}

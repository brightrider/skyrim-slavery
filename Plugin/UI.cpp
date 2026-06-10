#include "UI.h"

#include "SKSEMenuFramework.h"

#include "UI/actorListView.h"
#include "UI/markerListView.h"
#include "UI/trackingView.h"
#include "UI/actorSelector.h"
#include "UI/markerSelector.h"
#include "UI/formSelector.h"

void UI::Register() {
    if (!SKSEMenuFramework::IsInstalled()) {
        return;
    }

    ActorSelector::Window = SKSEMenuFramework::AddWindow(ActorSelector::Render);
    ActorSelector::Window->IsOpen = false;

    MarkerSelector::Window = SKSEMenuFramework::AddWindow(MarkerSelector::Render);
    MarkerSelector::Window->IsOpen = false;

    FormSelector::Window = SKSEMenuFramework::AddWindow(FormSelector::Render);
    FormSelector::Window->IsOpen = false;

    SKSEMenuFramework::SetSection("Skyrim Slavery");
    SKSEMenuFramework::AddSectionItem("Actor list", ActorListView::Render);
    SKSEMenuFramework::AddSectionItem("Marker list", MarkerListView::Render);
    SKSEMenuFramework::AddSectionItem("Tracking", TrackingView::Render);
}

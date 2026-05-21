#include "actorListView.h"

#include "../SKSEMenuFramework.h"

#include "../JCAPI.h"

void __stdcall UI::RenderActorListView() {
    JC::ObjectId actorDb = JC::GetActorDb();
    if (actorDb == 0) {
        ImGuiMCP::Text("No actor database found");
        return;
    }

    RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
    while (currentForm) {
        RE::Actor* currentActor = currentForm->As<RE::Actor>();
        if (!currentActor) {
            currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
            continue;
        }

        ImGuiMCP::Text(currentActor->GetBaseObject()->GetName());

        currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
    }
}

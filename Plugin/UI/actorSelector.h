#pragma once

#include "../SKSEMenuFramework.h"

#include <vector>

namespace UI {
    namespace ActorSelector {
        inline MENU_WINDOW Window;
        void Open();
        bool ConsumeSelectedActors(std::vector<RE::Actor*>& outActors);
        void __stdcall Render();
    }
}

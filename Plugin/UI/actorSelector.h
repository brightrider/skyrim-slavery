#pragma once

#include "../SKSEMenuFramework.h"

#include <vector>

namespace UI {
    namespace ActorSelector {
        inline MENU_WINDOW Window;
        void Open(const char* initialFilter = nullptr, bool force = false);
        bool ConsumeSelectedActors(std::vector<RE::Actor*>& outActors);
        void __stdcall Render();
    }
}

#pragma once

#include "../SKSEMenuFramework.h"

#include <vector>

namespace UI {
    namespace MarkerSelector {
        inline MENU_WINDOW Window;
        void Open(const char* initialFilter = nullptr, bool force = false);
        bool ConsumeSelectedMarkers(std::vector<RE::TESObjectREFR*>& outMarkers);
        void __stdcall Render();
    }
}

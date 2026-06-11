#pragma once

#include "../SKSEMenuFramework.h"

namespace UI {
    namespace FormSelector {
        inline MENU_WINDOW Window;
        void Open(const char* initialFilter = nullptr, bool force = false);
        bool ConsumeSelectedForm(RE::TESForm*& outForm);
        void __stdcall Render();
    }
}

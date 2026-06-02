#pragma once

#include "../SKSEMenuFramework.h"

namespace UI {
    namespace FormSelector {
        inline MENU_WINDOW Window;
        void Open();
        bool ConsumeSelectedForm(RE::TESForm*& outForm);
        void __stdcall Render();
    }
}

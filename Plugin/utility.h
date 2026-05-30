#pragma once

namespace Utility {
    void GetName(RE::TESObjectREFR* ref, char* buf, size_t bufSize);
    const char* GetFormEditorID(std::uint32_t formId);

    void CreateTaskDescription(RE::Actor* actor, std::string& buf);
    void CreateMarkerDescription(RE::TESObjectREFR* marker, std::string& buf, float distance);
}

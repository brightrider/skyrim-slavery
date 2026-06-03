#pragma once

namespace Utility {
    void GetName(RE::TESObjectREFR* ref, char* buf, size_t bufSize);
    const char* GetFormEditorID(std::uint32_t formId);

    std::vector<RE::TESObjectREFR*> FindAllReferencesOfFormTypes(
        RE::TESObjectREFR* a_origin,
        std::initializer_list<std::uint32_t> a_formTypes,
        float a_radius);

    void CreateTaskDescription(RE::Actor* actor, std::string& buf);
    void CreateMarkerDescription(RE::TESObjectREFR* marker, std::string& buf, float distance);
}

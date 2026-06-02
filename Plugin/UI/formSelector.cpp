#include "formSelector.h"

#include <Shlwapi.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "../utility.h"

struct FormSelectorEntry {
    RE::TESForm* form = nullptr;
    std::uint8_t typeFilterValue = 0;
    std::string label;
};

static RE::TESForm* g_selectedForm = nullptr;
static bool g_hasSelectedForm = false;
static RE::TESForm* g_pendingForm = nullptr;

static std::vector<FormSelectorEntry> g_entries;
static bool g_entriesBuilt = false;
static char g_filterBuffer[256] = {};
static bool g_focusFilterOnOpen = false;

enum FormTypeFilter : std::uint8_t {
    kFilterAll = 0,
    kFilterACTI,
    kFilterSTAT,
    kFilterMSTT,
    kFilterFURN,
    kFilterCONT,
    kFilterIDLM
};

static const char* ResolveEditorId(RE::TESForm* form) {
    const char* editorId = Utility::GetFormEditorID(form->GetFormID());
    return (editorId && editorId[0] != '\0') ? editorId : nullptr;
}

static const char* ResolveDisplayName(RE::TESForm* form) {
    const char* name = form->GetName();
    if (!name || name[0] == '\0' || std::strstr(name, "not be visible")) {
        return nullptr;
    }
    return name;
}

static void AppendLabel(std::string& label, const char* typeTag, const char* editorId, const char* name) {
    label = typeTag;
    label.append(" | ");
    label.append(editorId ? editorId : "?");
    label.append(" | ");
    label.append(name ? name : "?");
}

template <typename T>
static void AppendFormArray(RE::TESDataHandler* dh, const char* typeTag, std::uint8_t typeFilterValue) {
    auto& forms = dh->GetFormArray<T>();
    g_entries.reserve(g_entries.size() + forms.size());
    for (auto* form : forms) {
        if (!form || form->IsDeleted()) {
            continue;
        }

        FormSelectorEntry entry;
        entry.form = form;
        entry.typeFilterValue = typeFilterValue;
        AppendLabel(entry.label, typeTag, ResolveEditorId(form), ResolveDisplayName(form));
        g_entries.push_back(std::move(entry));
    }
}

static void AppendFormArrayByType(RE::TESDataHandler* dh, RE::FormType formType, const char* typeTag, std::uint8_t typeFilterValue) {
    auto& forms = dh->GetFormArray(formType);
    g_entries.reserve(g_entries.size() + forms.size());
    for (auto* form : forms) {
        if (!form || form->IsDeleted()) {
            continue;
        }

        FormSelectorEntry entry;
        entry.form = form;
        entry.typeFilterValue = typeFilterValue;
        AppendLabel(entry.label, typeTag, ResolveEditorId(form), ResolveDisplayName(form));
        g_entries.push_back(std::move(entry));
    }
}

static void EnsureEntriesBuilt() {
    if (g_entriesBuilt) {
        return;
    }

    RE::TESDataHandler* dh = RE::TESDataHandler::GetSingleton();
    if (!dh) {
        return;
    }

    g_entries.clear();
    AppendFormArray<RE::TESObjectACTI>(dh, "ACTI", kFilterACTI);
    AppendFormArray<RE::TESObjectSTAT>(dh, "STAT", kFilterSTAT);
    AppendFormArrayByType(dh, RE::FormType::MovableStatic, "MSTT", kFilterMSTT);
    AppendFormArray<RE::TESFurniture>(dh, "FURN", kFilterFURN);
    AppendFormArray<RE::TESObjectCONT>(dh, "CONT", kFilterCONT);
    AppendFormArray<RE::BGSIdleMarker>(dh, "IDLM", kFilterIDLM);

    std::sort(g_entries.begin(), g_entries.end(), [](const FormSelectorEntry& a, const FormSelectorEntry& b) {
        return a.label < b.label;
    });

    g_entriesBuilt = true;
}

static const char* SkipSpaces(const char* s) {
    while (s && *s == ' ') {
        ++s;
    }
    return s;
}

static std::uint8_t ParseTypePrefix(const char*& filterText) {
    filterText = SkipSpaces(filterText);
    if (!filterText || filterText[0] == '\0') {
        return kFilterAll;
    }

    const char* colon = std::strchr(filterText, ':');
    if (!colon) {
        return kFilterAll;
    }

    const std::size_t typeLen = static_cast<std::size_t>(colon - filterText);
    if (typeLen == 4) {
        if (_strnicmp(filterText, "acti", 4) == 0) {
            filterText = SkipSpaces(colon + 1);
            return kFilterACTI;
        }
        if (_strnicmp(filterText, "stat", 4) == 0) {
            filterText = SkipSpaces(colon + 1);
            return kFilterSTAT;
        }
        if (_strnicmp(filterText, "mstt", 4) == 0) {
            filterText = SkipSpaces(colon + 1);
            return kFilterMSTT;
        }
        if (_strnicmp(filterText, "furn", 4) == 0) {
            filterText = SkipSpaces(colon + 1);
            return kFilterFURN;
        }
        if (_strnicmp(filterText, "cont", 4) == 0) {
            filterText = SkipSpaces(colon + 1);
            return kFilterCONT;
        }
        if (_strnicmp(filterText, "idlm", 4) == 0) {
            filterText = SkipSpaces(colon + 1);
            return kFilterIDLM;
        }
    }

    return kFilterAll;
}

static bool MatchesFilter(const FormSelectorEntry& entry, std::uint8_t typeFilter, const char* textFilter) {
    if (typeFilter != kFilterAll && entry.typeFilterValue != typeFilter) {
        return false;
    }
    textFilter = SkipSpaces(textFilter);
    if (!textFilter || textFilter[0] == '\0') {
        return true;
    }
    return StrStrIA(entry.label.c_str(), textFilter) != nullptr;
}

void UI::FormSelector::Open() {
    g_hasSelectedForm = false;
    g_selectedForm = nullptr;
    g_pendingForm = nullptr;
    g_filterBuffer[0] = '\0';
    g_focusFilterOnOpen = true;
    EnsureEntriesBuilt();
    if (Window) {
        Window->IsOpen = true;
    }
}

bool UI::FormSelector::ConsumeSelectedForm(RE::TESForm*& outForm) {
    if (!g_hasSelectedForm) {
        outForm = nullptr;
        return false;
    }

    outForm = g_selectedForm;
    g_selectedForm = nullptr;
    g_hasSelectedForm = false;
    return true;
}

void __stdcall UI::FormSelector::Render() {
    auto viewport = ImGuiMCP::GetMainViewport();
    const ImGuiMCP::ImVec2 center{
        viewport->Pos.x + viewport->Size.x * 0.5f,
        viewport->Pos.y + viewport->Size.y * 0.5f
    };
    ImGuiMCP::SetNextWindowPos(center, ImGuiMCP::ImGuiCond_Appearing, ImGuiMCP::ImVec2{0.5f, 0.5f});
    ImGuiMCP::SetNextWindowSize(ImGuiMCP::ImVec2{viewport->Size.x * 0.4f, viewport->Size.y * 0.4f}, ImGuiMCP::ImGuiCond_Appearing);
    ImGuiMCP::Begin("Form selector##SkyrimSlavery", nullptr, 0);

    EnsureEntriesBuilt();

    const bool focusFilterShortcut = ImGuiMCP::Shortcut(
        ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_L,
        ImGuiMCP::ImGuiInputFlags_RouteFocused | ImGuiMCP::ImGuiInputFlags_RouteOverActive);
    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    const bool cancelShortcut = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_X, false);
    if (g_focusFilterOnOpen || focusFilterShortcut) {
        ImGuiMCP::SetKeyboardFocusHere();
        g_focusFilterOnOpen = false;
    }
    ImGuiMCP::SetNextItemWidth(-1.0f);
    ImGuiMCP::InputTextWithHint("##FormSelectorFilter", "Filter...", g_filterBuffer, sizeof(g_filterBuffer));
    ImGuiMCP::TextDisabled("Ctrl+L: focus filter | Ctrl+X: cancel | Prefix: acti:/stat:/mstt:/furn:/cont:/idlm:");
    ImGuiMCP::Spacing();

    const char* textFilter = g_filterBuffer;
    const std::uint8_t typeFilter = ParseTypePrefix(textFilter);

    bool closeWindow = false;
    std::size_t visibleCount = 0;
    RE::TESForm* singleVisibleForm = nullptr;
    ImGuiMCP::ImVec2 avail{};
    ImGuiMCP::GetContentRegionAvail(&avail);
    if (ImGuiMCP::BeginChild("##FormSelectorList", ImGuiMCP::ImVec2{0.0f, avail.y}, 0)) {
        for (const FormSelectorEntry& entry : g_entries) {
            if (!MatchesFilter(entry, typeFilter, textFilter)) {
                continue;
            }
            ++visibleCount;
            singleVisibleForm = entry.form;

            const bool selected = entry.form == g_pendingForm;
            if (ImGuiMCP::Selectable(entry.label.c_str(), selected, ImGuiMCP::ImGuiSelectableFlags_AllowDoubleClick)) {
                g_pendingForm = entry.form;
                if (ImGuiMCP::IsMouseDoubleClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
                    g_selectedForm = g_pendingForm;
                    g_hasSelectedForm = true;
                    closeWindow = true;
                }
            }
        }
        ImGuiMCP::EndChild();
    }

    const bool enterPressed = ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false) ||
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_KeypadEnter, false);
    const bool escPressed = ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false);
    RE::TESForm* formToAccept = g_pendingForm;
    if (!formToAccept && visibleCount == 1) {
        formToAccept = singleVisibleForm;
    }
    if (enterPressed && formToAccept) {
        g_selectedForm = formToAccept;
        g_hasSelectedForm = true;
        closeWindow = true;
    } else if (escPressed || cancelShortcut) {
        g_selectedForm = nullptr;
        g_hasSelectedForm = false;
        closeWindow = true;
    }

    if (closeWindow) {
        Window->IsOpen = false;
    }
    ImGuiMCP::End();
}

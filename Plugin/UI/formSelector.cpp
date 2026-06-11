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
    std::string matchText;
};

constexpr std::size_t kFormSelectorMaxFilterWords = 32;

enum FormTypeFilter : std::uint8_t {
    kFilterAll = 0,
    kFilterACTI,
    kFilterSTAT,
    kFilterMSTT,
    kFilterFURN,
    kFilterCONT,
    kFilterIDLM
};

static RE::TESForm* g_selectedForm = nullptr;
static bool g_hasSelectedForm = false;
static RE::TESForm* g_pendingForm = nullptr;

static std::vector<FormSelectorEntry> g_entries;
static bool g_entriesBuilt = false;
static char g_filterBuffer[256] = {};
static char g_lastParsedFilter[256] = {};
static char g_parsedTextFilter[256] = {};
static std::uint8_t g_cachedTypeFilter = kFilterAll;
static std::size_t g_filterWordCount = 0;
static const char* g_filterWords[kFormSelectorMaxFilterWords] = {};
static bool g_focusFilterOnOpen = false;

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

static void AppendEntryStrings(std::string& label, std::string& matchText, const char* typeTag, const char* editorId, const char* name) {
    const char* const id = editorId ? editorId : "?";
    const char* const displayName = name ? name : "?";

    matchText = id;
    matchText.append(" | ");
    matchText.append(displayName);

    label = typeTag;
    label.append(" | ");
    label.append(matchText);
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
        AppendEntryStrings(entry.label, entry.matchText, typeTag, ResolveEditorId(form), ResolveDisplayName(form));
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
        AppendEntryStrings(entry.label, entry.matchText, typeTag, ResolveEditorId(form), ResolveDisplayName(form));
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

static void RebuildCachedFilterWords() {
    g_cachedTypeFilter = kFilterAll;
    g_filterWordCount = 0;

    strncpy_s(g_parsedTextFilter, g_filterBuffer, sizeof(g_parsedTextFilter));
    g_parsedTextFilter[sizeof(g_parsedTextFilter) - 1] = '\0';

    const char* text = g_parsedTextFilter;
    g_cachedTypeFilter = ParseTypePrefix(text);
    text = SkipSpaces(text);
    if (!text || text[0] == '\0') {
        return;
    }

    char* cursor = g_parsedTextFilter + (text - g_parsedTextFilter);
    while (*cursor != '\0') {
        while (*cursor == ' ') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        if (g_filterWordCount >= kFormSelectorMaxFilterWords) {
            g_filterWordCount = 0;
            return;
        }

        g_filterWords[g_filterWordCount++] = cursor;
        while (*cursor != '\0' && *cursor != ' ') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        *cursor = '\0';
        ++cursor;
    }
}

static bool MatchesFilter(const FormSelectorEntry& entry) {
    if (g_cachedTypeFilter != kFilterAll && entry.typeFilterValue != g_cachedTypeFilter) {
        return false;
    }
    if (g_filterWordCount == 0) {
        return true;
    }

    for (std::size_t i = 0; i < g_filterWordCount; ++i) {
        const char* const word = g_filterWords[i];
        if (word == nullptr || word[0] == '\0') {
            continue;
        }
        if (StrStrIA(entry.matchText.c_str(), word) == nullptr) {
            return false;
        }
    }
    return true;
}

void UI::FormSelector::Open(const char* initialFilter, bool force) {
    g_hasSelectedForm = false;
    g_selectedForm = nullptr;
    g_pendingForm = nullptr;
    g_focusFilterOnOpen = true;
    if (initialFilter && initialFilter[0] != '\0' && (force || g_filterBuffer[0] == '\0')) {
        strncpy_s(g_filterBuffer, initialFilter, sizeof(g_filterBuffer) - 1);
        g_filterBuffer[sizeof(g_filterBuffer) - 1] = '\0';
        g_lastParsedFilter[0] = '\0';
    }
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
    ImGuiMCP::SetNextWindowBgAlpha(1.0f);
    ImGuiMCP::Begin("Form selector##SkyrimSlavery", nullptr, 0);

    EnsureEntriesBuilt();

    const bool focusFilterShortcut = ImGuiMCP::Shortcut(
        ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_L,
        ImGuiMCP::ImGuiInputFlags_RouteFocused | ImGuiMCP::ImGuiInputFlags_RouteOverActive);
    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    const bool cancelShortcut = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Q, false);
    if (g_focusFilterOnOpen || focusFilterShortcut) {
        ImGuiMCP::SetKeyboardFocusHere();
        g_focusFilterOnOpen = false;
    }
    ImGuiMCP::SetNextItemWidth(-1.0f);
    ImGuiMCP::InputTextWithHint("##FormSelectorFilter", "Filter...", g_filterBuffer, sizeof(g_filterBuffer));
    ImGuiMCP::TextDisabled("Ctrl+L: focus filter | Ctrl+Q: cancel | Prefix: acti:/stat:/mstt:/furn:/cont:/idlm:");
    ImGuiMCP::Spacing();

    if (std::strcmp(g_filterBuffer, g_lastParsedFilter) != 0) {
        strncpy_s(g_lastParsedFilter, g_filterBuffer, sizeof(g_lastParsedFilter));
        g_lastParsedFilter[sizeof(g_lastParsedFilter) - 1] = '\0';
        RebuildCachedFilterWords();
    }

    bool closeWindow = false;
    std::size_t visibleCount = 0;
    RE::TESForm* singleVisibleForm = nullptr;
    ImGuiMCP::ImVec2 avail{};
    ImGuiMCP::GetContentRegionAvail(&avail);
    const ImGuiMCP::ImGuiStyle* style = ImGuiMCP::GetStyle();
    const float footerHeight = ImGuiMCP::GetFrameHeight() + style->ItemSpacing.y;
    const float listHeight = std::max(0.0f, avail.y - footerHeight);
    if (ImGuiMCP::BeginChild("##FormSelectorList", ImGuiMCP::ImVec2{0.0f, listHeight}, 0)) {
        for (const FormSelectorEntry& entry : g_entries) {
            if (!MatchesFilter(entry)) {
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

    const bool okButtonPressed = ImGuiMCP::Button("OK");
    ImGuiMCP::SameLine();
    const bool cancelButtonPressed = ImGuiMCP::Button("Cancel");

    const bool escPressed = ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false);
    const bool enterPressed =
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false) ||
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_KeypadEnter, false) ||
        okButtonPressed;
    const bool cancelPressed = escPressed || cancelShortcut || cancelButtonPressed;
    RE::TESForm* formToAccept = g_pendingForm;
    if (!formToAccept && visibleCount == 1) {
        formToAccept = singleVisibleForm;
    }
    if (enterPressed && formToAccept) {
        g_selectedForm = formToAccept;
        g_hasSelectedForm = true;
        closeWindow = true;
    } else if (cancelPressed) {
        g_selectedForm = nullptr;
        g_hasSelectedForm = false;
        closeWindow = true;
    }

    if (closeWindow) {
        Window->IsOpen = false;
    }
    ImGuiMCP::End();
}

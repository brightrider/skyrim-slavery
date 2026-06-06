#include "markerSelector.h"

#include <cstring>
#include <unordered_set>
#include <vector>

#include "../SKSEMenuFramework.h"

#include "common/builders.h"
#include "common/filter.h"

#include "../JCAPI.h"
#include "../utility.h"

enum class MarkerSelectorFilterField : std::uint8_t {
    Unknown = 0,
    Name,
    Id,
    Type,
    Desc,
    Location,
    Distance,
};

static constexpr const char* kNameAliases[] = { "name", "na" };
static constexpr const char* kIdAliases[] = { "id" };
static constexpr const char* kTypeAliases[] = { "type", "ty" };
static constexpr const char* kDescAliases[] = { "desc", "de" };
static constexpr const char* kLocationAliases[] = { "location", "lo" };
static constexpr const char* kDistanceAliases[] = { "distance", "di" };

static constexpr FilterFieldSpec kMarkerSelectorFilterFields[] = {
    { static_cast<std::uint8_t>(MarkerSelectorFilterField::Name), FilterFieldKind::Text, kNameAliases, std::size(kNameAliases), false },
    { static_cast<std::uint8_t>(MarkerSelectorFilterField::Id), FilterFieldKind::Text, kIdAliases, std::size(kIdAliases), false },
    { static_cast<std::uint8_t>(MarkerSelectorFilterField::Type), FilterFieldKind::Text, kTypeAliases, std::size(kTypeAliases), false },
    { static_cast<std::uint8_t>(MarkerSelectorFilterField::Desc), FilterFieldKind::Text, kDescAliases, std::size(kDescAliases), false },
    { static_cast<std::uint8_t>(MarkerSelectorFilterField::Location), FilterFieldKind::Text, kLocationAliases, std::size(kLocationAliases), false },
    { static_cast<std::uint8_t>(MarkerSelectorFilterField::Distance), FilterFieldKind::Number, kDistanceAliases, std::size(kDistanceAliases), false },
};

static constexpr std::uint8_t kMarkerSelectorTextShorthandFieldIds[] = {
    static_cast<std::uint8_t>(MarkerSelectorFilterField::Name),
    static_cast<std::uint8_t>(MarkerSelectorFilterField::Id),
    static_cast<std::uint8_t>(MarkerSelectorFilterField::Type),
    static_cast<std::uint8_t>(MarkerSelectorFilterField::Desc),
    static_cast<std::uint8_t>(MarkerSelectorFilterField::Location),
};

static constexpr FilterShorthandConfig kMarkerSelectorFilterShorthand{
    static_cast<std::uint8_t>(MarkerSelectorFilterField::Distance),
    { kMarkerSelectorTextShorthandFieldIds, std::size(kMarkerSelectorTextShorthandFieldIds) },
    {},
};

static constexpr FilterSchema kMarkerSelectorFilterSchema{
    kMarkerSelectorFilterFields,
    std::size(kMarkerSelectorFilterFields),
    kMarkerSelectorFilterShorthand,
};

static std::unordered_set<RE::TESObjectREFR*> g_pendingSelection;
static std::vector<RE::TESObjectREFR*> g_confirmedMarkers;
static bool g_hasConfirmedSelection = false;
static char g_filterBuffer[256] = {};
static char g_lastTokenizedFilter[256] = {};
static FilterTokenizeResult g_tokenizeResult = {};
static FilterParseResult g_parseResult = {};
static bool g_focusFilterOnOpen = false;
static bool g_closeOnDoubleClick = false;
static RE::TESObjectREFR* g_navFocusedMarker = nullptr;

struct MarkerSelectorCachedEntry {
    RE::TESObjectREFR* marker = nullptr;
    MarkerTableRow row{};
};

static std::vector<MarkerSelectorCachedEntry> g_cachedMarkers;
static float g_lastReferenceSearchRadius = -1.0f;

static constexpr float kDefaultReferenceSearchRadius = 1024.0f;
static constexpr const char kNearMarkerName[] = "Near";

static void AddMarkerToCacheIfNew(
    RE::TESObjectREFR* marker, float distance, std::unordered_set<RE::FormID>& seen, bool fromNearbySearch = false) {
    if (!marker) {
        return;
    }

    const RE::FormID formId = marker->GetFormID();
    if (!seen.insert(formId).second) {
        return;
    }

    MarkerSelectorCachedEntry entry;
    entry.marker = marker;
    PopulateMarkerTableRow(marker, distance, entry.row);
    if (fromNearbySearch) {
        entry.row.jcNameStorage = kNearMarkerName;
    }
    g_cachedMarkers.push_back(entry);
}

static void RebuildMarkerSelectorCache(float referenceSearchRadius) {
    g_cachedMarkers.clear();

    std::unordered_set<RE::FormID> seen;
    seen.reserve(256);
    g_cachedMarkers.reserve(256);

    const auto player = RE::PlayerCharacter::GetSingleton();
    const RE::NiPoint3 playerPos = player ? player->GetPosition() : RE::NiPoint3{};

    const JC::ObjectId markerDb = JC::GetMarkerDb();
    if (markerDb != 0) {
        RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, nullptr, nullptr);
        while (currentForm) {
            RE::TESObjectREFR* currentMarker = currentForm->As<RE::TESObjectREFR>();
            if (currentMarker) {
                const float distance = player ? currentMarker->GetPosition().GetDistance(playerPos) : 0.0f;
                AddMarkerToCacheIfNew(currentMarker, distance, seen);
            }
            currentForm = JC::jFormMapNextKey(JC::Domain, markerDb, currentForm, nullptr);
        }
    }

    if (player) {
        const std::vector<RE::TESObjectREFR*> nearbyRefs = Utility::FindAllReferencesOfFormTypes(
            player,
            {
                static_cast<std::uint32_t>(RE::FormType::Activator),
                static_cast<std::uint32_t>(RE::FormType::Static),
                static_cast<std::uint32_t>(RE::FormType::MovableStatic),
                static_cast<std::uint32_t>(RE::FormType::Furniture),
                static_cast<std::uint32_t>(RE::FormType::Container),
                static_cast<std::uint32_t>(RE::FormType::IdleMarker),
            },
            referenceSearchRadius);
        for (RE::TESObjectREFR* ref : nearbyRefs) {
            const float distance = ref ? ref->GetPosition().GetDistance(playerPos) : 0.0f;
            AddMarkerToCacheIfNew(ref, distance, seen, true);
        }
    }
}

static const char* MarkerTableRowTextOrEmpty(std::string_view text) {
    return text.empty() ? "" : text.data();
}

static std::string_view MarkerSelectorFilterRowText(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const MarkerTableRow*>(rowContext);
    switch (static_cast<MarkerSelectorFilterField>(fieldId)) {
    case MarkerSelectorFilterField::Name:
        return MarkerTableRowName(row);
    case MarkerSelectorFilterField::Id:
        return MarkerTableRowIdHex(row);
    case MarkerSelectorFilterField::Type:
        return row.type;
    case MarkerSelectorFilterField::Desc:
        return row.description;
    case MarkerSelectorFilterField::Location:
        return row.location;
    default:
        return {};
    }
}

static bool MarkerSelectorFilterRowBool(const void* rowContext, std::uint8_t fieldId) {
    (void)rowContext;
    (void)fieldId;
    return false;
}

static float MarkerSelectorFilterRowNumber(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const MarkerTableRow*>(rowContext);
    switch (static_cast<MarkerSelectorFilterField>(fieldId)) {
    case MarkerSelectorFilterField::Distance:
        return row.distance;
    default:
        return 0.0f;
    }
}

static const FilterRowAccess kMarkerSelectorFilterRowAccess{
    MarkerSelectorFilterRowText,
    MarkerSelectorFilterRowBool,
    MarkerSelectorFilterRowNumber,
};

static bool CachedEntryMatchesFilter(const MarkerSelectorCachedEntry& entry, const FilterParseResult& parseResult) {
    return FilterMatchesExpression(
        parseResult, &entry.row, kMarkerSelectorFilterSchema, kMarkerSelectorFilterRowAccess);
}

static bool MarkerSelectorFilterIsNonEmpty() {
    return g_filterBuffer[0] != '\0';
}

static void CollectVisibleMarkers(std::vector<RE::TESObjectREFR*>& outMarkers, bool applyFilter) {
    outMarkers.clear();
    outMarkers.reserve(g_cachedMarkers.size());
    for (const MarkerSelectorCachedEntry& entry : g_cachedMarkers) {
        if (!applyFilter || CachedEntryMatchesFilter(entry, g_parseResult)) {
            outMarkers.push_back(entry.marker);
        }
    }
}

static void UpdateSelectionOnRowClick(RE::TESObjectREFR* marker) {
    const bool wasSelected = g_pendingSelection.contains(marker);
    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    const bool ctrlHeld = io && (io->KeyCtrl || io->KeySuper);

    if (ctrlHeld) {
        if (wasSelected) {
            g_pendingSelection.erase(marker);
        } else {
            g_pendingSelection.insert(marker);
        }
        return;
    }

    g_pendingSelection.clear();
    if (!wasSelected) {
        g_pendingSelection.insert(marker);
    }
}

static void RenderMarkerSelectorTableRow(const MarkerTableRow& row, RE::TESObjectREFR* marker) {
    const bool selected = g_pendingSelection.contains(marker);

    ImGuiMCP::TableNextRow();
    ImGuiMCP::PushID(static_cast<int>(marker->GetFormID()));

    ImGuiMCP::TableSetColumnIndex(0);
    ImGuiMCP::TextUnformatted(MarkerTableRowTextOrEmpty(MarkerTableRowName(row)));
    ImGuiMCP::TableSetColumnIndex(1);
    ImGuiMCP::Text("%08X", marker->GetFormID());
    ImGuiMCP::TableSetColumnIndex(2);
    ImGuiMCP::TextUnformatted(MarkerTableRowTextOrEmpty(row.type));
    ImGuiMCP::TableSetColumnIndex(3);
    ImGuiMCP::TextUnformatted(MarkerTableRowTextOrEmpty(row.description));
    ImGuiMCP::TableSetColumnIndex(4);
    ImGuiMCP::TextUnformatted(MarkerTableRowTextOrEmpty(row.location));
    ImGuiMCP::TableSetColumnIndex(5);
    ImGuiMCP::Text("%.2f", row.distance);

    ImGuiMCP::TableSetColumnIndex(0);
    constexpr ImGuiMCP::ImGuiSelectableFlags selectableFlags =
        ImGuiMCP::ImGuiSelectableFlags_SpanAllColumns | ImGuiMCP::ImGuiSelectableFlags_AllowOverlap |
        ImGuiMCP::ImGuiSelectableFlags_AllowDoubleClick |
        static_cast<ImGuiMCP::ImGuiSelectableFlags>(ImGuiMCP::ImGuiSelectableFlags_NoSetKeyOwner);
    if (ImGuiMCP::Selectable("##MarkerSelectorRow", selected, selectableFlags)) {
        if (ImGuiMCP::IsMouseDoubleClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
            g_confirmedMarkers.clear();
            g_confirmedMarkers.push_back(marker);
            g_hasConfirmedSelection = true;
            g_closeOnDoubleClick = true;
        } else {
            const bool enterActivatingRow = ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false) ||
                ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_KeypadEnter, false);
            if (!enterActivatingRow) {
                UpdateSelectionOnRowClick(marker);
            }
        }
    }
    if (ImGuiMCP::IsItemFocused()) {
        g_navFocusedMarker = marker;
    }

    ImGuiMCP::PopID();
}

void UI::MarkerSelector::Open() {
    g_hasConfirmedSelection = false;
    g_confirmedMarkers.clear();
    g_pendingSelection.clear();
    g_focusFilterOnOpen = true;
    if (g_lastReferenceSearchRadius < 0.0f) {
        g_lastReferenceSearchRadius = kDefaultReferenceSearchRadius;
    }
    RebuildMarkerSelectorCache(g_lastReferenceSearchRadius);
    if (Window) {
        Window->IsOpen = true;
    }
}

bool UI::MarkerSelector::ConsumeSelectedMarkers(std::vector<RE::TESObjectREFR*>& outMarkers) {
    if (!g_hasConfirmedSelection) {
        outMarkers.clear();
        return false;
    }

    outMarkers = g_confirmedMarkers;
    g_confirmedMarkers.clear();
    g_hasConfirmedSelection = false;
    return true;
}

void __stdcall UI::MarkerSelector::Render() {
    auto viewport = ImGuiMCP::GetMainViewport();
    const ImGuiMCP::ImVec2 center{
        viewport->Pos.x + viewport->Size.x * 0.5f,
        viewport->Pos.y + viewport->Size.y * 0.5f
    };
    ImGuiMCP::SetNextWindowPos(center, ImGuiMCP::ImGuiCond_Appearing, ImGuiMCP::ImVec2{0.5f, 0.5f});
    ImGuiMCP::SetNextWindowSize(
        ImGuiMCP::ImVec2{viewport->Size.x * 0.5f, viewport->Size.y * 0.5f},
        ImGuiMCP::ImGuiCond_Appearing);
    ImGuiMCP::SetNextWindowBgAlpha(1.0f);
    ImGuiMCP::Begin("Marker selector##SkyrimSlavery", nullptr, 0);

    g_closeOnDoubleClick = false;

    if (g_cachedMarkers.empty()) {
        ImGuiMCP::Text("No markers found");
        ImGuiMCP::End();
        return;
    }

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
    ImGuiMCP::InputTextWithHint("##MarkerSelectorFilter", "Filter...", g_filterBuffer, sizeof(g_filterBuffer));
    ImGuiMCP::TextDisabled("Ctrl+L: focus filter | Ctrl+Enter: select all visible | Ctrl+Q: cancel");
    if (std::strcmp(g_filterBuffer, g_lastTokenizedFilter) != 0) {
        strncpy_s(g_lastTokenizedFilter, g_filterBuffer, sizeof(g_lastTokenizedFilter));
        g_lastTokenizedFilter[sizeof(g_lastTokenizedFilter) - 1] = '\0';
        FilterTokenize(g_filterBuffer, g_tokenizeResult);
        FilterParseExpression(g_filterBuffer, g_tokenizeResult, g_parseResult, kMarkerSelectorFilterSchema);

        const float referenceSearchRadius = FilterGetLessUpperBound(
            g_parseResult,
            static_cast<std::uint8_t>(MarkerSelectorFilterField::Distance),
            kDefaultReferenceSearchRadius);
        if (referenceSearchRadius != g_lastReferenceSearchRadius) {
            g_lastReferenceSearchRadius = referenceSearchRadius;
            RebuildMarkerSelectorCache(referenceSearchRadius);
        }
    }
    if (!g_parseResult.ok && g_parseResult.error[0] != '\0') {
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", g_parseResult.error);
    }
    ImGuiMCP::Spacing();
    ImGuiMCP::TextDisabled("Selected: %zu", g_pendingSelection.size());
    ImGuiMCP::Spacing();

    const bool applyFilter = g_parseResult.ok && g_parseResult.hasExpression;

    ImGuiMCP::ImVec2 tableAvail{};
    ImGuiMCP::GetContentRegionAvail(&tableAvail);

    bool closeWindow = false;
    std::size_t visibleRowCount = 0;
    RE::TESObjectREFR* singleVisibleMarker = nullptr;

    if (ImGuiMCP::BeginChild("##MarkerSelectorTable", ImGuiMCP::ImVec2{0.0f, tableAvail.y}, 0)) {
        constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                         ImGuiMCP::ImGuiTableFlags_RowBg |
                                                         ImGuiMCP::ImGuiTableFlags_SizingFixedFit |
                                                         ImGuiMCP::ImGuiTableFlags_ScrollY;
        if (ImGuiMCP::BeginTable("MarkerSelectorTable", 6, tableFlags)) {
            ImGuiMCP::TableSetupColumn("Name", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 300.0f);
            ImGuiMCP::TableSetupColumn("ID");
            ImGuiMCP::TableSetupColumn("Type");
            ImGuiMCP::TableSetupColumn("Description");
            ImGuiMCP::TableSetupColumn("Location");
            ImGuiMCP::TableSetupColumn("Distance");
            ImGuiMCP::TableSetupScrollFreeze(0, 1);
            ImGuiMCP::TableHeadersRow();

            g_navFocusedMarker = nullptr;
            for (const MarkerSelectorCachedEntry& entry : g_cachedMarkers) {
                if (!applyFilter || CachedEntryMatchesFilter(entry, g_parseResult)) {
                    ++visibleRowCount;
                    singleVisibleMarker = entry.marker;
                    RenderMarkerSelectorTableRow(entry.row, entry.marker);
                }
            }

            ImGuiMCP::EndTable();
        }
        ImGuiMCP::EndChild();
    }

    constexpr ImGuiMCP::ImGuiInputFlags kConfirmInputRoute = ImGuiMCP::ImGuiInputFlags_RouteGlobal;
    const bool confirmAllVisiblePressed =
        ImGuiMCP::Shortcut(ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_Enter, kConfirmInputRoute) ||
        ImGuiMCP::Shortcut(ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_KeypadEnter, kConfirmInputRoute);
    const bool confirmPressed = !confirmAllVisiblePressed && !(io && io->KeyCtrl) &&
        (ImGuiMCP::Shortcut(ImGuiMCP::ImGuiKey_Enter, kConfirmInputRoute) ||
            ImGuiMCP::Shortcut(ImGuiMCP::ImGuiKey_KeypadEnter, kConfirmInputRoute));
    const bool escPressed = ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false);

    if (confirmAllVisiblePressed && MarkerSelectorFilterIsNonEmpty()) {
        CollectVisibleMarkers(g_confirmedMarkers, applyFilter);
        g_hasConfirmedSelection = true;
        closeWindow = true;
    } else if (confirmPressed) {
        if (!g_pendingSelection.empty()) {
            g_confirmedMarkers.assign(g_pendingSelection.begin(), g_pendingSelection.end());
        } else if (applyFilter && visibleRowCount == 1 && singleVisibleMarker) {
            g_confirmedMarkers.clear();
            g_confirmedMarkers.push_back(singleVisibleMarker);
        } else if (g_navFocusedMarker) {
            g_confirmedMarkers.clear();
            g_confirmedMarkers.push_back(g_navFocusedMarker);
        } else {
            g_confirmedMarkers.clear();
        }
        g_hasConfirmedSelection = true;
        closeWindow = true;
    } else if (escPressed || cancelShortcut) {
        g_hasConfirmedSelection = false;
        g_confirmedMarkers.clear();
        closeWindow = true;
    }

    if ((closeWindow || g_closeOnDoubleClick) && Window) {
        Window->IsOpen = false;
        g_cachedMarkers.clear();
    }

    ImGuiMCP::End();
}

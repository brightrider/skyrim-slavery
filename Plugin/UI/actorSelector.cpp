#include "actorSelector.h"

#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <vector>

#include "../SKSEMenuFramework.h"

#include "common/builders.h"
#include "common/filter.h"

#include "../JCAPI.h"
#include "../utility.h"

enum class ActorSelectorFilterField : std::uint8_t {
    Unknown = 0,
    Name,
    Id,
    Type,
    Weapon,
    Age,
    Status,
    Location,
    Distance,
};

static constexpr const char* kNameAliases[] = { "name", "na" };
static constexpr const char* kIdAliases[] = { "id" };
static constexpr const char* kTypeAliases[] = { "type", "ty" };
static constexpr const char* kWeaponAliases[] = { "weapon", "we" };
static constexpr const char* kAgeAliases[] = { "age", "ag" };
static constexpr const char* kStatusAliases[] = { "status", "st" };
static constexpr const char* kLocationAliases[] = { "location", "lo" };
static constexpr const char* kDistanceAliases[] = { "distance", "di" };

static constexpr FilterFieldSpec kActorSelectorFilterFields[] = {
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Name), FilterFieldKind::Text, kNameAliases, std::size(kNameAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Id), FilterFieldKind::Text, kIdAliases, std::size(kIdAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Type), FilterFieldKind::Text, kTypeAliases, std::size(kTypeAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Weapon), FilterFieldKind::Text, kWeaponAliases, std::size(kWeaponAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Age), FilterFieldKind::Text, kAgeAliases, std::size(kAgeAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Status), FilterFieldKind::Text, kStatusAliases, std::size(kStatusAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Location), FilterFieldKind::Text, kLocationAliases, std::size(kLocationAliases), false },
    { static_cast<std::uint8_t>(ActorSelectorFilterField::Distance), FilterFieldKind::Number, kDistanceAliases, std::size(kDistanceAliases), false },
};

static constexpr std::uint8_t kActorSelectorTextShorthandFieldIds[] = {
    static_cast<std::uint8_t>(ActorSelectorFilterField::Name),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Id),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Type),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Weapon),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Age),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Status),
    static_cast<std::uint8_t>(ActorSelectorFilterField::Location),
};

static constexpr FilterShorthandConfig kActorSelectorFilterShorthand{
    static_cast<std::uint8_t>(ActorSelectorFilterField::Distance),
    { kActorSelectorTextShorthandFieldIds, std::size(kActorSelectorTextShorthandFieldIds) },
    {},
};

static constexpr FilterSchema kActorSelectorFilterSchema{
    kActorSelectorFilterFields,
    std::size(kActorSelectorFilterFields),
    kActorSelectorFilterShorthand,
};

static std::vector<RE::Actor*> g_pendingSelection;
static std::vector<RE::Actor*> g_confirmedActors;
static bool g_hasConfirmedSelection = false;
static char g_filterBuffer[256] = {};
static char g_lastTokenizedFilter[256] = {};
static FilterTokenizeResult g_tokenizeResult = {};
static FilterParseResult g_parseResult = {};
static bool g_focusFilterOnOpen = false;
static bool g_closeOnDoubleClick = false;
static RE::Actor* g_navFocusedActor = nullptr;
static RE::Actor* g_selectionAnchor = nullptr;

struct ActorSelectorCachedEntry {
    RE::Actor* actor = nullptr;
    ActorTableRow row{};
};

static std::vector<ActorSelectorCachedEntry> g_cachedActors;
static float g_lastReferenceSearchRadius = -1.0f;

static constexpr float kDefaultReferenceSearchRadius = 1024.0f;
static constexpr const char kNearActorType[] = "Near";

static void AddActorToCacheIfNew(RE::Actor* actor, std::unordered_set<RE::FormID>& seen, bool fromNearbySearch = false) {
    if (!actor) {
        return;
    }

    const RE::FormID formId = actor->GetFormID();
    if (!seen.insert(formId).second) {
        return;
    }

    ActorSelectorCachedEntry entry;
    entry.actor = actor;
    PopulateActorTableRow(actor, nullptr, entry.row);
    if (fromNearbySearch) {
        entry.row.type = kNearActorType;
    }
    g_cachedActors.push_back(entry);
}

static void RebuildActorSelectorCache(float referenceSearchRadius) {
    g_cachedActors.clear();

    std::unordered_set<RE::FormID> seen;
    seen.reserve(256);
    g_cachedActors.reserve(256);

    const JC::ObjectId actorDb = JC::GetActorDb();
    if (actorDb != 0) {
        RE::TESForm* currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, nullptr, nullptr);
        while (currentForm) {
            RE::Actor* currentActor = currentForm->As<RE::Actor>();
            AddActorToCacheIfNew(currentActor, seen);
            currentForm = JC::jFormMapNextKey(JC::Domain, actorDb, currentForm, nullptr);
        }
    }

    const auto player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        const std::vector<RE::TESObjectREFR*> nearbyRefs = Utility::FindAllReferencesOfFormTypes(
            player, { static_cast<std::uint32_t>(RE::FormType::NPC) }, referenceSearchRadius);
        for (RE::TESObjectREFR* ref : nearbyRefs) {
            AddActorToCacheIfNew(ref ? ref->As<RE::Actor>() : nullptr, seen, true);
        }
    }
}

static const char* ActorTableRowTextOrEmpty(std::string_view text) {
    return text.empty() ? "" : text.data();
}

static std::string_view ActorSelectorFilterRowText(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const ActorTableRow*>(rowContext);
    switch (static_cast<ActorSelectorFilterField>(fieldId)) {
    case ActorSelectorFilterField::Name:
        return row.name;
    case ActorSelectorFilterField::Id:
        return ActorTableRowIdHex(row);
    case ActorSelectorFilterField::Type:
        return row.type;
    case ActorSelectorFilterField::Weapon:
        return row.weapon;
    case ActorSelectorFilterField::Age:
        return row.age;
    case ActorSelectorFilterField::Status:
        return row.status;
    case ActorSelectorFilterField::Location:
        return row.location;
    default:
        return {};
    }
}

static bool ActorSelectorFilterRowBool(const void* rowContext, std::uint8_t fieldId) {
    (void)rowContext;
    (void)fieldId;
    return false;
}

static float ActorSelectorFilterRowNumber(const void* rowContext, std::uint8_t fieldId) {
    const auto& row = *static_cast<const ActorTableRow*>(rowContext);
    switch (static_cast<ActorSelectorFilterField>(fieldId)) {
    case ActorSelectorFilterField::Distance:
        return row.distance;
    default:
        return 0.0f;
    }
}

static const FilterRowAccess kActorSelectorFilterRowAccess{
    ActorSelectorFilterRowText,
    ActorSelectorFilterRowBool,
    ActorSelectorFilterRowNumber,
};

static bool CachedEntryMatchesFilter(const ActorSelectorCachedEntry& entry, const FilterParseResult& parseResult) {
    return FilterMatchesExpression(
        parseResult, &entry.row, kActorSelectorFilterSchema, kActorSelectorFilterRowAccess);
}

static bool ActorSelectorFilterIsNonEmpty() {
    return g_filterBuffer[0] != '\0';
}

static void CollectVisibleActors(std::vector<RE::Actor*>& outActors, bool applyFilter) {
    outActors.clear();
    outActors.reserve(g_cachedActors.size());
    for (const ActorSelectorCachedEntry& entry : g_cachedActors) {
        if (!applyFilter || CachedEntryMatchesFilter(entry, g_parseResult)) {
            outActors.push_back(entry.actor);
        }
    }
}

static bool PendingSelectionContains(RE::Actor* actor) {
    return std::find(g_pendingSelection.begin(), g_pendingSelection.end(), actor) != g_pendingSelection.end();
}

static void PendingSelectionRemove(RE::Actor* actor) {
    const auto it = std::find(g_pendingSelection.begin(), g_pendingSelection.end(), actor);
    if (it != g_pendingSelection.end()) {
        g_pendingSelection.erase(it);
    }
}

static std::size_t FindVisibleActorIndex(RE::Actor* actor, bool applyFilter) {
    std::size_t rowIndex = 0;
    for (const ActorSelectorCachedEntry& entry : g_cachedActors) {
        if (!applyFilter || CachedEntryMatchesFilter(entry, g_parseResult)) {
            if (entry.actor == actor) {
                return rowIndex;
            }
            ++rowIndex;
        }
    }
    return static_cast<std::size_t>(-1);
}

static void SelectVisibleActorRange(
    std::size_t fromIndex, std::size_t toIndex, bool addToSelection, bool applyFilter) {
    const std::size_t begin = std::min(fromIndex, toIndex);
    const std::size_t end = std::max(fromIndex, toIndex);
    if (!addToSelection) {
        g_pendingSelection.clear();
    }
    std::size_t rowIndex = 0;
    for (const ActorSelectorCachedEntry& entry : g_cachedActors) {
        if (!applyFilter || CachedEntryMatchesFilter(entry, g_parseResult)) {
            if (rowIndex >= begin && rowIndex <= end) {
                if (!PendingSelectionContains(entry.actor)) {
                    g_pendingSelection.push_back(entry.actor);
                }
            }
            ++rowIndex;
        }
    }
}

static void UpdateSelectionOnRowClick(RE::Actor* actor, std::size_t rowIndex, bool applyFilter) {
    const bool wasSelected = PendingSelectionContains(actor);
    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    const bool ctrlHeld = io && (io->KeyCtrl || io->KeySuper);
    const bool shiftHeld = io && io->KeyShift;

    if (shiftHeld) {
        std::size_t anchorIndex = rowIndex;
        if (g_selectionAnchor) {
            const std::size_t foundIndex = FindVisibleActorIndex(g_selectionAnchor, applyFilter);
            if (foundIndex != static_cast<std::size_t>(-1)) {
                anchorIndex = foundIndex;
            }
        }
        SelectVisibleActorRange(anchorIndex, rowIndex, ctrlHeld, applyFilter);
        return;
    }

    if (ctrlHeld) {
        if (wasSelected) {
            PendingSelectionRemove(actor);
        } else {
            g_pendingSelection.push_back(actor);
        }
        return;
    }

    g_pendingSelection.clear();
    g_pendingSelection.push_back(actor);
    g_selectionAnchor = actor;
}

static void RenderActorSelectorTableRow(
    const ActorTableRow& row, RE::Actor* actor, std::size_t rowIndex, bool applyFilter) {
    const bool selected = PendingSelectionContains(actor);

    ImGuiMCP::TableNextRow();
    ImGuiMCP::PushID(static_cast<int>(actor->GetFormID()));

    ImGuiMCP::TableSetColumnIndex(0);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.name));
    ImGuiMCP::TableSetColumnIndex(1);
    ImGuiMCP::Text("%08X", actor->GetFormID());
    ImGuiMCP::TableSetColumnIndex(2);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.type));
    ImGuiMCP::TableSetColumnIndex(3);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.weapon));
    ImGuiMCP::TableSetColumnIndex(4);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.age));
    ImGuiMCP::TableSetColumnIndex(5);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.status));
    ImGuiMCP::TableSetColumnIndex(6);
    ImGuiMCP::TextUnformatted(ActorTableRowTextOrEmpty(row.location));
    ImGuiMCP::TableSetColumnIndex(7);
    ImGuiMCP::Text("%.2f", row.distance);

    ImGuiMCP::TableSetColumnIndex(0);
    constexpr ImGuiMCP::ImGuiSelectableFlags selectableFlags =
        ImGuiMCP::ImGuiSelectableFlags_SpanAllColumns | ImGuiMCP::ImGuiSelectableFlags_AllowOverlap |
        ImGuiMCP::ImGuiSelectableFlags_AllowDoubleClick |
        static_cast<ImGuiMCP::ImGuiSelectableFlags>(ImGuiMCP::ImGuiSelectableFlags_NoSetKeyOwner);
    if (ImGuiMCP::Selectable("##ActorSelectorRow", selected, selectableFlags)) {
        if (ImGuiMCP::IsMouseDoubleClicked(ImGuiMCP::ImGuiMouseButton_Left)) {
            g_confirmedActors.clear();
            g_confirmedActors.push_back(actor);
            g_hasConfirmedSelection = true;
            g_closeOnDoubleClick = true;
        } else {
            const bool enterActivatingRow = ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false) ||
                ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_KeypadEnter, false);
            if (!enterActivatingRow) {
                UpdateSelectionOnRowClick(actor, rowIndex, applyFilter);
            }
        }
    }
    if (ImGuiMCP::IsItemFocused()) {
        g_navFocusedActor = actor;
    }

    ImGuiMCP::PopID();
}

void UI::ActorSelector::Open(const char* initialFilter, bool force) {
    g_hasConfirmedSelection = false;
    g_confirmedActors.clear();
    g_pendingSelection.clear();
    g_selectionAnchor = nullptr;
    g_focusFilterOnOpen = true;
    if (initialFilter && initialFilter[0] != '\0' && (force || g_filterBuffer[0] == '\0')) {
        strncpy_s(g_filterBuffer, initialFilter, sizeof(g_filterBuffer) - 1);
        g_filterBuffer[sizeof(g_filterBuffer) - 1] = '\0';
        g_lastTokenizedFilter[0] = '\0';
    }
    if (g_lastReferenceSearchRadius < 0.0f) {
        g_lastReferenceSearchRadius = kDefaultReferenceSearchRadius;
    }
    RebuildActorSelectorCache(g_lastReferenceSearchRadius);
    if (Window) {
        Window->IsOpen = true;
    }
}

bool UI::ActorSelector::ConsumeSelectedActors(std::vector<RE::Actor*>& outActors) {
    if (!g_hasConfirmedSelection) {
        outActors.clear();
        return false;
    }

    outActors = g_confirmedActors;
    g_confirmedActors.clear();
    g_hasConfirmedSelection = false;
    return true;
}

void __stdcall UI::ActorSelector::Render() {
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
    ImGuiMCP::Begin("Actor selector##SkyrimSlavery", nullptr, 0);

    g_closeOnDoubleClick = false;

    ImGuiMCP::ImGuiIO* io = ImGuiMCP::GetIO();
    const bool cancelShortcut = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper &&
        ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Q, false);
    const bool escPressed = ImGuiMCP::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false);

    if (g_cachedActors.empty()) {
        ImGuiMCP::Text("No actors found");
        if (ImGuiMCP::Button("Cancel") || escPressed || cancelShortcut) {
            g_hasConfirmedSelection = false;
            g_confirmedActors.clear();
            if (Window) {
                Window->IsOpen = false;
            }
            g_cachedActors.clear();
        }
        ImGuiMCP::End();
        return;
    }

    constexpr ImGuiMCP::ImGuiInputFlags kFilterShortcutRoute =
        ImGuiMCP::ImGuiInputFlags_RouteFocused | ImGuiMCP::ImGuiInputFlags_RouteOverActive;
    const bool focusFilterShortcut =
        ImGuiMCP::Shortcut(ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_L, kFilterShortcutRoute);

    if (g_focusFilterOnOpen || focusFilterShortcut) {
        ImGuiMCP::SetKeyboardFocusHere();
        g_focusFilterOnOpen = false;
    }
    if (ImGuiMCP::Shortcut(ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_D, kFilterShortcutRoute)) {
        FilterToggleTrailingDistance(g_filterBuffer, sizeof(g_filterBuffer));
    }
    ImGuiMCP::SetNextItemWidth(-1.0f);
    ImGuiMCP::InputTextWithHint("##ActorSelectorFilter", "Filter...", g_filterBuffer, sizeof(g_filterBuffer));
    ImGuiMCP::TextDisabled("Ctrl+L: focus filter | Ctrl+D: distance toggle | Ctrl+Enter: select all visible | Ctrl+Q: cancel");
    if (std::strcmp(g_filterBuffer, g_lastTokenizedFilter) != 0) {
        strncpy_s(g_lastTokenizedFilter, g_filterBuffer, sizeof(g_lastTokenizedFilter));
        g_lastTokenizedFilter[sizeof(g_lastTokenizedFilter) - 1] = '\0';
        FilterTokenize(g_filterBuffer, g_tokenizeResult);
        FilterParseExpression(g_filterBuffer, g_tokenizeResult, g_parseResult, kActorSelectorFilterSchema);

        const float referenceSearchRadius = FilterGetLessUpperBound(
            g_parseResult,
            static_cast<std::uint8_t>(ActorSelectorFilterField::Distance),
            kDefaultReferenceSearchRadius);
        if (referenceSearchRadius != g_lastReferenceSearchRadius) {
            g_lastReferenceSearchRadius = referenceSearchRadius;
            RebuildActorSelectorCache(referenceSearchRadius);
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
    const ImGuiMCP::ImGuiStyle* style = ImGuiMCP::GetStyle();
    const float footerHeight = ImGuiMCP::GetFrameHeight() + style->ItemSpacing.y;
    const float tableHeight = std::max(0.0f, tableAvail.y - footerHeight);

    bool closeWindow = false;
    std::size_t visibleRowCount = 0;
    RE::Actor* singleVisibleActor = nullptr;

    if (ImGuiMCP::BeginChild("##ActorSelectorTable", ImGuiMCP::ImVec2{0.0f, tableHeight}, 0)) {
        constexpr ImGuiMCP::ImGuiTableFlags tableFlags = ImGuiMCP::ImGuiTableFlags_Resizable |
                                                         ImGuiMCP::ImGuiTableFlags_RowBg |
                                                         ImGuiMCP::ImGuiTableFlags_SizingFixedFit |
                                                         ImGuiMCP::ImGuiTableFlags_ScrollY;
        if (ImGuiMCP::BeginTable("ActorSelectorTable", 8, tableFlags)) {
            ImGuiMCP::TableSetupColumn("Name", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 300.0f);
            ImGuiMCP::TableSetupColumn("ID");
            ImGuiMCP::TableSetupColumn("Type");
            ImGuiMCP::TableSetupColumn("Weapon");
            ImGuiMCP::TableSetupColumn("Age");
            ImGuiMCP::TableSetupColumn("Status");
            ImGuiMCP::TableSetupColumn("Location");
            ImGuiMCP::TableSetupColumn("Distance");
            ImGuiMCP::TableSetupScrollFreeze(0, 1);
            ImGuiMCP::TableHeadersRow();

            g_navFocusedActor = nullptr;
            std::size_t rowIndex = 0;
            for (const ActorSelectorCachedEntry& entry : g_cachedActors) {
                if (!applyFilter || CachedEntryMatchesFilter(entry, g_parseResult)) {
                    ++visibleRowCount;
                    singleVisibleActor = entry.actor;
                    RenderActorSelectorTableRow(entry.row, entry.actor, rowIndex, applyFilter);
                    ++rowIndex;
                }
            }

            ImGuiMCP::EndTable();
        }
        ImGuiMCP::EndChild();
    }

    const bool okButtonPressed = ImGuiMCP::Button("OK");
    ImGuiMCP::SameLine();
    const bool cancelButtonPressed = ImGuiMCP::Button("Cancel");

    constexpr ImGuiMCP::ImGuiInputFlags kConfirmInputRoute = ImGuiMCP::ImGuiInputFlags_RouteGlobal;
    const bool ctrlHeld = io && io->KeyCtrl && !io->KeyAlt && !io->KeySuper;
    const bool confirmAllVisiblePressed =
        ImGuiMCP::Shortcut(ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_Enter, kConfirmInputRoute) ||
        ImGuiMCP::Shortcut(ImGuiMCP::ImGuiMod_Ctrl | ImGuiMCP::ImGuiKey_KeypadEnter, kConfirmInputRoute) ||
        (okButtonPressed && ctrlHeld);
    const bool confirmPressed = !confirmAllVisiblePressed && !ctrlHeld &&
        (ImGuiMCP::Shortcut(ImGuiMCP::ImGuiKey_Enter, kConfirmInputRoute) ||
            ImGuiMCP::Shortcut(ImGuiMCP::ImGuiKey_KeypadEnter, kConfirmInputRoute) ||
            okButtonPressed);
    const bool cancelPressed = escPressed || cancelShortcut || cancelButtonPressed;

    if (confirmAllVisiblePressed && ActorSelectorFilterIsNonEmpty()) {
        CollectVisibleActors(g_confirmedActors, applyFilter);
        g_hasConfirmedSelection = true;
        closeWindow = true;
    } else if (confirmPressed) {
        if (!g_pendingSelection.empty()) {
            g_confirmedActors.assign(g_pendingSelection.begin(), g_pendingSelection.end());
        } else if (applyFilter && visibleRowCount == 1 && singleVisibleActor) {
            g_confirmedActors.clear();
            g_confirmedActors.push_back(singleVisibleActor);
        } else if (g_navFocusedActor) {
            g_confirmedActors.clear();
            g_confirmedActors.push_back(g_navFocusedActor);
        } else {
            g_confirmedActors.clear();
        }
        g_hasConfirmedSelection = true;
        closeWindow = true;
    } else if (cancelPressed) {
        g_hasConfirmedSelection = false;
        g_confirmedActors.clear();
        closeWindow = true;
    }

    if ((closeWindow || g_closeOnDoubleClick) && Window) {
        Window->IsOpen = false;
        g_cachedActors.clear();
    }

    ImGuiMCP::End();
}
